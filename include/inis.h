#ifndef INIS_H
#define INIS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define INIS_MAX_NAME 64
#define INIS_MAX_WINDOWS 256
#define INIS_MAX_MONITORS 16
#define INIS_MAX_WORKSPACES 64
#define INIS_MAX_BINDINGS 256
#define INIS_MAX_RULES 128
#define INIS_MAX_CONFIG_VARS 32
#define INIS_MAX_STARTUP_COMMANDS 32
#define INIS_MAX_ARGS 256
#define INIS_MAX_LINE 512

#define INIS_BIND_MOD_CTRL  (1u << 0)
#define INIS_BIND_MOD_ALT   (1u << 1)
#define INIS_BIND_MOD_LOGO  (1u << 2)
#define INIS_BIND_MOD_SHIFT (1u << 3)

#define INIS_BIND_KEY_RETURN 0xff0d
#define INIS_BIND_KEY_ESCAPE 0xff1b
#define INIS_BIND_KEY_LEFT   0xff51
#define INIS_BIND_KEY_UP     0xff52
#define INIS_BIND_KEY_RIGHT  0xff53
#define INIS_BIND_KEY_DOWN   0xff54

#define INIS_BIND_BUTTON_SCROLL_UP   4
#define INIS_BIND_BUTTON_SCROLL_DOWN 5

#define INIS_WINDOW_EDGE_AUTO   0
#define INIS_WINDOW_EDGE_TOP    (1u << 0)
#define INIS_WINDOW_EDGE_BOTTOM (1u << 1)
#define INIS_WINDOW_EDGE_LEFT   (1u << 2)
#define INIS_WINDOW_EDGE_RIGHT  (1u << 3)

#define INIS_COLOR_FOCUSED_BORDER 0xff5e81ac
#define INIS_COLOR_NORMAL_BORDER  0xff24283b

struct inis_rect {
	int x;
	int y;
	int w;
	int h;
};

struct inis_server;
struct inis_backend;
struct inis_monitor;
struct inis_window;
struct inis_workspace;
struct inis_layout;
struct inis_binding;
struct inis_dispatcher;
struct inis_rule;
struct inis_damage;

#endif
