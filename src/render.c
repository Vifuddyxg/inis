#include "render.h"

#include "damage.h"
#include "log.h"
#include "monitor.h"
#include "server.h"

void
inis_render_output(struct inis_server *server, struct inis_monitor *monitor)
{
	(void)server;
	inis_debug("TODO render output %s only when damaged", monitor->name);
}

void
inis_render_flush_damage(struct inis_server *server, const char *why)
{
	struct inis_damage *damage = &server->damage;
	size_t i;

	if (!damage->pending) {
		damage->skipped_flushes++;
		for (i = 0; i < server->monitor_count; i++)
			server->monitors[i].damage.skipped_flushes++;
		inis_debug("render skip: no damage%s%s",
		    why != NULL && why[0] != '\0' ? " after " : "",
		    why != NULL ? why : "");
		return;
	}

	damage->flushes++;
	inis_debug("render damage: %dx%d+%d+%d reason:%s trigger:%s",
	    damage->bounds.w, damage->bounds.h,
	    damage->bounds.x, damage->bounds.y,
	    damage->reason[0] != '\0' ? damage->reason : "unknown",
	    why != NULL && why[0] != '\0' ? why : "unspecified");

	for (i = 0; i < server->monitor_count; i++) {
		struct inis_damage *mon_dmg = &server->monitors[i].damage;

		if (mon_dmg->pending) {
			mon_dmg->flushes++;
			inis_damage_clear(mon_dmg);
		}
	}

	inis_damage_clear(damage);
}
