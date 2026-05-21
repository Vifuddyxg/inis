#ifndef INIS_BACKEND_H
#define INIS_BACKEND_H

#include "inis.h"

#include <stdint.h>

struct wl_display;
struct wl_event_source;

struct inis_backend_binding {
	struct inis_server *server;
	char dispatcher[INIS_MAX_NAME];
	char args[INIS_MAX_ARGS];
	uint32_t swc_type;
	uint32_t swc_mods;
	uint32_t swc_value;
};

struct inis_backend {
	bool started;
	struct inis_server *server;
	struct wl_display *display;
	struct wl_event_source *sigint_source;
	struct wl_event_source *sigterm_source;
	struct wl_event_source *ipc_source;
	const char *wayland_socket;
	char wayland_env[INIS_MAX_NAME + 32];
	struct inis_backend_binding backend_bindings[INIS_MAX_BINDINGS];
	size_t backend_binding_count;
};

void inis_backend_init(struct inis_backend *backend, struct inis_server *server);
int inis_backend_start(struct inis_backend *backend);
int inis_backend_run(struct inis_backend *backend);
void inis_backend_request_stop(struct inis_backend *backend);
void inis_backend_finish(struct inis_backend *backend);
void inis_backend_close_window(struct inis_backend *backend,
    struct inis_window *window);
void inis_backend_focus_window(struct inis_backend *backend,
    struct inis_window *window);
void inis_backend_apply_window(struct inis_backend *backend,
    struct inis_window *window);
void inis_backend_set_window_visible(struct inis_backend *backend,
    struct inis_window *window, bool visible);
void inis_backend_raise_window(struct inis_backend *backend,
    struct inis_window *window);
int inis_backend_sync_window_geometry(struct inis_backend *backend,
    struct inis_window *window);
void inis_backend_begin_move(struct inis_backend *backend,
    struct inis_window *window);
void inis_backend_begin_resize(struct inis_backend *backend,
    struct inis_window *window, uint32_t edges);
void inis_backend_reload_bindings(struct inis_backend *backend);

#endif
