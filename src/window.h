#ifndef INIS_WINDOW_H
#define INIS_WINDOW_H

#include "inis.h"

#if INIS_HAVE_SWC
struct swc_window;
#endif

enum inis_window_state {
	INIS_WINDOW_TILED,
	INIS_WINDOW_FLOATING,
	INIS_WINDOW_FULLSCREEN
};

struct inis_window {
	char app_id[INIS_MAX_NAME];
	char title[INIS_MAX_NAME];
	struct inis_rect tiled;
	struct inis_rect floating;
	enum inis_window_state state;
	unsigned int layout_order;
	unsigned int workspace_index;
	unsigned int monitor_index;
	bool mapped;
	bool focused;
	bool urgent;
	bool no_border;
	bool no_anim;
	bool sync_geometry_from_backend;
#if INIS_HAVE_SWC
	struct swc_window *swc;
#else
	void *swc;
#endif
};

void inis_window_init(struct inis_window *window);
void inis_window_set_floating(struct inis_window *window, bool floating);
void inis_window_set_fullscreen(struct inis_window *window, bool fullscreen);

#endif
