#include "window.h"

#include <string.h>

void
inis_window_init(struct inis_window *window)
{
	memset(window, 0, sizeof(*window));
	window->state = INIS_WINDOW_TILED;
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
	window->state = fullscreen ? INIS_WINDOW_FULLSCREEN : INIS_WINDOW_TILED;
}
