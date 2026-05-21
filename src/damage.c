#include "damage.h"

#include <stdio.h>
#include <string.h>

static int
max_int(int a, int b)
{
	return a > b ? a : b;
}

static int
min_int(int a, int b)
{
	return a < b ? a : b;
}

void
inis_damage_init(struct inis_damage *damage)
{
	damage->pending = false;
	damage->bounds.x = 0;
	damage->bounds.y = 0;
	damage->bounds.w = 0;
	damage->bounds.h = 0;
	damage->events = 0;
	damage->flushes = 0;
	damage->skipped_flushes = 0;
	damage->reason[0] = '\0';
}

void
inis_damage_add_rect(struct inis_damage *damage,
    const struct inis_rect *rect, const char *reason)
{
	int x2;
	int y2;
	int bx2;
	int by2;

	if (rect->w <= 0 || rect->h <= 0)
		return;

	damage->events++;
	if (reason != NULL && reason[0] != '\0')
		snprintf(damage->reason, sizeof(damage->reason), "%s", reason);

	if (!damage->pending) {
		damage->bounds = *rect;
		damage->pending = true;
		return;
	}

	x2 = rect->x + rect->w;
	y2 = rect->y + rect->h;
	bx2 = damage->bounds.x + damage->bounds.w;
	by2 = damage->bounds.y + damage->bounds.h;

	damage->bounds.x = min_int(damage->bounds.x, rect->x);
	damage->bounds.y = min_int(damage->bounds.y, rect->y);
	damage->bounds.w = max_int(bx2, x2) - damage->bounds.x;
	damage->bounds.h = max_int(by2, y2) - damage->bounds.y;
}

void
inis_damage_clear(struct inis_damage *damage)
{
	damage->pending = false;
	damage->bounds.x = 0;
	damage->bounds.y = 0;
	damage->bounds.w = 0;
	damage->bounds.h = 0;
	damage->reason[0] = '\0';
}
