#ifndef INIS_LAYOUT_H
#define INIS_LAYOUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

struct wc_box {
	int x;
	int y;
	int w;
	int h;
};

enum wc_split {
	WC_SPLIT_AUTO,
	WC_SPLIT_VERTICAL,
	WC_SPLIT_HORIZONTAL
};

enum wc_layout_type {
	WC_LAYOUT_MASTER,
	WC_LAYOUT_DWINDLE,
	WC_LAYOUT_MONOCLE
};

enum wc_direction {
	WC_DIRECTION_LEFT,
	WC_DIRECTION_RIGHT,
	WC_DIRECTION_UP,
	WC_DIRECTION_DOWN
};

struct wc_view;
struct wc_workspace;

struct wc_layout_node {
	bool is_leaf;
	struct wc_view *view;

	struct wc_layout_node *parent;
	struct wc_layout_node *left;
	struct wc_layout_node *right;

	enum wc_split split;
	float ratio;
	bool preserve_split;

	struct wc_box box;
};

struct wc_layout_config {
	enum wc_layout_type default_layout;

	int outer_gap;
	int inner_gap;
	int border_width;

	bool preserve_split;
	bool smart_split;

	float default_split_ratio;
	float min_split_ratio;
	float max_split_ratio;

	float master_ratio;
	int master_count;
	bool new_is_master;
};

struct wc_view {
	unsigned int id;
	bool tiled_visible;
	bool floating;
	struct wc_box pending_geometry;
	struct wc_box floating_geometry;
	struct wc_layout_node *layout_node;
	struct wc_workspace *workspace;
	void *user_data;
};

struct wc_workspace {
	struct wc_layout_node *root;
	struct wc_view *focused_view;
	struct wc_layout_config config;
	enum wc_layout_type layout_type;
	struct wc_box last_usable_area;
	bool has_last_usable_area;
};

void layout_init_config(struct wc_layout_config *config);
void layout_init_view(struct wc_view *view, unsigned int id);
void layout_init_workspace(struct wc_workspace *ws);
void layout_finish_workspace(struct wc_workspace *ws);
void layout_set_type(struct wc_workspace *ws, enum wc_layout_type type);
enum wc_layout_type layout_get_type(const struct wc_workspace *ws);
void layout_add_view(struct wc_workspace *ws, struct wc_view *view);
void layout_remove_view(struct wc_workspace *ws, struct wc_view *view);
void layout_focus_view(struct wc_workspace *ws, struct wc_view *view);
void layout_arrange(struct wc_workspace *ws, struct wc_box usable_area);
void layout_resize_focused(struct wc_workspace *ws, float delta);
void layout_toggle_split(struct wc_workspace *ws);
void layout_swap_focused_with_master_or_root(struct wc_workspace *ws);
void layout_swap_focused_neighbor(struct wc_workspace *ws, int direction);
void layout_cycle_next(struct wc_workspace *ws);
void layout_cycle_prev(struct wc_workspace *ws);
int layout_focus_direction(struct wc_workspace *ws, enum wc_direction direction);
int layout_validate_tree(const struct wc_workspace *ws);
void layout_debug_print_tree(const struct wc_workspace *ws, FILE *fp);

#endif
