#ifndef INIS_DAMAGE_H
#define INIS_DAMAGE_H

#include "inis.h"

struct inis_damage {
	bool pending;
	struct inis_rect bounds;
	unsigned long events;
	unsigned long flushes;
	unsigned long skipped_flushes;
	char reason[INIS_MAX_NAME];
};

void inis_damage_init(struct inis_damage *damage);
void inis_damage_add_rect(struct inis_damage *damage,
    const struct inis_rect *rect, const char *reason);
void inis_damage_clear(struct inis_damage *damage);

#endif
