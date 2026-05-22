#include "window.h"

#include <string.h>

static struct inis_rect
default_usable_area(const struct inis_rect *usable_area)
{
	if (usable_area != NULL && usable_area->w > 0 && usable_area->h > 0)
		return *usable_area;
	return (struct inis_rect){ 0, 0, 800, 600 };
}

static struct inis_rect
default_floating_rect(const struct inis_rect *usable_area)
{
	struct inis_rect area;
	struct inis_rect rect;

	area = default_usable_area(usable_area);
	rect.w = area.w * 3 / 5;
	rect.h = area.h * 3 / 5;
	if (rect.w < 50)
		rect.w = area.w < 50 ? area.w : 50;
	if (rect.h < 50)
		rect.h = area.h < 50 ? area.h : 50;
	rect.x = area.x + (area.w - rect.w) / 2;
	rect.y = area.y + (area.h - rect.h) / 2;
	return rect;
}

static int
clamp_int(int value, int min, int max)
{
	if (value < min)
		return min;
	if (value > max)
		return max;
	return value;
}

static void
place_floating_near_tiled(struct inis_window *window,
    const struct inis_rect *usable_area)
{
	struct inis_rect area;
	int min_x, max_x, min_y, max_y;

	if (window == NULL || !inis_window_rect_valid(&window->tiled) ||
	    !inis_window_rect_valid(&window->floating))
		return;

	area = default_usable_area(usable_area);
	window->floating.x = window->tiled.x +
	    (window->tiled.w - window->floating.w) / 2;
	window->floating.y = window->tiled.y +
	    (window->tiled.h - window->floating.h) / 2;

	min_x = area.x;
	min_y = area.y;
	max_x = area.x + area.w - window->floating.w;
	max_y = area.y + area.h - window->floating.h;
	if (max_x < min_x)
		max_x = min_x;
	if (max_y < min_y)
		max_y = min_y;

	window->floating.x = clamp_int(window->floating.x, min_x, max_x);
	window->floating.y = clamp_int(window->floating.y, min_y, max_y);
}

void
inis_window_init(struct inis_window *window)
{
	memset(window, 0, sizeof(*window));
	window->state = INIS_WINDOW_TILED;
	layout_init_view(&window->layout_view, 0);
	window->layout_view.user_data = window;
}

bool
inis_window_rect_valid(const struct inis_rect *rect)
{
	return rect != NULL && rect->w > 0 && rect->h > 0;
}

void
inis_window_ensure_floating_rect(struct inis_window *window,
    const struct inis_rect *usable_area)
{
	if (window == NULL)
		return;
	if (!inis_window_rect_valid(&window->floating))
		window->floating = default_floating_rect(usable_area);
}

bool
inis_window_make_floating(struct inis_window *window,
    const struct inis_rect *usable_area)
{
	bool changed = false;

	if (window == NULL)
		return false;
	if (window->state == INIS_WINDOW_FULLSCREEN) {
		inis_window_set_fullscreen(window, false);
		changed = true;
	}
	if (window->state == INIS_WINDOW_TILED) {
		if (!inis_window_rect_valid(&window->floating))
			inis_window_ensure_floating_rect(window, usable_area);
		place_floating_near_tiled(window, usable_area);
		inis_window_set_floating(window, true);
		changed = true;
	} else {
		inis_window_ensure_floating_rect(window, usable_area);
	}
	return changed;
}

bool
inis_window_make_floating_centered(struct inis_window *window,
    const struct inis_rect *usable_area)
{
	bool changed = false;

	if (window == NULL)
		return false;
	if (window->state == INIS_WINDOW_FULLSCREEN) {
		inis_window_set_fullscreen(window, false);
		changed = true;
	}
	if (window->state != INIS_WINDOW_FLOATING)
		changed = true;
	window->floating = default_floating_rect(usable_area);
	inis_window_set_floating(window, true);
	return changed;
}

void
inis_window_center_floating(struct inis_window *window,
    const struct inis_rect *usable_area)
{
	struct inis_rect area;

	if (window == NULL)
		return;
	inis_window_ensure_floating_rect(window, usable_area);
	area = default_usable_area(usable_area);
	window->floating.x = area.x + (area.w - window->floating.w) / 2;
	window->floating.y = area.y + (area.h - window->floating.h) / 2;
}

void
inis_window_set_floating(struct inis_window *window, bool floating)
{
	if (window->state == INIS_WINDOW_FULLSCREEN)
		return;
	window->state = floating ? INIS_WINDOW_FLOATING : INIS_WINDOW_TILED;
}

void
inis_window_set_fullscreen(struct inis_window *window, bool fullscreen)
{
	if (fullscreen) {
		window->was_floating = (window->state == INIS_WINDOW_FLOATING);
		window->state = INIS_WINDOW_FULLSCREEN;
	} else if (window->state == INIS_WINDOW_FULLSCREEN) {
		window->state = window->was_floating
		    ? INIS_WINDOW_FLOATING : INIS_WINDOW_TILED;
	}
}
