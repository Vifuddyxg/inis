#ifndef INIS_LAYOUT_H
#define INIS_LAYOUT_H

#include "inis.h"

struct inis_layout {
	double master_ratio;
	int gaps_in;
	int gaps_out;
};

void inis_layout_master(const struct inis_layout *layout,
    const struct inis_rect *area, size_t count, struct inis_rect *out);

#endif
