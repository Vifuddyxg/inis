#include "layout.h"

#include "inis.h"

#include <stdlib.h>
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

static float
clamp_ratio(const struct wc_layout_config *config, float ratio)
{
	if (ratio < config->min_split_ratio)
		return config->min_split_ratio;
	if (ratio > config->max_split_ratio)
		return config->max_split_ratio;
	return ratio;
}

static struct wc_box
box_inset(struct wc_box box, int amount)
{
	if (amount <= 0)
		return box;

	box.x += amount;
	box.y += amount;
	box.w -= amount * 2;
	box.h -= amount * 2;
	box.w = max_int(box.w, 1);
	box.h = max_int(box.h, 1);
	return box;
}

static enum wc_split
choose_split_from_box(struct wc_box box)
{
	if (box.w > box.h)
		return WC_SPLIT_VERTICAL;
	return WC_SPLIT_HORIZONTAL;
}

static struct wc_box
split_primary_box(struct wc_box box, enum wc_split split, int primary,
    int gap)
{
	struct wc_box child = box;
	int gap_a;

	gap_a = gap / 2;
	if (split == WC_SPLIT_VERTICAL)
		child.w = max_int(primary - gap_a, 1);
	else
		child.h = max_int(primary - gap_a, 1);
	return child;
}

static struct wc_box
split_secondary_box(struct wc_box box, enum wc_split split, int primary,
    int gap)
{
	struct wc_box child = box;
	int gap_b;

	gap_b = gap - gap / 2;
	if (split == WC_SPLIT_VERTICAL) {
		child.x += primary + gap_b;
		child.w = max_int(box.w - primary - gap_b, 1);
	} else {
		child.y += primary + gap_b;
		child.h = max_int(box.h - primary - gap_b, 1);
	}
	return child;
}

static struct wc_layout_node *
first_leaf(struct wc_layout_node *node)
{
	if (node == NULL)
		return NULL;
	while (!node->is_leaf && node->left != NULL)
		node = node->left;
	return node;
}

static struct wc_layout_node *
create_leaf(struct wc_view *view)
{
	struct wc_layout_node *node;

	node = calloc(1, sizeof(*node));
	if (node == NULL)
		return NULL;

	node->is_leaf = true;
	node->view = view;
	node->split = WC_SPLIT_AUTO;
	node->ratio = 0.5f;
	return node;
}

static struct wc_layout_node *
create_branch(enum wc_split split, float ratio)
{
	struct wc_layout_node *node;

	node = calloc(1, sizeof(*node));
	if (node == NULL)
		return NULL;

	node->split = split;
	node->ratio = ratio;
	return node;
}

static void
destroy_tree(struct wc_layout_node *node)
{
	if (node == NULL)
		return;
	destroy_tree(node->left);
	destroy_tree(node->right);
	if (node->is_leaf && node->view != NULL) {
		node->view->layout_node = NULL;
		node->view->workspace = NULL;
		node->view->tiled_visible = false;
	}
	free(node);
}

static void
swap_leaf_views(struct wc_layout_node *a, struct wc_layout_node *b)
{
	struct wc_view *tmp;

	if (a == NULL || b == NULL || !a->is_leaf || !b->is_leaf)
		return;

	tmp = a->view;
	a->view = b->view;
	b->view = tmp;

	if (a->view != NULL)
		a->view->layout_node = a;
	if (b->view != NULL)
		b->view->layout_node = b;
}

static void
collect_leaves(struct wc_layout_node *node, struct wc_layout_node **out,
    size_t max, size_t *count)
{
	if (node == NULL || *count >= max)
		return;
	if (node->is_leaf) {
		out[(*count)++] = node;
		return;
	}
	collect_leaves(node->left, out, max, count);
	collect_leaves(node->right, out, max, count);
}

static void
clear_visibility(struct wc_layout_node *node)
{
	if (node == NULL)
		return;
	if (node->is_leaf) {
		if (node->view != NULL)
			node->view->tiled_visible = false;
		return;
	}
	clear_visibility(node->left);
	clear_visibility(node->right);
}

static void
assign_leaf_box(const struct wc_workspace *ws, struct wc_layout_node *node,
    struct wc_box box, bool visible)
{
	if (node == NULL || !node->is_leaf || node->view == NULL)
		return;

	node->box = box;
	node->view->pending_geometry = box_inset(box, ws->config.border_width);
	node->view->tiled_visible = visible;
}

static void
arrange_dwindle_node(struct wc_workspace *ws, struct wc_layout_node *node,
    struct wc_box box)
{
	enum wc_split split;
	int primary;
	struct wc_box left_box;
	struct wc_box right_box;

	if (node == NULL)
		return;

	node->box = box;
	if (node->is_leaf) {
		assign_leaf_box(ws, node, box, true);
		return;
	}

	split = node->split;
	if (split == WC_SPLIT_AUTO)
		split = choose_split_from_box(box);
	else if (ws->config.smart_split && !ws->config.preserve_split &&
	    !node->preserve_split)
		split = choose_split_from_box(box);

	if (split == WC_SPLIT_VERTICAL) {
		primary = (int)((float)box.w * clamp_ratio(&ws->config, node->ratio));
		primary = min_int(max_int(primary, 1), max_int(box.w - 1, 1));
	} else {
		primary = (int)((float)box.h * clamp_ratio(&ws->config, node->ratio));
		primary = min_int(max_int(primary, 1), max_int(box.h - 1, 1));
	}

	left_box = split_primary_box(box, split, primary, ws->config.inner_gap);
	right_box = split_secondary_box(box, split, primary, ws->config.inner_gap);
	arrange_dwindle_node(ws, node->left, left_box);
	arrange_dwindle_node(ws, node->right, right_box);
}

static void
arrange_column(const struct wc_workspace *ws, struct wc_layout_node **nodes,
    size_t count, struct wc_box box, bool visible)
{
	size_t i;
	int gap;
	int each;
	int y;

	if (count == 0)
		return;
	if (count == 1) {
		assign_leaf_box(ws, nodes[0], box, visible);
		return;
	}

	gap = ws->config.inner_gap;
	each = (box.h - (int)(count - 1) * gap) / (int)count;
	if (each < 1)
		each = 1;
	y = box.y;
	for (i = 0; i < count; i++) {
		struct wc_box cell;

		cell.x = box.x;
		cell.y = y;
		cell.w = box.w;
		cell.h = i + 1 == count ? max_int(box.y + box.h - y, 1) : each;
		assign_leaf_box(ws, nodes[i], cell, visible);
		y += each + gap;
	}
}

static void
arrange_master(struct wc_workspace *ws, struct wc_box box)
{
	struct wc_layout_node *nodes[INIS_MAX_WINDOWS];
	size_t count;
	size_t master_count;
	struct wc_box master_box;
	struct wc_box stack_box;
	int master_w;
	int gap_a;
	int gap_b;

	count = 0;
	collect_leaves(ws->root, nodes, INIS_MAX_WINDOWS, &count);
	if (count == 0)
		return;

	master_count = (size_t)max_int(ws->config.master_count, 1);
	if (master_count >= count) {
		arrange_column(ws, nodes, count, box, true);
		return;
	}

	master_w = (int)((float)box.w * ws->config.master_ratio);
	master_w = min_int(max_int(master_w, 1), max_int(box.w - 1, 1));
	gap_a = ws->config.inner_gap / 2;
	gap_b = ws->config.inner_gap - gap_a;

	master_box = box;
	master_box.w = max_int(master_w - gap_a, 1);

	stack_box = box;
	stack_box.x += master_w + gap_b;
	stack_box.w = max_int(box.w - master_w - gap_b, 1);

	arrange_column(ws, nodes, master_count, master_box, true);
	arrange_column(ws, nodes + master_count, count - master_count, stack_box,
	    true);
}

static void
arrange_monocle(struct wc_workspace *ws, struct wc_box box)
{
	struct wc_layout_node *nodes[INIS_MAX_WINDOWS];
	size_t count;
	size_t i;

	count = 0;
	collect_leaves(ws->root, nodes, INIS_MAX_WINDOWS, &count);
	if (count == 0)
		return;

	for (i = 0; i < count; i++)
		assign_leaf_box(ws, nodes[i], box,
		    ws->focused_view == NULL || nodes[i]->view == ws->focused_view);
}

void
layout_init_config(struct wc_layout_config *config)
{
	memset(config, 0, sizeof(*config));
	config->default_layout = WC_LAYOUT_DWINDLE;
	config->outer_gap = 8;
	config->inner_gap = 4;
	config->border_width = 2;
	config->preserve_split = false;
	config->smart_split = true;
	config->default_split_ratio = 0.5f;
	config->min_split_ratio = 0.1f;
	config->max_split_ratio = 0.9f;
	config->master_ratio = 0.55f;
	config->master_count = 1;
	config->new_is_master = false;
}

void
layout_init_view(struct wc_view *view, unsigned int id)
{
	memset(view, 0, sizeof(*view));
	view->id = id;
}

void
layout_init_workspace(struct wc_workspace *ws)
{
	memset(ws, 0, sizeof(*ws));
	layout_init_config(&ws->config);
	ws->layout_type = ws->config.default_layout;
}

void
layout_finish_workspace(struct wc_workspace *ws)
{
	destroy_tree(ws->root);
	ws->root = NULL;
	ws->focused_view = NULL;
	ws->has_last_usable_area = false;
}

void
layout_set_type(struct wc_workspace *ws, enum wc_layout_type type)
{
	ws->layout_type = type;
}

enum wc_layout_type
layout_get_type(const struct wc_workspace *ws)
{
	return ws->layout_type;
}

void
layout_add_view(struct wc_workspace *ws, struct wc_view *view)
{
	struct wc_layout_node *leaf;
	struct wc_layout_node *focus;
	struct wc_layout_node *parent;
	struct wc_layout_node *branch;
	struct wc_box ref_box;
	enum wc_split split;
	bool new_is_left;

	if (ws == NULL || view == NULL || view->layout_node != NULL)
		return;

	leaf = create_leaf(view);
	if (leaf == NULL)
		return;

	view->workspace = ws;
	view->layout_node = leaf;
	if (ws->root == NULL) {
		ws->root = leaf;
		ws->focused_view = view;
		return;
	}

	focus = ws->focused_view != NULL ? ws->focused_view->layout_node : NULL;
	if (focus == NULL || !focus->is_leaf)
		focus = first_leaf(ws->root);
	if (focus == NULL) {
		ws->root = leaf;
		ws->focused_view = view;
		return;
	}

	ref_box = focus->box;
	if (ref_box.w <= 0 || ref_box.h <= 0) {
		if (ws->has_last_usable_area)
			ref_box = ws->last_usable_area;
		else {
			ref_box.x = 0;
			ref_box.y = 0;
			ref_box.w = 800;
			ref_box.h = 600;
		}
	}

	split = choose_split_from_box(ref_box);
	branch = create_branch(split,
	    clamp_ratio(&ws->config, ws->config.default_split_ratio));
	if (branch == NULL) {
		view->workspace = NULL;
		view->layout_node = NULL;
		free(leaf);
		return;
	}

	new_is_left = ws->config.new_is_master;
	parent = focus->parent;
	branch->parent = parent;
	branch->preserve_split = ws->config.preserve_split;
	if (new_is_left) {
		branch->left = leaf;
		branch->right = focus;
	} else {
		branch->left = focus;
		branch->right = leaf;
	}
	branch->left->parent = branch;
	branch->right->parent = branch;

	if (parent == NULL)
		ws->root = branch;
	else if (parent->left == focus)
		parent->left = branch;
	else
		parent->right = branch;

	focus->parent = branch;
	leaf->parent = branch;
	ws->focused_view = view;

	if (ws->config.new_is_master) {
		struct wc_layout_node *master = first_leaf(ws->root);

		if (master != NULL && master != view->layout_node)
			swap_leaf_views(master, view->layout_node);
	}
}

void
layout_remove_view(struct wc_workspace *ws, struct wc_view *view)
{
	struct wc_layout_node *leaf;
	struct wc_layout_node *parent;
	struct wc_layout_node *sibling;
	struct wc_layout_node *grandparent;
	struct wc_layout_node *replacement;

	if (ws == NULL || view == NULL)
		return;

	leaf = view->layout_node;
	if (leaf == NULL || view->workspace != ws)
		return;

	parent = leaf->parent;
	if (parent == NULL) {
		free(leaf);
		ws->root = NULL;
		ws->focused_view = NULL;
		view->layout_node = NULL;
		view->workspace = NULL;
		view->tiled_visible = false;
		return;
	}

	sibling = parent->left == leaf ? parent->right : parent->left;
	grandparent = parent->parent;
	if (grandparent == NULL)
		ws->root = sibling;
	else if (grandparent->left == parent)
		grandparent->left = sibling;
	else
		grandparent->right = sibling;
	sibling->parent = grandparent;

	view->layout_node = NULL;
	view->workspace = NULL;
	view->tiled_visible = false;
	free(leaf);
	free(parent);

	replacement = first_leaf(sibling);
	if (ws->focused_view == view)
		ws->focused_view = replacement != NULL ? replacement->view : NULL;
}

void
layout_focus_view(struct wc_workspace *ws, struct wc_view *view)
{
	if (ws == NULL || view == NULL)
		return;
	if (view->workspace != ws || view->layout_node == NULL)
		return;
	ws->focused_view = view;
}

void
layout_arrange(struct wc_workspace *ws, struct wc_box usable_area)
{
	struct wc_box area;

	if (ws == NULL)
		return;

	ws->last_usable_area = usable_area;
	ws->has_last_usable_area = true;
	if (ws->root == NULL)
		return;
	if (ws->focused_view == NULL) {
		struct wc_layout_node *leaf = first_leaf(ws->root);

		ws->focused_view = leaf != NULL ? leaf->view : NULL;
	}

	clear_visibility(ws->root);
	area = box_inset(usable_area, ws->config.outer_gap);
	switch (ws->layout_type) {
	case WC_LAYOUT_MASTER:
		arrange_master(ws, area);
		break;
	case WC_LAYOUT_MONOCLE:
		arrange_monocle(ws, area);
		break;
	case WC_LAYOUT_DWINDLE:
	default:
		arrange_dwindle_node(ws, ws->root, area);
		break;
	}
}

void
layout_resize_focused(struct wc_workspace *ws, float delta)
{
	struct wc_layout_node *leaf;
	struct wc_layout_node *parent;

	if (ws == NULL || ws->focused_view == NULL)
		return;

	leaf = ws->focused_view->layout_node;
	if (leaf == NULL || leaf->parent == NULL)
		return;

	parent = leaf->parent;
	if (parent->left == leaf)
		parent->ratio = clamp_ratio(&ws->config, parent->ratio + delta);
	else
		parent->ratio = clamp_ratio(&ws->config, parent->ratio - delta);
}

void
layout_toggle_split(struct wc_workspace *ws)
{
	struct wc_layout_node *leaf;
	struct wc_layout_node *parent;
	enum wc_split split;

	if (ws == NULL || ws->focused_view == NULL)
		return;

	leaf = ws->focused_view->layout_node;
	if (leaf == NULL || leaf->parent == NULL)
		return;

	parent = leaf->parent;
	split = parent->split;
	if (split == WC_SPLIT_AUTO)
		split = choose_split_from_box(parent->box);
	parent->split = split == WC_SPLIT_VERTICAL ?
	    WC_SPLIT_HORIZONTAL : WC_SPLIT_VERTICAL;
	parent->preserve_split = true;
}

void
layout_swap_focused_with_master_or_root(struct wc_workspace *ws)
{
	struct wc_layout_node *leaf;
	struct wc_layout_node *master;

	if (ws == NULL || ws->focused_view == NULL)
		return;

	leaf = ws->focused_view->layout_node;
	master = first_leaf(ws->root);
	if (leaf == NULL || master == NULL || leaf == master)
		return;

	swap_leaf_views(leaf, master);
	ws->focused_view = master->view;
}

void
layout_swap_focused_neighbor(struct wc_workspace *ws, int direction)
{
	struct wc_layout_node *nodes[INIS_MAX_WINDOWS];
	struct wc_layout_node *focused_node;
	size_t count;
	size_t i;
	size_t focused_idx;
	size_t swap_idx;

	if (ws == NULL || ws->focused_view == NULL)
		return;

	focused_node = ws->focused_view->layout_node;
	if (focused_node == NULL)
		return;

	count = 0;
	collect_leaves(ws->root, nodes, INIS_MAX_WINDOWS, &count);
	if (count < 2)
		return;

	focused_idx = count;
	for (i = 0; i < count; i++) {
		if (nodes[i] == focused_node) {
			focused_idx = i;
			break;
		}
	}
	if (focused_idx == count)
		return;

	if (direction > 0)
		swap_idx = (focused_idx + 1) % count;
	else
		swap_idx = (focused_idx + count - 1) % count;

	/*
	 * Swap the view pointers between the two leaf nodes.  After this,
	 * ws->focused_view still points to the same wc_view object (the user's
	 * window), which is now located at nodes[swap_idx].
	 */
	swap_leaf_views(nodes[focused_idx], nodes[swap_idx]);
}

void
layout_cycle_next(struct wc_workspace *ws)
{
	struct wc_layout_node *nodes[INIS_MAX_WINDOWS];
	size_t count;
	size_t i;

	if (ws == NULL || ws->root == NULL)
		return;

	count = 0;
	collect_leaves(ws->root, nodes, INIS_MAX_WINDOWS, &count);
	if (count == 0)
		return;
	if (ws->focused_view == NULL) {
		ws->focused_view = nodes[0]->view;
		return;
	}

	for (i = 0; i < count; i++) {
		if (nodes[i]->view != ws->focused_view)
			continue;
		ws->focused_view = nodes[(i + 1) % count]->view;
		return;
	}

	ws->focused_view = nodes[0]->view;
}

void
layout_cycle_prev(struct wc_workspace *ws)
{
	struct wc_layout_node *nodes[INIS_MAX_WINDOWS];
	size_t count;
	size_t i;

	if (ws == NULL || ws->root == NULL)
		return;

	count = 0;
	collect_leaves(ws->root, nodes, INIS_MAX_WINDOWS, &count);
	if (count == 0)
		return;
	if (ws->focused_view == NULL) {
		ws->focused_view = nodes[count - 1]->view;
		return;
	}

	for (i = 0; i < count; i++) {
		if (nodes[i]->view != ws->focused_view)
			continue;
		ws->focused_view = nodes[(i + count - 1) % count]->view;
		return;
	}

	ws->focused_view = nodes[count - 1]->view;
}

int
layout_focus_direction(struct wc_workspace *ws, enum wc_direction direction)
{
	struct wc_layout_node *nodes[INIS_MAX_WINDOWS];
	struct wc_view *focused;
	int cx;
	int cy;
	size_t count;
	size_t i;
	size_t best_index = (size_t)-1;
	int best_primary = 0;
	int best_distance = 0;

	if (ws == NULL || ws->root == NULL)
		return -1;
	if (ws->focused_view == NULL || ws->focused_view->layout_node == NULL)
		return -1;

	focused = ws->focused_view;
	cx = focused->pending_geometry.x + focused->pending_geometry.w / 2;
	cy = focused->pending_geometry.y + focused->pending_geometry.h / 2;
	count = 0;
	collect_leaves(ws->root, nodes, INIS_MAX_WINDOWS, &count);

	for (i = 0; i < count; i++) {
		struct wc_view *candidate;
		int dx;
		int dy;
		int primary;
		int distance;
		bool valid = false;

		candidate = nodes[i]->view;
		if (candidate == NULL || candidate == focused || !candidate->tiled_visible)
			continue;

		dx = candidate->pending_geometry.x +
		    candidate->pending_geometry.w / 2 - cx;
		dy = candidate->pending_geometry.y +
		    candidate->pending_geometry.h / 2 - cy;
		switch (direction) {
		case WC_DIRECTION_LEFT:
			valid = dx < 0;
			primary = -dx;
			break;
		case WC_DIRECTION_RIGHT:
			valid = dx > 0;
			primary = dx;
			break;
		case WC_DIRECTION_UP:
			valid = dy < 0;
			primary = -dy;
			break;
		case WC_DIRECTION_DOWN:
			valid = dy > 0;
			primary = dy;
			break;
		default:
			return -1;
		}
		if (!valid)
			continue;

		distance = dx * dx + dy * dy;
		if (best_index == (size_t)-1 || primary < best_primary ||
		    (primary == best_primary && distance < best_distance)) {
			best_index = i;
			best_primary = primary;
			best_distance = distance;
		}
	}

	if (best_index == (size_t)-1)
		return -1;

	ws->focused_view = nodes[best_index]->view;
	return 0;
}

static int
validate_node(const struct wc_workspace *ws, const struct wc_layout_node *node,
    const struct wc_layout_node *parent, const struct wc_view **seen,
    size_t *seen_count)
{
	size_t i;

	if (node == NULL)
		return 1;
	if (node->parent != parent)
		return 0;
	if (node->is_leaf) {
		if (node->left != NULL || node->right != NULL || node->view == NULL)
			return 0;
		if (node->view->layout_node != node || node->view->workspace != ws)
			return 0;
		for (i = 0; i < *seen_count; i++) {
			if (seen[i] == node->view)
				return 0;
		}
		if (*seen_count >= INIS_MAX_WINDOWS)
			return 0;
		seen[(*seen_count)++] = node->view;
		return 1;
	}

	if (node->left == NULL || node->right == NULL || node->view != NULL)
		return 0;
	return validate_node(ws, node->left, node, seen, seen_count) &&
	    validate_node(ws, node->right, node, seen, seen_count);
}

int
layout_validate_tree(const struct wc_workspace *ws)
{
	const struct wc_view *seen[INIS_MAX_WINDOWS];
	size_t seen_count = 0;

	if (ws == NULL)
		return 0;
	if (ws->root == NULL)
		return 1;
	return validate_node(ws, ws->root, NULL, seen, &seen_count);
}

static const char *
split_name(enum wc_split split)
{
	switch (split) {
	case WC_SPLIT_VERTICAL:
		return "vertical";
	case WC_SPLIT_HORIZONTAL:
		return "horizontal";
	case WC_SPLIT_AUTO:
	default:
		return "auto";
	}
}

static void
debug_print_node(const struct wc_layout_node *node, FILE *fp, int depth)
{
	int i;

	if (node == NULL)
		return;
	for (i = 0; i < depth; i++)
		fputs("  ", fp);
	if (node->is_leaf) {
		fprintf(fp, "leaf view=%u box=%dx%d+%d+%d\n",
		    node->view != NULL ? node->view->id : 0,
		    node->box.w, node->box.h, node->box.x, node->box.y);
		return;
	}

	fprintf(fp, "branch split=%s ratio=%.2f preserve=%d box=%dx%d+%d+%d\n",
	    split_name(node->split), node->ratio, node->preserve_split ? 1 : 0,
	    node->box.w, node->box.h, node->box.x, node->box.y);
	debug_print_node(node->left, fp, depth + 1);
	debug_print_node(node->right, fp, depth + 1);
}

void
layout_debug_print_tree(const struct wc_workspace *ws, FILE *fp)
{
	if (fp == NULL)
		return;
	if (ws == NULL || ws->root == NULL) {
		fputs("(empty)\n", fp);
		return;
	}
	debug_print_node(ws->root, fp, 0);
}

