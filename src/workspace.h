#ifndef INIS_WORKSPACE_H
#define INIS_WORKSPACE_H

#include "inis.h"
#include "layout.h"

struct inis_workspace {
	char name[INIS_MAX_NAME];
	unsigned int monitor_index;
	bool active;
	bool special;
	bool visible;
	size_t window_count;
	struct wc_workspace layout;
};

void inis_workspace_init(struct inis_workspace *workspace, const char *name, bool special);
void inis_workspace_finish(struct inis_workspace *workspace);

#endif
