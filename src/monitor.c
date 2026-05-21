#include "monitor.h"

#include <stdio.h>
#include <string.h>

void
inis_monitor_init(struct inis_monitor *monitor, const char *name)
{
	memset(monitor, 0, sizeof(*monitor));
	snprintf(monitor->name, sizeof(monitor->name), "%s", name);
	monitor->scale = 1;
	monitor->enabled = true;
	inis_damage_init(&monitor->damage);
}
