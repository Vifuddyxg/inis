#include "src/layout.h"

#include <stdio.h>
#include <stdlib.h>

static const char *
layout_name(enum wc_layout_type type)
{
	switch (type) {
	case WC_LAYOUT_MASTER:
		return "master";
	case WC_LAYOUT_MONOCLE:
		return "monocle";
	case WC_LAYOUT_DWINDLE:
	default:
		return "dwindle";
	}
}

static void
expect_valid(const struct wc_workspace *ws, const char *label)
{
	if (!layout_validate_tree(ws)) {
		fprintf(stderr, "tree validation failed after %s\n", label);
		layout_debug_print_tree(ws, stderr);
		exit(1);
	}
}

static void
dump_views(const struct wc_workspace *ws, struct wc_view *views, size_t count,
    const char *label)
{
	size_t i;

	printf("\n== %s (%s) ==\n", label, layout_name(layout_get_type(ws)));
	printf("focused:");
	if (ws->focused_view != NULL)
		printf(" %u\n", ws->focused_view->id);
	else
		printf(" none\n");

	for (i = 0; i < count; i++) {
		struct wc_view *view = &views[i];

		if (view->workspace != ws || view->layout_node == NULL)
			continue;
		printf("view %u visible:%s geom:%dx%d+%d+%d\n",
		    view->id,
		    view->tiled_visible ? "yes" : "no",
		    view->pending_geometry.w, view->pending_geometry.h,
		    view->pending_geometry.x, view->pending_geometry.y);
	}

	puts("tree:");
	layout_debug_print_tree(ws, stdout);
}

int
main(void)
{
	struct wc_workspace ws;
	struct wc_view views[5];
	struct wc_box area;
	size_t i;

	layout_init_workspace(&ws);
	ws.config.outer_gap = 8;
	ws.config.inner_gap = 6;
	ws.config.border_width = 2;
	ws.config.default_split_ratio = 0.5f;
	ws.config.master_ratio = 0.55f;
	area.x = 0;
	area.y = 0;
	area.w = 1280;
	area.h = 720;

	for (i = 0; i < 5; i++)
		layout_init_view(&views[i], (unsigned int)i + 1);

	layout_add_view(&ws, &views[0]);
	layout_arrange(&ws, area);
	expect_valid(&ws, "add 1");
	dump_views(&ws, views, 5, "add 1 window");

	layout_add_view(&ws, &views[1]);
	layout_arrange(&ws, area);
	expect_valid(&ws, "add 2");
	dump_views(&ws, views, 5, "add 2 windows");

	layout_add_view(&ws, &views[2]);
	layout_arrange(&ws, area);
	expect_valid(&ws, "add 3");
	dump_views(&ws, views, 5, "add 3 windows");

	layout_add_view(&ws, &views[3]);
	layout_add_view(&ws, &views[4]);
	layout_arrange(&ws, area);
	expect_valid(&ws, "add 5");
	dump_views(&ws, views, 5, "add 5 windows");

	layout_toggle_split(&ws);
	layout_arrange(&ws, area);
	expect_valid(&ws, "toggle split");
	dump_views(&ws, views, 5, "toggle split on focused");

	layout_resize_focused(&ws, 0.12f);
	layout_arrange(&ws, area);
	expect_valid(&ws, "resize split");
	dump_views(&ws, views, 5, "resize focused split");

	layout_cycle_prev(&ws);
	layout_arrange(&ws, area);
	expect_valid(&ws, "cycle prev");
	dump_views(&ws, views, 5, "cycle prev");

	layout_focus_direction(&ws, WC_DIRECTION_LEFT);
	layout_arrange(&ws, area);
	expect_valid(&ws, "focus left");
	dump_views(&ws, views, 5, "directional focus left");

	layout_remove_view(&ws, ws.focused_view);
	layout_arrange(&ws, area);
	expect_valid(&ws, "remove focused");
	dump_views(&ws, views, 5, "remove focused window");

	layout_remove_view(&ws, &views[0]);
	layout_arrange(&ws, area);
	expect_valid(&ws, "remove root candidate");
	dump_views(&ws, views, 5, "remove first/root-side window");

	layout_set_type(&ws, WC_LAYOUT_MASTER);
	layout_arrange(&ws, area);
	expect_valid(&ws, "master");
	dump_views(&ws, views, 5, "switch to master");

	layout_set_type(&ws, WC_LAYOUT_MONOCLE);
	layout_arrange(&ws, area);
	expect_valid(&ws, "monocle");
	dump_views(&ws, views, 5, "switch to monocle");

	layout_set_type(&ws, WC_LAYOUT_DWINDLE);
	layout_swap_focused_with_master_or_root(&ws);
	layout_arrange(&ws, area);
	expect_valid(&ws, "swap root");
	dump_views(&ws, views, 5, "swap focused with root/master");

	layout_finish_workspace(&ws);
	return 0;
}
