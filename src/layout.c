#include "layout.h"

void
inis_layout_master(const struct inis_layout *layout,
    const struct inis_rect *area, size_t count, struct inis_rect *out)
{
	size_t i;
	int gap = layout->gaps_in;
	int outer = layout->gaps_out;
	struct inis_rect a = *area;
	int master_w;
	int stack_h;

	if (count == 0)
		return;

	a.x += outer;
	a.y += outer;
	a.w -= outer * 2;
	a.h -= outer * 2;
	if (a.w < 1)
		a.w = 1;
	if (a.h < 1)
		a.h = 1;

	if (count == 1) {
		out[0] = a;
		return;
	}

	master_w = (int)(a.w * layout->master_ratio);
	out[0].x = a.x;
	out[0].y = a.y;
	out[0].w = master_w - gap / 2;
	out[0].h = a.h;

	stack_h = (a.h - (int)(count - 2) * gap) / (int)(count - 1);
	for (i = 1; i < count; i++) {
		out[i].x = a.x + master_w + gap / 2;
		out[i].y = a.y + (int)(i - 1) * (stack_h + gap);
		out[i].w = a.w - master_w - gap / 2;
		out[i].h = stack_h;
	}
}
