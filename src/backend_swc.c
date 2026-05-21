#include "backend_swc.h"

#include "bind.h"
#include "config.h"
#include "dispatch.h"
#include "ipc.h"
#include "log.h"
#include "rules.h"
#include "server.h"

#include <swc.h>
#include <wayland-server-core.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

int putenv(char *string);
int setenv(const char *name, const char *value, int overwrite);

static void
ensure_xdg_runtime_dir(void)
{
	char buf[256];
	const char *v = getenv("XDG_RUNTIME_DIR");

	if (v != NULL && v[0] != '\0' && access(v, W_OK) == 0)
		return;

	snprintf(buf, sizeof(buf), "/run/user/%d", (int)getuid());
	if (access(buf, W_OK) == 0) {
		setenv("XDG_RUNTIME_DIR", buf, 1);
		inis_info("set XDG_RUNTIME_DIR=%s", buf);
		return;
	}

	snprintf(buf, sizeof(buf), "/tmp/runtime-%d", (int)getuid());
	mkdir(buf, 0700);
	setenv("XDG_RUNTIME_DIR", buf, 1);
	inis_info("set XDG_RUNTIME_DIR=%s (fallback)", buf);
}

struct swc_window_data {
	struct inis_window *window;
};

static void swc_new_screen(struct swc_screen *screen);
static void swc_new_window(struct swc_window *window);
static void swc_new_device(struct libinput_device *device);
static void swc_activate(void);
static void swc_deactivate(void);
static void swc_window_destroy(void *data);
static void swc_window_title_changed(void *data);
static void swc_window_app_id_changed(void *data);
static void swc_window_parent_changed(void *data);
static void swc_window_entered(void *data);
static void swc_window_move(void *data);
static void swc_window_resize(void *data);
static int swc_signal(int signal_number, void *data);
static int swc_ipc(int fd, uint32_t mask, void *data);
static void swc_binding(void *data, uint32_t time, uint32_t value,
    uint32_t state);
static void install_bindings(struct inis_backend *backend);

static struct inis_backend *active_backend;

static const struct swc_manager manager = {
	.new_screen = swc_new_screen,
	.new_window = swc_new_window,
	.new_device = swc_new_device,
	.activate   = swc_activate,
	.deactivate = swc_deactivate,
};

static const struct swc_window_handler window_handler = {
	.destroy        = swc_window_destroy,
	.title_changed  = swc_window_title_changed,
	.app_id_changed = swc_window_app_id_changed,
	.parent_changed = swc_window_parent_changed,
	.entered        = swc_window_entered,
	.move           = swc_window_move,
	.resize         = swc_window_resize,
};

static struct inis_rect
rect_from_swc(const struct swc_rectangle *rect)
{
	struct inis_rect out;

	out.x = rect->x;
	out.y = rect->y;
	out.w = (int)rect->width;
	out.h = (int)rect->height;
	return out;
}

static void
swc_new_screen(struct swc_screen *screen)
{
	char name[INIS_MAX_NAME];
	struct inis_rect geometry;
	struct inis_rect usable;
	struct inis_server *server;

	inis_info("swc screen: %dx%d at %d,%d usable %dx%d at %d,%d",
	    (int)screen->geometry.width, (int)screen->geometry.height,
	    (int)screen->geometry.x, (int)screen->geometry.y,
	    (int)screen->usable_geometry.width,
	    (int)screen->usable_geometry.height,
	    (int)screen->usable_geometry.x, (int)screen->usable_geometry.y);

	if (active_backend == NULL || active_backend->server == NULL)
		return;

	server = active_backend->server;
	snprintf(name, sizeof(name), "swc-%zu", server->monitor_count);
	geometry = rect_from_swc(&screen->geometry);
	usable   = rect_from_swc(&screen->usable_geometry);
	inis_server_add_monitor(server, name, &geometry, &usable, screen);
}

static void
swc_new_window(struct swc_window *window)
{
	struct swc_window_data *data;
	struct inis_window *inis_window;

	if (active_backend == NULL || active_backend->server == NULL)
		return;

	data = calloc(1, sizeof(*data));
	if (data == NULL) {
		inis_warn("failed to allocate swc window data");
		return;
	}

	/*
	 * Set handler and tiled mode BEFORE add_window so that when arrange()
	 * calls swc_window_set_geometry inside add_window, swc already knows
	 * the window is tiled and applies the geometry immediately. Without
	 * this ordering, the window appears at an undefined position and then
	 * snaps — leaving a visible gap in the tiled layout.
	 */
	swc_window_set_handler(window, &window_handler, data);
	swc_window_set_tiled(window);

	inis_window = inis_server_add_window(active_backend->server,
	    window->app_id, window->title, window);
	if (inis_window == NULL) {
		free(data);
		return;
	}
	data->window = inis_window;

	/* Show after geometry is applied — window appears at final position. */
	swc_window_show(window);
}

static void
swc_new_device(struct libinput_device *device)
{
	(void)device;
	inis_debug("swc input device detected");
}

static void
swc_activate(void)
{
	inis_info("swc session activated");
}

static void
swc_deactivate(void)
{
	inis_info("swc session deactivated");
}

static void
swc_window_destroy(void *data)
{
	struct swc_window_data *window_data = data;

	if (window_data == NULL || window_data->window == NULL)
		return;
	if (active_backend != NULL && active_backend->server != NULL)
		inis_server_remove_window(active_backend->server,
		    window_data->window);
	free(window_data);
}

static void
swc_window_title_changed(void *data)
{
	struct swc_window_data *window_data = data;
	struct swc_window *swc;

	if (window_data == NULL || window_data->window == NULL)
		return;
	swc = window_data->window->swc;
	if (swc != NULL)
		snprintf(window_data->window->title,
		    sizeof(window_data->window->title), "%s",
		    swc->title != NULL ? swc->title : "");
}

static void
swc_window_app_id_changed(void *data)
{
	struct swc_window_data *window_data = data;
	struct swc_window *swc;

	if (window_data == NULL || window_data->window == NULL)
		return;
	swc = window_data->window->swc;
	if (swc != NULL)
		snprintf(window_data->window->app_id,
		    sizeof(window_data->window->app_id), "%s",
		    swc->app_id != NULL ? swc->app_id : "");
}

static void
swc_window_parent_changed(void *data)
{
	(void)data;
	inis_debug("swc window parent changed");
}

static void
swc_window_entered(void *data)
{
	struct swc_window_data *window_data = data;

	if (window_data == NULL || window_data->window == NULL ||
	    active_backend == NULL || active_backend->server == NULL)
		return;
	inis_server_focus_window(active_backend->server, window_data->window);
}

static void
swc_window_move(void *data)
{
	struct swc_window_data *window_data = data;
	struct inis_server *server;

	if (window_data == NULL || window_data->window == NULL ||
	    active_backend == NULL || active_backend->server == NULL)
		return;

	server = active_backend->server;
	(void)inis_server_focus_window(server, window_data->window);
	if (inis_server_begin_mouse_move(server) != 0)
		inis_debug("swc interactive move ignored");
}

static void
swc_window_resize(void *data)
{
	struct swc_window_data *window_data = data;
	struct inis_server *server;

	if (window_data == NULL || window_data->window == NULL ||
	    active_backend == NULL || active_backend->server == NULL)
		return;

	server = active_backend->server;
	(void)inis_server_focus_window(server, window_data->window);
	if (inis_server_begin_mouse_resize(server, INIS_WINDOW_EDGE_AUTO) != 0)
		inis_debug("swc interactive resize ignored");
}

static int
swc_signal(int signal_number, void *data)
{
	struct inis_backend *backend = data;

	inis_info("signal %d received, stopping compositor", signal_number);
	if (backend->server != NULL)
		inis_server_request_exit(backend->server);
	if (backend->display != NULL)
		wl_display_terminate(backend->display);
	return 0;
}

static int
swc_ipc(int fd, uint32_t mask, void *data)
{
	struct inis_backend *backend = data;

	(void)fd;
	if ((mask & WL_EVENT_READABLE) == 0)
		return 0;
	if (backend->server != NULL)
		inis_ipc_accept(backend->server);
	return 0;
}

static void
swc_binding(void *data, uint32_t time, uint32_t value, uint32_t state)
{
	struct inis_backend_binding *binding = data;

	(void)time;
	(void)value;
	if (state == 0)
		return;
	inis_dispatch(binding->server, binding->dispatcher, binding->args);
}

static void
install_bindings(struct inis_backend *backend)
{
	struct inis_server *server = backend->server;
	size_t i;

	if (server == NULL)
		return;

	backend->backend_binding_count = 0;
	for (i = 0; i < server->binding_count; i++) {
		struct inis_binding *binding = &server->bindings[i];
		struct inis_backend_binding *bb;
		enum swc_binding_type type;
		unsigned int value;
		unsigned int mods;

		if (inis_bind_numeric_value(binding, &value) != 0) {
			inis_warn("skipping unsupported binding key: %s",
			    binding->key);
			continue;
		}
		if (backend->backend_binding_count >= INIS_MAX_BINDINGS)
			break;

		bb = &backend->backend_bindings[
		    backend->backend_binding_count++];
		bb->server = server;
		snprintf(bb->dispatcher, sizeof(bb->dispatcher), "%s",
		    binding->dispatcher);
		snprintf(bb->args, sizeof(bb->args), "%s", binding->args);

		type = binding->mouse ? SWC_BINDING_BUTTON : SWC_BINDING_KEY;
		mods = inis_bind_modifier_mask(binding);
		bb->swc_type  = (uint32_t)type;
		bb->swc_mods  = mods;
		bb->swc_value = value;
		if (swc_add_binding(type, mods, value, swc_binding, bb) != 0)
			inis_warn("failed to register binding: %s %s",
			    binding->mods, binding->key);
		else
			inis_debug("registered binding: %s %s -> %s %s",
			    binding->mods, binding->key,
			    binding->dispatcher, binding->args);
	}
}

int
inis_backend_swc_start(struct inis_backend *backend)
{
	struct wl_event_loop *loop;

	ensure_xdg_runtime_dir();
	active_backend = backend;
	backend->display = wl_display_create();
	if (backend->display == NULL) {
		inis_error("failed to create Wayland display");
		active_backend = NULL;
		return -1;
	}

	backend->wayland_socket = wl_display_add_socket_auto(backend->display);
	if (backend->wayland_socket == NULL) {
		inis_error("failed to create Wayland socket");
		wl_display_destroy(backend->display);
		backend->display = NULL;
		active_backend = NULL;
		return -1;
	}
	snprintf(backend->wayland_env, sizeof(backend->wayland_env),
	    "WAYLAND_DISPLAY=%s", backend->wayland_socket);
	putenv(backend->wayland_env);
	inis_info("Wayland socket: %s", backend->wayland_socket);

	if (!swc_initialize(backend->display, NULL, &manager)) {
		inis_error("swc_initialize failed");
		wl_display_destroy(backend->display);
		backend->display = NULL;
		backend->wayland_socket = NULL;
		active_backend = NULL;
		return -1;
	}

	loop = wl_display_get_event_loop(backend->display);
	backend->sigint_source  = wl_event_loop_add_signal(loop, SIGINT,
	    swc_signal, backend);
	backend->sigterm_source = wl_event_loop_add_signal(loop, SIGTERM,
	    swc_signal, backend);
	if (backend->sigint_source == NULL || backend->sigterm_source == NULL) {
		inis_error("failed to install signal handlers");
		swc_finalize();
		wl_display_destroy(backend->display);
		backend->display = NULL;
		backend->wayland_socket = NULL;
		active_backend = NULL;
		return -1;
	}
	if (backend->server != NULL &&
	    inis_ipc_get_fd(&backend->server->ipc) >= 0) {
		backend->ipc_source = wl_event_loop_add_fd(loop,
		    inis_ipc_get_fd(&backend->server->ipc),
		    WL_EVENT_READABLE, swc_ipc, backend);
		if (backend->ipc_source == NULL)
			inis_warn("failed to add IPC fd to event loop");
	}

	backend->started = true;
	install_bindings(backend);
	inis_info("swc backend initialized");
	return 0;
}

int
inis_backend_swc_run(struct inis_backend *backend)
{
	if (backend->display == NULL)
		return -1;
	inis_info("entering swc event loop");
	wl_display_run(backend->display);
	inis_info("left swc event loop");
	return 0;
}

void
inis_backend_swc_request_stop(struct inis_backend *backend)
{
	if (backend->display != NULL)
		wl_display_terminate(backend->display);
}

void
inis_backend_swc_finish(struct inis_backend *backend)
{
	if (backend->sigint_source != NULL) {
		wl_event_source_remove(backend->sigint_source);
		backend->sigint_source = NULL;
	}
	if (backend->sigterm_source != NULL) {
		wl_event_source_remove(backend->sigterm_source);
		backend->sigterm_source = NULL;
	}
	if (backend->ipc_source != NULL) {
		wl_event_source_remove(backend->ipc_source);
		backend->ipc_source = NULL;
	}
	swc_finalize();
	active_backend = NULL;
	if (backend->display != NULL) {
		wl_display_destroy(backend->display);
		backend->display = NULL;
	}
	backend->wayland_socket = NULL;
}

void
inis_backend_swc_close_window(struct inis_backend *backend,
    struct inis_window *window)
{
	(void)backend;
	if (window->swc != NULL)
		swc_window_close(window->swc);
}

void
inis_backend_swc_set_window_visible(struct inis_backend *backend,
    struct inis_window *window, bool visible)
{
	(void)backend;
	if (window->swc == NULL)
		return;
	if (visible)
		swc_window_show(window->swc);
	else
		swc_window_hide(window->swc);
}

void
inis_backend_swc_raise_window(struct inis_backend *backend,
    struct inis_window *window)
{
	size_t n;

	if (window == NULL || window->swc == NULL)
		return;

	/*
	 * swc_window_stack(w, -1) moves w one step towards the front.
	 * Calling it window_count times guarantees w reaches the top
	 * regardless of its starting position.
	 */
	n = backend->server != NULL ? backend->server->window_count : 1;
	while (n-- > 0)
		swc_window_stack(window->swc, -1);
}

int
inis_backend_swc_sync_window_geometry(struct inis_backend *backend,
    struct inis_window *window)
{
	struct swc_rectangle geometry;

	(void)backend;
	if (window == NULL || window->swc == NULL)
		return -1;
	if (!swc_window_get_geometry(window->swc, &geometry))
		return -1;
	if (geometry.width == 0 || geometry.height == 0)
		return -1;

	window->floating = rect_from_swc(&geometry);
	return 0;
}

void
inis_backend_swc_focus_window(struct inis_backend *backend,
    struct inis_window *window)
{
	(void)backend;
	if (window != NULL && window->swc != NULL)
		swc_window_focus(window->swc);
	else
		swc_window_focus(NULL);
}

void
inis_backend_swc_apply_window(struct inis_backend *backend,
    struct inis_window *window)
{
	struct inis_rect *rect;

	if (window->swc == NULL)
		return;

	swc_window_set_border(window->swc,
	    window->focused
	        ? INIS_COLOR_FOCUSED_BORDER
	        : INIS_COLOR_NORMAL_BORDER,
	    window->no_border
	        ? 0
	        : (uint32_t)backend->server->config.border_size,
	    INIS_COLOR_NORMAL_BORDER, 0);

	if (window->state == INIS_WINDOW_FULLSCREEN) {
		if (backend->server != NULL &&
		    window->monitor_index < backend->server->monitor_count)
			swc_window_set_fullscreen(window->swc,
			    backend->server->monitors[
			        window->monitor_index].swc);
		return;
	}

	if (window->state == INIS_WINDOW_FLOATING) {
		rect = &window->floating;
		swc_window_set_stacked(window->swc);
	} else {
		rect = &window->tiled;
		swc_window_set_tiled(window->swc);
	}

	swc_window_set_geometry(window->swc,
	    &(struct swc_rectangle){
	        .x      = rect->x,
	        .y      = rect->y,
	        .width  = (uint32_t)rect->w,
	        .height = (uint32_t)rect->h,
	    });
}

void
inis_backend_swc_begin_move(struct inis_backend *backend,
    struct inis_window *window)
{
	(void)backend;
	if (window->swc != NULL) {
		swc_window_end_resize(window->swc);
		swc_window_end_move(window->swc);
		swc_window_begin_move(window->swc);
	}
}

void
inis_backend_swc_begin_resize(struct inis_backend *backend,
    struct inis_window *window, uint32_t edges)
{
	(void)backend;
	if (window->swc != NULL) {
		swc_window_end_move(window->swc);
		swc_window_end_resize(window->swc);
		swc_window_begin_resize(window->swc, edges);
	}
}

static void
uninstall_bindings(struct inis_backend *backend)
{
	size_t i;

	for (i = 0; i < backend->backend_binding_count; i++) {
		struct inis_backend_binding *bb = &backend->backend_bindings[i];

		swc_remove_binding((enum swc_binding_type)bb->swc_type,
		    bb->swc_mods, bb->swc_value);
	}
	backend->backend_binding_count = 0;
}

void
inis_backend_swc_reload_bindings(struct inis_backend *backend)
{
	struct inis_server *server = backend->server;

	if (server == NULL || !backend->started)
		return;

	uninstall_bindings(backend);

	server->binding_count = 0;
	server->rule_count = 0;
	inis_config_init(&server->config);

	{
		const char *home = getenv("HOME");
		char path[512];

		if (home != NULL && home[0] != '\0') {
			snprintf(path, sizeof(path),
			    "%s/.config/inis/inis.conf", home);
			if (inis_config_load_file(&server->config,
			    server->bindings, &server->binding_count,
			    server->rules, &server->rule_count, path) == 0)
				inis_info("config reloaded: %s", path);
			else
				inis_warn("config reload failed or not found: %s",
				    path);
		}
	}

	install_bindings(backend);
	inis_info("bindings reinstalled: %zu", backend->backend_binding_count);
}
