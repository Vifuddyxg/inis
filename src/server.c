#include "server.h"

#include "dispatch.h"
#include "layout.h"
#include "log.h"
#include "render.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
load_user_config(struct inis_server *server)
{
	const char *home;
	char path[512];
	FILE *probe;

	home = getenv("HOME");
	if (home == NULL || home[0] == '\0')
		return;

	snprintf(path, sizeof(path), "%s/.config/inis/inis.conf", home);

	probe = fopen(path, "r");
	if (probe == NULL)
		return;
	fclose(probe);

	/*
	 * User config exists — clear default bindings so the config file
	 * has full ownership. Without this, duplicate keys register twice
	 * and the default always wins (it was registered first).
	 */
	server->binding_count = 0;
	server->rule_count = 0;

	if (inis_config_load_file(&server->config, server->bindings,
	    &server->binding_count, server->rules, &server->rule_count, path) == 0) {
		inis_info("loaded config: %s", path);
		inis_info("loaded %zu bindings and %zu window rules",
		    server->binding_count, server->rule_count);
	}
}

static void
add_default_bind(struct inis_server *server, const char *line)
{
	if (server->binding_count >= INIS_MAX_BINDINGS)
		return;
	if (inis_bind_parse_line(&server->bindings[server->binding_count], line) == 0)
		server->binding_count++;
	else
		inis_warn("invalid default binding: %s", line);
}

static void
load_default_binds(struct inis_server *server)
{
	add_default_bind(server, "bind $mainMod Q exec kitty");
	add_default_bind(server, "bind $mainMod C killactive");
	add_default_bind(server, "bind $mainMod M shutdownmenu");
	add_default_bind(server, "bind $mainMod E exec dolphin");
	add_default_bind(server, "bind $mainMod V togglefloating");
	add_default_bind(server, "bind $mainMod R exec hyprlauncher");
	add_default_bind(server, "bind $mainMod P pseudo");
	add_default_bind(server, "bind $mainMod J togglesplit");
	add_default_bind(server, "bind $mainMod S togglespecialworkspace magic");
	add_default_bind(server, "bind $mainMod Left movefocus left");
	add_default_bind(server, "bind $mainMod Down movefocus down");
	add_default_bind(server, "bind $mainMod Up movefocus up");
	add_default_bind(server, "bind $mainMod Right movefocus right");
	add_default_bind(server, "bind $mainMod 1 workspace 1");
	add_default_bind(server, "bind $mainMod 2 workspace 2");
	add_default_bind(server, "bind $mainMod 3 workspace 3");
	add_default_bind(server, "bind $mainMod 4 workspace 4");
	add_default_bind(server, "bind $mainMod 5 workspace 5");
	add_default_bind(server, "bind $mainMod 6 workspace 6");
	add_default_bind(server, "bind $mainMod 7 workspace 7");
	add_default_bind(server, "bind $mainMod 8 workspace 8");
	add_default_bind(server, "bind $mainMod 9 workspace 9");
	add_default_bind(server, "bind $mainMod 0 workspace 10");
	add_default_bind(server, "bind $mainMod SHIFT 1 movetoworkspace 1");
	add_default_bind(server, "bind $mainMod SHIFT 2 movetoworkspace 2");
	add_default_bind(server, "bind $mainMod SHIFT 3 movetoworkspace 3");
	add_default_bind(server, "bind $mainMod SHIFT 4 movetoworkspace 4");
	add_default_bind(server, "bind $mainMod SHIFT 5 movetoworkspace 5");
	add_default_bind(server, "bind $mainMod SHIFT 6 movetoworkspace 6");
	add_default_bind(server, "bind $mainMod SHIFT 7 movetoworkspace 7");
	add_default_bind(server, "bind $mainMod SHIFT 8 movetoworkspace 8");
	add_default_bind(server, "bind $mainMod SHIFT 9 movetoworkspace 9");
	add_default_bind(server, "bind $mainMod SHIFT 0 movetoworkspace 10");
	add_default_bind(server, "bind $mainMod SHIFT S movetoworkspace special:magic");
	add_default_bind(server, "bindm $mainMod mouse_down workspace e+1");
	add_default_bind(server, "bindm $mainMod mouse_up workspace e-1");
	add_default_bind(server, "bindm $mainMod mouse:272 movewindow");
	add_default_bind(server, "bindm $mainMod mouse:273 resizewindow");
}

static void
ignore_sigchld(void)
{
	signal(SIGCHLD, SIG_IGN);
}

static void
run_startup_commands(struct inis_server *server)
{
	size_t i;

	for (i = 0; i < server->config.startup_command_count; i++) {
		inis_dispatch(server, "exec", server->config.startup_commands[i]);
		inis_debug("exec-once started: %s",
		    server->config.startup_commands[i]);
	}
}

static int
workspace_find(struct inis_server *server, const char *name)
{
	size_t i;

	for (i = 0; i < server->workspace_count; i++) {
		if (strcmp(server->workspaces[i].name, name) == 0)
			return (int)i;
	}
	return -1;
}

static int
workspace_get_or_create(struct inis_server *server, const char *name, bool special)
{
	int index;

	index = workspace_find(server, name);
	if (index >= 0)
		return index;

	if (server->workspace_count >= INIS_MAX_WORKSPACES) {
		inis_warn("workspace ignored: limit reached");
		return -1;
	}

	index = (int)server->workspace_count++;
	inis_workspace_init(&server->workspaces[index], name, special);
	return index;
}

static unsigned int
server_active_workspace(const struct inis_server *server)
{
	if (server->monitor_count == 0)
		return 0;
	return server->monitors[0].active_workspace;
}

static bool
window_is_focusable(const struct inis_server *server,
    const struct inis_window *window)
{
	const struct inis_workspace *workspace;

	if (window == NULL || !window->mapped)
		return false;
	if (window->workspace_index >= server->workspace_count)
		return false;
	workspace = &server->workspaces[window->workspace_index];
	return window->workspace_index == server_active_workspace(server) ||
	    (workspace->special && workspace->visible);
}

static int
min_int(int a, int b)
{
	return a < b ? a : b;
}

static void
window_default_floating(struct inis_server *server, struct inis_window *window)
{
	struct inis_rect area = { 0, 0, 800, 600 };
	int w;
	int h;

	if (server->monitor_count > window->monitor_index)
		area = server->monitors[window->monitor_index].usable;

	w = min_int(800, area.w > 0 ? area.w : 800);
	h = min_int(600, area.h > 0 ? area.h : 600);
	window->floating.w = w;
	window->floating.h = h;
	window->floating.x = area.x + (area.w - w) / 2;
	window->floating.y = area.y + (area.h - h) / 2;
}

static void
window_center(struct inis_server *server, struct inis_window *window)
{
	struct inis_rect area = { 0, 0, 800, 600 };

	if (server->monitor_count > window->monitor_index)
		area = server->monitors[window->monitor_index].usable;
	if (window->floating.w <= 0 || window->floating.h <= 0)
		window_default_floating(server, window);
	window->floating.x = area.x + (area.w - window->floating.w) / 2;
	window->floating.y = area.y + (area.h - window->floating.h) / 2;
}

static struct inis_rect
window_current_rect(const struct inis_window *window)
{
	if (window->state == INIS_WINDOW_FLOATING ||
	    window->state == INIS_WINDOW_FULLSCREEN)
		return window->floating;
	return window->tiled;
}

static void
damage_window(struct inis_server *server, const struct inis_window *window,
    const char *reason)
{
	struct inis_rect rect;

	if (window == NULL)
		return;
	if (window->state == INIS_WINDOW_FLOATING &&
	    window->sync_geometry_from_backend) {
		(void)inis_backend_sync_window_geometry(&server->backend,
		    (struct inis_window *)window);
	}
	rect = window_current_rect(window);
	inis_server_mark_damage(server, &rect, reason);
}

static void
damage_monitor(struct inis_server *server, const struct inis_monitor *monitor,
    const char *reason)
{
	if (monitor == NULL)
		return;
	inis_server_mark_damage(server, &monitor->geometry, reason);
}

static void
mark_damage_on_outputs(struct inis_server *server, const struct inis_rect *rect,
    const char *reason)
{
	size_t i;

	for (i = 0; i < server->monitor_count; i++) {
		struct inis_monitor *mon = &server->monitors[i];
		struct inis_rect inter;
		int x1, y1, x2, y2;
		int mx2, my2;

		mx2 = mon->geometry.x + mon->geometry.w;
		my2 = mon->geometry.y + mon->geometry.h;
		x1 = rect->x > mon->geometry.x ? rect->x : mon->geometry.x;
		y1 = rect->y > mon->geometry.y ? rect->y : mon->geometry.y;
		x2 = (rect->x + rect->w) < mx2 ? (rect->x + rect->w) : mx2;
		y2 = (rect->y + rect->h) < my2 ? (rect->y + rect->h) : my2;

		if (x2 <= x1 || y2 <= y1)
			continue;

		inter.x = x1;
		inter.y = y1;
		inter.w = x2 - x1;
		inter.h = y2 - y1;
		inis_damage_add_rect(&mon->damage, &inter, reason);
	}
}

static void
window_make_directly_managed(struct inis_server *server,
    struct inis_window *window)
{
	if (window->state == INIS_WINDOW_FULLSCREEN)
		inis_window_set_fullscreen(window, false);
	if (window->state == INIS_WINDOW_TILED) {
		inis_window_set_floating(window, true);
		window_default_floating(server, window);
	}
}

static int
clamp_int(int value, int min, int max)
{
	if (value < min)
		return min;
	if (value > max)
		return max;
	return value;
}

static void
sort_windows_by_layout_order(struct inis_window **windows, size_t count)
{
	size_t i;

	for (i = 1; i < count; i++) {
		struct inis_window *window = windows[i];
		size_t j = i;

		while (j > 0 &&
		    windows[j - 1]->layout_order > window->layout_order) {
			windows[j] = windows[j - 1];
			j--;
		}
		windows[j] = window;
	}
}

static void
apply_window_rules(struct inis_server *server, struct inis_window *window)
{
	size_t i;

	for (i = 0; i < server->rule_count; i++) {
		struct inis_rule *rule = &server->rules[i];
		int a;
		int b;
		int workspace_index;

		if (!inis_rule_matches(rule, window))
			continue;

		switch (rule->action) {
		case INIS_RULE_FLOAT:
			inis_window_set_floating(window, true);
			window_default_floating(server, window);
			break;
		case INIS_RULE_TILE:
			inis_window_set_floating(window, false);
			break;
		case INIS_RULE_FULLSCREEN:
			inis_window_set_fullscreen(window, true);
			break;
		case INIS_RULE_CENTER:
			inis_window_set_floating(window, true);
			window_center(server, window);
			break;
		case INIS_RULE_WORKSPACE:
			workspace_index = workspace_get_or_create(server, rule->args,
			    strncmp(rule->args, "special:", 8) == 0);
			if (workspace_index >= 0)
				window->workspace_index = (unsigned int)workspace_index;
			break;
		case INIS_RULE_SIZE:
			if (sscanf(rule->args, "%d %d", &a, &b) == 2 && a > 0 && b > 0) {
				inis_window_set_floating(window, true);
				window->floating.w = a;
				window->floating.h = b;
			}
			break;
		case INIS_RULE_MOVE:
			if (sscanf(rule->args, "%d %d", &a, &b) == 2) {
				inis_window_set_floating(window, true);
				if (window->floating.w <= 0 || window->floating.h <= 0)
					window_default_floating(server, window);
				window->floating.x = a;
				window->floating.y = b;
			}
			break;
		case INIS_RULE_NOBORDER:
			window->no_border = true;
			break;
		case INIS_RULE_NOANIM:
			window->no_anim = true;
			break;
		case INIS_RULE_MONITOR:
			inis_debug("monitor rule is parsed but not implemented yet: %s", rule->args);
			break;
		}
	}
}

int
inis_server_init(struct inis_server *server)
{
	memset(server, 0, sizeof(*server));
	server->running = true;
	server->socket_name = "inis-0";

	inis_config_init(&server->config);
	server->next_window_order = 1;
	inis_backend_init(&server->backend, server);
	inis_damage_init(&server->damage);
	inis_ipc_init(&server->ipc, server->socket_name);
	load_default_binds(server);
	load_user_config(server);

	inis_workspace_init(&server->workspaces[0], "1", false);
	server->workspaces[0].active = true;
	server->workspace_count = 1;
	ignore_sigchld();
	inis_ipc_start(server);

	inis_info("phase 0 init complete");
	return 0;
}

int
inis_server_run(struct inis_server *server)
{
	if (inis_backend_start(&server->backend) != 0) {
		inis_error("backend init failed: swc_initialize returned false");
		inis_dispatch(server, "exit", "");
		return -1;
	}

	run_startup_commands(server);
	return inis_backend_run(&server->backend);
}

void
inis_server_shutdown(struct inis_server *server)
{
	inis_ipc_finish(&server->ipc);
	inis_backend_finish(&server->backend);
	inis_info("shutdown complete");
}

void
inis_server_request_exit(struct inis_server *server)
{
	server->running = false;
	inis_backend_request_stop(&server->backend);
}

struct inis_monitor *
inis_server_add_monitor(struct inis_server *server, const char *name,
    const struct inis_rect *geometry, const struct inis_rect *usable,
    void *backend_monitor)
{
	struct inis_monitor *monitor;

	if (server->monitor_count >= INIS_MAX_MONITORS) {
		inis_warn("monitor ignored: limit reached");
		return NULL;
	}

	monitor = &server->monitors[server->monitor_count++];
	inis_monitor_init(monitor, name);
	monitor->geometry = *geometry;
	monitor->usable = *usable;
	monitor->swc = backend_monitor;
	monitor->active_workspace = 0;
	if (server->focused_monitor == NULL)
		server->focused_monitor = monitor;

	inis_info("monitor added: %s %dx%d+%d+%d usable %dx%d+%d+%d",
	    monitor->name,
	    monitor->geometry.w, monitor->geometry.h,
	    monitor->geometry.x, monitor->geometry.y,
	    monitor->usable.w, monitor->usable.h,
	    monitor->usable.x, monitor->usable.y);
	damage_monitor(server, monitor, "monitor-add");
	inis_server_flush_damage(server, "monitor-add");
	return monitor;
}

struct inis_window *
inis_server_add_window(struct inis_server *server, const char *app_id,
    const char *title, void *backend_window)
{
	struct inis_window *window;
	size_t i;

	for (i = 0; i < server->window_count; i++) {
		if (!server->windows[i].mapped)
			break;
	}

	if (i == server->window_count && server->window_count >= INIS_MAX_WINDOWS) {
		inis_warn("window ignored: limit reached");
		return NULL;
	}

	if (i == server->window_count)
		server->window_count++;

	window = &server->windows[i];
	inis_window_init(window);
	snprintf(window->app_id, sizeof(window->app_id), "%s", app_id ? app_id : "");
	snprintf(window->title, sizeof(window->title), "%s", title ? title : "");
	window->workspace_index = 0;
	window->monitor_index = 0;
	window->layout_order = server->next_window_order++;
	window->mapped = true;
	window->swc = backend_window;

	if (server->focused_monitor != NULL)
		window->workspace_index = server->focused_monitor->active_workspace;
	apply_window_rules(server, window);
	if (window->workspace_index < server->workspace_count)
		server->workspaces[window->workspace_index].window_count++;

	inis_info("window added: app_id=%s title=%s",
	    window->app_id, window->title);
	damage_window(server, window, "window-add");
	inis_server_arrange(server);
	inis_server_focus_window(server, window);
	inis_server_flush_damage(server, "window-add");
	return window;
}

void
inis_server_remove_window(struct inis_server *server, struct inis_window *window)
{
	size_t i;

	if (window == NULL)
		return;

	i = (size_t)(window - server->windows);
	if (i >= server->window_count || !window->mapped)
		return;

	if (window->workspace_index < server->workspace_count &&
	    server->workspaces[window->workspace_index].window_count > 0)
		server->workspaces[window->workspace_index].window_count--;

	damage_window(server, window, "window-remove");
	if (server->focused_window == window)
		server->focused_window = NULL;

	window->mapped = false;
	window->focused = false;
	window->swc = NULL;

	if (server->focused_window == NULL) {
		for (i = server->window_count; i > 0; i--) {
			if (window_is_focusable(server, &server->windows[i - 1])) {
				inis_server_focus_window(server, &server->windows[i - 1]);
				break;
			}
		}
	}

	inis_server_arrange(server);
	inis_server_flush_damage(server, "window-remove");
}

void
inis_server_arrange(struct inis_server *server)
{
	struct inis_layout layout;
	struct inis_rect area;
	struct inis_rect rects[INIS_MAX_WINDOWS];
	struct inis_window *visible[INIS_MAX_WINDOWS];
	size_t count = 0;
	unsigned int active_workspace = 0;
	size_t i;

	if (server->monitor_count == 0)
		return;

	/* repaint entire output so removed/toggled windows leave no ghost */
	inis_damage_add_rect(&server->monitors[0].damage,
	    &server->monitors[0].geometry, "arrange");
	inis_damage_add_rect(&server->damage,
	    &server->monitors[0].geometry, "arrange");

	active_workspace = server->monitors[0].active_workspace;

	for (i = 0; i < server->window_count; i++) {
		struct inis_workspace *workspace;
		bool show;

		if (!server->windows[i].mapped)
			continue;
		if (server->windows[i].workspace_index >= server->workspace_count) {
			inis_backend_set_window_visible(&server->backend, &server->windows[i], false);
			continue;
		}
		workspace = &server->workspaces[server->windows[i].workspace_index];
		show = server->windows[i].workspace_index == active_workspace ||
		    (workspace->special && workspace->visible);
		if (!show) {
			inis_backend_set_window_visible(&server->backend, &server->windows[i], false);
			continue;
		}

		inis_backend_set_window_visible(&server->backend, &server->windows[i], true);
		if (server->windows[i].workspace_index != active_workspace ||
		    server->windows[i].state != INIS_WINDOW_TILED)
			continue;
		visible[count++] = &server->windows[i];
	}

	layout.master_ratio = server->config.master_ratio;
	layout.gaps_in = server->config.gaps_in;
	layout.gaps_out = server->config.gaps_out;
	area = server->monitors[0].usable;
	sort_windows_by_layout_order(visible, count);
	inis_layout_master(&layout, &area, count, rects);

	for (i = 0; i < count; i++) {
		damage_window(server, visible[i], "layout-old");
		visible[i]->tiled = rects[i];
		damage_window(server, visible[i], "layout-new");
		inis_backend_apply_window(&server->backend, visible[i]);
	}

	for (i = 0; i < server->window_count; i++) {
		if (!server->windows[i].mapped)
			continue;
		if (server->windows[i].workspace_index >= server->workspace_count)
			continue;
		if (server->windows[i].state == INIS_WINDOW_TILED) {
			if (!server->workspaces[server->windows[i].workspace_index].special)
				continue;
			window_default_floating(server, &server->windows[i]);
			inis_window_set_floating(&server->windows[i], true);
		}
		if (server->windows[i].state == INIS_WINDOW_FLOATING &&
		    server->windows[i].sync_geometry_from_backend)
			(void)inis_backend_sync_window_geometry(&server->backend,
			    &server->windows[i]);
		/* ensure floating windows always have a valid rect */
		if (server->windows[i].state == INIS_WINDOW_FLOATING &&
		    server->windows[i].floating.w <= 0)
			window_default_floating(server, &server->windows[i]);
		inis_backend_apply_window(&server->backend, &server->windows[i]);
	}

	/*
	 * Floating and fullscreen windows must always render above tiled ones.
	 * Raise non-focused floating windows first, then the focused window last
	 * so it sits at the very top of the stack.
	 */
	for (i = 0; i < server->window_count; i++) {
		if (!server->windows[i].mapped)
			continue;
		if (server->windows[i].workspace_index >= server->workspace_count)
			continue;
		if (server->windows[i].state == INIS_WINDOW_TILED)
			continue;
		if (&server->windows[i] == server->focused_window)
			continue;
		if (server->windows[i].workspace_index != active_workspace) {
			const struct inis_workspace *ws =
			    &server->workspaces[server->windows[i].workspace_index];
			if (!ws->special || !ws->visible)
				continue;
		}
		inis_backend_raise_window(&server->backend, &server->windows[i]);
	}
	if (server->focused_window != NULL &&
	    server->focused_window->mapped &&
	    server->focused_window->state != INIS_WINDOW_TILED)
		inis_backend_raise_window(&server->backend, server->focused_window);
}

void
inis_server_mark_damage(struct inis_server *server,
    const struct inis_rect *rect, const char *reason)
{
	if (rect == NULL)
		return;
	inis_damage_add_rect(&server->damage, rect, reason);
	mark_damage_on_outputs(server, rect, reason);
}

void
inis_server_flush_damage(struct inis_server *server, const char *why)
{
	inis_render_flush_damage(server, why);
}

int
inis_server_focus_window(struct inis_server *server, struct inis_window *window)
{
	struct inis_window *old;

	if (window != NULL && !window_is_focusable(server, window))
		return -1;
	if (server->focused_window == window) {
		if (window != NULL) {
			inis_backend_focus_window(&server->backend, window);
			if (window->state != INIS_WINDOW_TILED)
				inis_backend_raise_window(&server->backend, window);
		}
		return 0;
	}

	old = server->focused_window;
	if (old != NULL) {
		damage_window(server, old, "focus-old");
		old->focused = false;
		inis_backend_apply_window(&server->backend, old);
	}

	server->focused_window = window;
	if (window != NULL) {
		window->focused = true;
		inis_backend_focus_window(&server->backend, window);
		if (window->state != INIS_WINDOW_TILED)
			inis_backend_raise_window(&server->backend, window);
		damage_window(server, window, "focus-new");
		inis_backend_apply_window(&server->backend, window);
	} else {
		inis_backend_focus_window(&server->backend, NULL);
	}

	return 0;
}

int
inis_server_focus_next(struct inis_server *server, int direction)
{
	struct inis_window *candidate = NULL;
	size_t i;
	size_t focused_index = INIS_MAX_WINDOWS;

	if (direction == 0)
		direction = 1;

	for (i = 0; i < server->window_count; i++) {
		if (&server->windows[i] == server->focused_window) {
			focused_index = i;
			break;
		}
	}

	if (direction > 0) {
		size_t start = focused_index == INIS_MAX_WINDOWS ? 0 : focused_index + 1;

		for (i = start; i < server->window_count; i++) {
			if (window_is_focusable(server, &server->windows[i])) {
				candidate = &server->windows[i];
				break;
			}
		}
		if (candidate == NULL) {
			for (i = 0; i < start && i < server->window_count; i++) {
				if (window_is_focusable(server, &server->windows[i])) {
					candidate = &server->windows[i];
					break;
				}
			}
		}
	} else {
		size_t start = focused_index == INIS_MAX_WINDOWS ?
		    server->window_count : focused_index;

		for (i = start; i > 0; i--) {
			if (window_is_focusable(server, &server->windows[i - 1])) {
				candidate = &server->windows[i - 1];
				break;
			}
		}
		if (candidate == NULL) {
			for (i = server->window_count; i > start; i--) {
				if (window_is_focusable(server, &server->windows[i - 1])) {
					candidate = &server->windows[i - 1];
					break;
				}
			}
		}
	}

	if (candidate == NULL)
		return -1;
	return inis_server_focus_window(server, candidate);
}

int
inis_server_switch_workspace(struct inis_server *server, const char *name)
{
	int index;
	size_t i;

	if (name == NULL || name[0] == '\0')
		return -1;

	index = workspace_get_or_create(server, name, false);
	if (index < 0)
		return -1;

	if (server->monitor_count == 0) {
		server->workspaces[index].active = true;
		return 0;
	}

	server->monitors[0].previous_workspace = server->monitors[0].active_workspace;
	server->monitors[0].active_workspace = (unsigned int)index;

	for (i = 0; i < server->workspace_count; i++)
		server->workspaces[i].active = i == (size_t)index;

	damage_monitor(server, &server->monitors[0], "workspace-switch");
	inis_server_focus_window(server, NULL);
	for (i = server->window_count; i > 0; i--) {
		if (server->windows[i - 1].mapped &&
		    server->windows[i - 1].workspace_index == (unsigned int)index) {
			inis_server_focus_window(server, &server->windows[i - 1]);
			break;
		}
	}

	inis_info("workspace switched: %s", server->workspaces[index].name);
	inis_server_arrange(server);
	inis_server_flush_damage(server, "workspace-switch");
	return 0;
}

int
inis_server_switch_previous_workspace(struct inis_server *server)
{
	unsigned int previous;
	char name[INIS_MAX_NAME];

	if (server->monitor_count == 0)
		return -1;

	previous = server->monitors[0].previous_workspace;
	if (previous >= server->workspace_count)
		return -1;

	snprintf(name, sizeof(name), "%s", server->workspaces[previous].name);
	return inis_server_switch_workspace(server, name);
}

int
inis_server_switch_relative_workspace(struct inis_server *server, int delta)
{
	unsigned int active;
	int current;
	int next;
	char name[INIS_MAX_NAME];

	if (server->monitor_count == 0)
		return -1;

	active = server->monitors[0].active_workspace;
	if (active >= server->workspace_count)
		return -1;

	current = atoi(server->workspaces[active].name);
	if (current < 1 || current > 10)
		return -1;

	next = current + delta;
	while (next < 1)
		next += 10;
	while (next > 10)
		next -= 10;

	snprintf(name, sizeof(name), "%d", next);
	return inis_server_switch_workspace(server, name);
}

int
inis_server_move_focused_to_workspace(struct inis_server *server,
    const char *name, bool silent)
{
	int index;
	struct inis_window *window;
	struct inis_workspace *workspace;

	if (server->focused_window == NULL || name == NULL || name[0] == '\0')
		return -1;

	index = workspace_get_or_create(server, name,
	    strncmp(name, "special:", 8) == 0);
	if (index < 0)
		return -1;

	window = server->focused_window;
	damage_window(server, window, "workspace-move-old");
	if (window->workspace_index < server->workspace_count &&
	    server->workspaces[window->workspace_index].window_count > 0)
		server->workspaces[window->workspace_index].window_count--;

	window->workspace_index = (unsigned int)index;
	workspace = &server->workspaces[index];
	workspace->window_count++;
	if (workspace->special && window->state == INIS_WINDOW_TILED) {
		inis_window_set_floating(window, true);
		window_default_floating(server, window);
	}

	if (workspace->special) {
		if (!workspace->visible) {
			inis_server_focus_window(server, NULL);
			(void)inis_server_focus_next(server, 1);
		} else {
			(void)inis_server_focus_window(server, window);
		}
		inis_server_arrange(server);
	} else if (!silent) {
		inis_server_switch_workspace(server, name);
	} else {
		inis_server_arrange(server);
		inis_server_flush_damage(server, "workspace-move-silent");
	}

	inis_info("window moved to workspace: %s", name);
	return 0;
}

int
inis_server_toggle_special_workspace(struct inis_server *server, const char *name)
{
	char special_name[INIS_MAX_NAME];
	struct inis_workspace *workspace;
	int index;
	size_t i;

	if (name == NULL || name[0] == '\0')
		name = "magic";

	if (strncmp(name, "special:", 8) == 0)
		snprintf(special_name, sizeof(special_name), "%s", name);
	else
		snprintf(special_name, sizeof(special_name), "special:%s", name);

	index = workspace_get_or_create(server, special_name, true);
	if (index < 0)
		return -1;

	workspace = &server->workspaces[index];
	workspace->special = true;
	workspace->visible = !workspace->visible;

	if (server->monitor_count > 0)
		damage_monitor(server, &server->monitors[0], "special-workspace");

	if (!workspace->visible && server->focused_window != NULL &&
	    server->focused_window->workspace_index == (unsigned int)index)
		inis_server_focus_window(server, NULL);

	if (workspace->visible) {
		for (i = server->window_count; i > 0; i--) {
			if (server->windows[i - 1].mapped &&
			    server->windows[i - 1].workspace_index == (unsigned int)index) {
				inis_server_focus_window(server, &server->windows[i - 1]);
				break;
			}
		}
	} else if (server->focused_window == NULL) {
		(void)inis_server_focus_next(server, 1);
	}

	inis_server_arrange(server);
	inis_info("special workspace %s: %s", workspace->name,
	    workspace->visible ? "visible" : "hidden");
	inis_server_flush_damage(server, "special-workspace");
	return 0;
}

int
inis_server_center_focused(struct inis_server *server)
{
	struct inis_window *window = server->focused_window;

	if (window == NULL)
		return -1;
	window_make_directly_managed(server, window);
	damage_window(server, window, "center-old");
	window_center(server, window);
	damage_window(server, window, "center-new");
	inis_backend_apply_window(&server->backend, window);
	inis_server_flush_damage(server, "centerwindow");
	return 0;
}

int
inis_server_move_focused(struct inis_server *server, int dx, int dy)
{
	struct inis_window *window = server->focused_window;

	if (window == NULL)
		return -1;
	window_make_directly_managed(server, window);
	damage_window(server, window, "move-old");
	window->floating.x += dx;
	window->floating.y += dy;
	damage_window(server, window, "move-new");
	inis_backend_apply_window(&server->backend, window);
	inis_server_flush_damage(server, "moveactive");
	return 0;
}

int
inis_server_resize_focused(struct inis_server *server, int dw, int dh)
{
	struct inis_window *window = server->focused_window;
	struct inis_rect area = { 0, 0, 800, 600 };
	int max_w;
	int max_h;

	if (window == NULL)
		return -1;
	window_make_directly_managed(server, window);
	damage_window(server, window, "resize-old");
	if (server->monitor_count > window->monitor_index)
		area = server->monitors[window->monitor_index].usable;

	max_w = area.w > 0 ? area.w : 8192;
	max_h = area.h > 0 ? area.h : 8192;
	window->floating.w = clamp_int(window->floating.w + dw, 80, max_w);
	window->floating.h = clamp_int(window->floating.h + dh, 60, max_h);
	damage_window(server, window, "resize-new");
	inis_backend_apply_window(&server->backend, window);
	inis_server_flush_damage(server, "resizeactive");
	return 0;
}

int
inis_server_swap_focused(struct inis_server *server, int direction)
{
	struct inis_window *focused = server->focused_window;
	struct inis_window *candidate = NULL;
	unsigned int active_workspace;
	unsigned int tmp_order;
	size_t i;

	if (focused == NULL || focused->state != INIS_WINDOW_TILED)
		return -1;
	if (server->monitor_count == 0)
		return -1;

	active_workspace = server->monitors[0].active_workspace;
	if (focused->workspace_index != active_workspace)
		return -1;
	if (direction == 0)
		direction = 1;

	for (i = 0; i < server->window_count; i++) {
		struct inis_window *window = &server->windows[i];

		if (!window->mapped || window == focused)
			continue;
		if (window->workspace_index != active_workspace ||
		    window->state != INIS_WINDOW_TILED)
			continue;

		if (direction > 0) {
			if (window->layout_order <= focused->layout_order)
				continue;
			if (candidate == NULL ||
			    window->layout_order < candidate->layout_order)
				candidate = window;
		} else {
			if (window->layout_order >= focused->layout_order)
				continue;
			if (candidate == NULL ||
			    window->layout_order > candidate->layout_order)
				candidate = window;
		}
	}

	if (candidate == NULL)
		return -1;

	tmp_order = focused->layout_order;
	focused->layout_order = candidate->layout_order;
	candidate->layout_order = tmp_order;
	inis_server_arrange(server);
	inis_server_flush_damage(server, "swapwindow");
	return 0;
}

int
inis_server_begin_mouse_move(struct inis_server *server)
{
	struct inis_window *window = server->focused_window;

	if (window == NULL)
		return -1;
	window_make_directly_managed(server, window);
	damage_window(server, window, "mouse-move");
	window->sync_geometry_from_backend = true;
	inis_backend_apply_window(&server->backend, window);
	(void)inis_server_focus_window(server, window);
	inis_backend_begin_move(&server->backend, window);
	inis_server_flush_damage(server, "movewindow");
	return 0;
}

int
inis_server_begin_mouse_resize(struct inis_server *server, uint32_t edges)
{
	struct inis_window *window = server->focused_window;

	if (window == NULL)
		return -1;
	window_make_directly_managed(server, window);
	damage_window(server, window, "mouse-resize");
	window->sync_geometry_from_backend = true;
	inis_backend_apply_window(&server->backend, window);
	(void)inis_server_focus_window(server, window);
	inis_backend_begin_resize(&server->backend, window, edges);
	inis_server_flush_damage(server, "resizewindow");
	return 0;
}

void
inis_server_reload_config(struct inis_server *server)
{
	inis_backend_reload_bindings(&server->backend);
	inis_server_arrange(server);
	inis_server_flush_damage(server, "reload");
	inis_info("config reload complete");
}
