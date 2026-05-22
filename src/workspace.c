#include "workspace.h"

#include <stdio.h>
#include <string.h>

void
inis_workspace_init(struct inis_workspace *workspace, const char *name, bool special)
{
	memset(workspace, 0, sizeof(*workspace));
	snprintf(workspace->name, sizeof(workspace->name), "%s", name);
	workspace->special = special;
	workspace->visible = !special;
	layout_init_workspace(&workspace->layout);
}

void
inis_workspace_finish(struct inis_workspace *workspace)
{
	layout_finish_workspace(&workspace->layout);
}
