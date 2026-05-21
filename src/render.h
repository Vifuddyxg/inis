#ifndef INIS_RENDER_H
#define INIS_RENDER_H

#include "inis.h"

struct inis_render_stats {
	unsigned long frames;
	unsigned long skipped;
};

void inis_render_output(struct inis_server *server, struct inis_monitor *monitor);
void inis_render_flush_damage(struct inis_server *server, const char *why);

#endif
