#ifndef INIS_MONITOR_H
#define INIS_MONITOR_H

#include "inis.h"
#include "damage.h"

struct swc_screen;

struct inis_monitor {
	char name[INIS_MAX_NAME];
	struct inis_rect geometry;
	struct inis_rect usable;
	int scale;
	int refresh_mhz;
	unsigned int active_workspace;
	unsigned int previous_workspace;
	bool enabled;
	struct inis_damage damage;
	struct swc_screen *swc;
};

void inis_monitor_init(struct inis_monitor *monitor, const char *name);

#endif
