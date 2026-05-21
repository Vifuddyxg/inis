#include "dispatch.h"

#include "backend.h"
#include "log.h"
#include "server.h"
#include "window.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static void dispatch_exec(struct inis_server *server, const char *args);
static void dispatch_exit(struct inis_server *server, const char *args);
static void dispatch_shutdownmenu(struct inis_server *server, const char *args);
static void dispatch_killactive(struct inis_server *server, const char *args);
static void dispatch_closewindow(struct inis_server *server, const char *args);
static void dispatch_togglefloating(struct inis_server *server, const char *args);
static void dispatch_fullscreen(struct inis_server *server, const char *args);
static void dispatch_centerwindow(struct inis_server *server, const char *args);
static void dispatch_moveactive(struct inis_server *server, const char *args);
static void dispatch_resizeactive(struct inis_server *server, const char *args);
static void dispatch_swapwindow(struct inis_server *server, const char *args);
static void dispatch_pseudo(struct inis_server *server, const char *args);
static void dispatch_togglesplit(struct inis_server *server, const char *args);
static void dispatch_workspace(struct inis_server *server, const char *args);
static void dispatch_movetoworkspace(struct inis_server *server, const char *args);
static void dispatch_movetoworkspacesilent(struct inis_server *server, const char *args);
static void dispatch_previous(struct inis_server *server, const char *args);
static void dispatch_movefocus(struct inis_server *server, const char *args);
static void dispatch_cyclenext(struct inis_server *server, const char *args);
static void dispatch_cycleprev(struct inis_server *server, const char *args);
static void dispatch_togglespecialworkspace(struct inis_server *server, const char *args);
static void dispatch_movewindow(struct inis_server *server, const char *args);
static void dispatch_resizewindow(struct inis_server *server, const char *args);
static void dispatch_reload(struct inis_server *server, const char *args);

static const struct inis_dispatcher dispatchers[] = {
	{ "exec", dispatch_exec },
	{ "exit", dispatch_exit },
	{ "shutdownmenu", dispatch_shutdownmenu },
	{ "killactive", dispatch_killactive },
	{ "closewindow", dispatch_closewindow },
	{ "togglefloating", dispatch_togglefloating },
	{ "fullscreen", dispatch_fullscreen },
	{ "centerwindow", dispatch_centerwindow },
	{ "moveactive", dispatch_moveactive },
	{ "resizeactive", dispatch_resizeactive },
	{ "swapwindow", dispatch_swapwindow },
	{ "pseudo", dispatch_pseudo },
	{ "togglesplit", dispatch_togglesplit },
	{ "workspace", dispatch_workspace },
	{ "movetoworkspace", dispatch_movetoworkspace },
	{ "movetoworkspacesilent", dispatch_movetoworkspacesilent },
	{ "previous", dispatch_previous },
	{ "movefocus", dispatch_movefocus },
	{ "cyclenext", dispatch_cyclenext },
	{ "cycleprev", dispatch_cycleprev },
	{ "togglespecialworkspace", dispatch_togglespecialworkspace },
	{ "movewindow", dispatch_movewindow },
	{ "resizewindow", dispatch_resizewindow },
	{ "reload", dispatch_reload },
};

int
inis_dispatch(struct inis_server *server, const char *name, const char *args)
{
	size_t i;

	for (i = 0; i < sizeof(dispatchers) / sizeof(dispatchers[0]); i++) {
		if (strcmp(dispatchers[i].name, name) == 0) {
			dispatchers[i].run(server, args ? args : "");
			return 0;
		}
	}

	inis_warn("unknown dispatcher: %s", name);
	return -1;
}

static void
dispatch_exec(struct inis_server *server, const char *args)
{
	pid_t pid;

	(void)server;
	if (args == NULL || args[0] == '\0') {
		inis_warn("exec requires a command");
		return;
	}

	pid = fork();
	if (pid < 0) {
		inis_warn("fork failed for exec: %s", strerror(errno));
		return;
	}
	if (pid == 0) {
		setsid();
		execl("/bin/sh", "sh", "-c", args, (char *)NULL);
		_exit(127);
	}
}

static void
dispatch_exit(struct inis_server *server, const char *args)
{
	(void)args;
	inis_server_request_exit(server);
}

static bool
path_has_executable(const char *name)
{
	char path[1024];
	char full[1024];
	char *dir;
	const char *env;

	env = getenv("PATH");
	if (env == NULL || env[0] == '\0')
		return false;

	snprintf(path, sizeof(path), "%s", env);
	dir = strtok(path, ":");
	while (dir != NULL) {
		if (dir[0] == '\0')
			dir = ".";
		snprintf(full, sizeof(full), "%s/%s", dir, name);
		if (access(full, X_OK) == 0)
			return true;
		dir = strtok(NULL, ":");
	}

	return false;
}

static void
dispatch_shutdownmenu(struct inis_server *server, const char *args)
{
	(void)args;
	if (path_has_executable("hyprshutdown")) {
		dispatch_exec(server, "hyprshutdown");
		return;
	}
	inis_info("hyprshutdown not found; exiting compositor");
	inis_server_request_exit(server);
}

static void
dispatch_killactive(struct inis_server *server, const char *args)
{
	(void)args;
	if (server->focused_window == NULL) {
		inis_debug("killactive ignored: no focused window");
		return;
	}
	inis_backend_close_window(&server->backend, server->focused_window);
}

static void
dispatch_closewindow(struct inis_server *server, const char *args)
{
	dispatch_killactive(server, args);
}

static void
dispatch_togglefloating(struct inis_server *server, const char *args)
{
	bool make_floating;

	(void)args;
	if (server->focused_window == NULL)
		return;
	make_floating = server->focused_window->state != INIS_WINDOW_FLOATING;
	if (make_floating) {
		/*
		 * Reset floating rect to zero so arrange() calls
		 * window_default_floating(), which centres the window
		 * at a compact default size.  Without this, the window
		 * keeps its tiled dimensions and just detaches in place.
		 */
		server->focused_window->floating.w = 0;
		server->focused_window->floating.h = 0;
	}
	inis_window_set_floating(server->focused_window, make_floating);
	inis_server_arrange(server);
	(void)inis_server_focus_window(server, server->focused_window);
	inis_server_flush_damage(server, "togglefloating");
}

static void
dispatch_fullscreen(struct inis_server *server, const char *args)
{
	(void)args;
	if (server->focused_window == NULL)
		return;
	inis_window_set_fullscreen(server->focused_window,
	    server->focused_window->state != INIS_WINDOW_FULLSCREEN);
	inis_server_arrange(server);
	inis_server_flush_damage(server, "fullscreen");
}

static int
parse_delta(const char *args, int *a, int *b)
{
	if (args == NULL || args[0] == '\0')
		return -1;
	return sscanf(args, "%d %d", a, b) == 2 ? 0 : -1;
}

static void
dispatch_centerwindow(struct inis_server *server, const char *args)
{
	(void)args;
	if (inis_server_center_focused(server) != 0)
		inis_debug("centerwindow ignored: no focused window");
}

static void
dispatch_moveactive(struct inis_server *server, const char *args)
{
	int dx;
	int dy;

	if (parse_delta(args, &dx, &dy) != 0) {
		inis_warn("moveactive requires: DX DY");
		return;
	}
	if (inis_server_move_focused(server, dx, dy) != 0)
		inis_debug("moveactive ignored: no focused window");
}

static void
dispatch_resizeactive(struct inis_server *server, const char *args)
{
	int dw;
	int dh;

	if (parse_delta(args, &dw, &dh) != 0) {
		inis_warn("resizeactive requires: DW DH");
		return;
	}
	if (inis_server_resize_focused(server, dw, dh) != 0)
		inis_debug("resizeactive ignored: no focused window");
}

static void
dispatch_swapwindow(struct inis_server *server, const char *args)
{
	int direction = 1;

	if (args != NULL &&
	    (strcmp(args, "left") == 0 || strcmp(args, "up") == 0))
		direction = -1;
	if (inis_server_swap_focused(server, direction) != 0)
		inis_debug("swapwindow ignored: no tiled neighbor");
}

static void
dispatch_pseudo(struct inis_server *server, const char *args)
{
	(void)server;
	(void)args;
	inis_debug("pseudo ignored: dwindle pseudo mode is not implemented");
}

static void
dispatch_togglesplit(struct inis_server *server, const char *args)
{
	(void)server;
	(void)args;
	inis_debug("togglesplit ignored: dwindle split state is not implemented");
}

static void
dispatch_workspace(struct inis_server *server, const char *args)
{
	if (strcmp(args, "e+1") == 0 || strcmp(args, "+1") == 0) {
		if (inis_server_switch_relative_workspace(server, 1) != 0)
			inis_warn("workspace next failed");
		return;
	}
	if (strcmp(args, "e-1") == 0 || strcmp(args, "-1") == 0) {
		if (inis_server_switch_relative_workspace(server, -1) != 0)
			inis_warn("workspace previous failed");
		return;
	}
	if (strcmp(args, "previous") == 0) {
		if (inis_server_switch_previous_workspace(server) != 0)
			inis_warn("workspace previous failed");
		return;
	}
	if (inis_server_switch_workspace(server, args) != 0)
		inis_warn("workspace failed: %s", args);
}

static void
dispatch_movetoworkspace(struct inis_server *server, const char *args)
{
	if (inis_server_move_focused_to_workspace(server, args, false) != 0)
		inis_warn("movetoworkspace failed: %s", args);
}

static void
dispatch_movetoworkspacesilent(struct inis_server *server, const char *args)
{
	if (inis_server_move_focused_to_workspace(server, args, true) != 0)
		inis_warn("movetoworkspacesilent failed: %s", args);
}

static void
dispatch_previous(struct inis_server *server, const char *args)
{
	(void)args;
	if (inis_server_switch_previous_workspace(server) != 0)
		inis_warn("previous workspace failed");
}

static void
dispatch_movefocus(struct inis_server *server, const char *args)
{
	int direction = 1;

	if (args != NULL &&
	    (strcmp(args, "left") == 0 || strcmp(args, "up") == 0))
		direction = -1;
	if (inis_server_focus_next(server, direction) != 0)
		inis_debug("movefocus ignored: no focusable window");
}

static void
dispatch_cyclenext(struct inis_server *server, const char *args)
{
	(void)args;
	if (inis_server_focus_next(server, 1) != 0)
		inis_debug("cyclenext ignored: no focusable window");
}

static void
dispatch_cycleprev(struct inis_server *server, const char *args)
{
	(void)args;
	if (inis_server_focus_next(server, -1) != 0)
		inis_debug("cycleprev ignored: no focusable window");
}

static void
dispatch_togglespecialworkspace(struct inis_server *server, const char *args)
{
	if (inis_server_toggle_special_workspace(server, args) != 0)
		inis_warn("togglespecialworkspace failed: %s", args);
}

static void
dispatch_movewindow(struct inis_server *server, const char *args)
{
	(void)args;
	if (inis_server_begin_mouse_move(server) != 0)
		inis_debug("movewindow ignored: no focused window");
}

static void
dispatch_resizewindow(struct inis_server *server, const char *args)
{
	(void)args;
	if (inis_server_begin_mouse_resize(server, INIS_WINDOW_EDGE_AUTO) != 0)
		inis_debug("resizewindow ignored: no focused window");
}

static void
dispatch_reload(struct inis_server *server, const char *args)
{
	(void)args;
	inis_server_reload_config(server);
}
