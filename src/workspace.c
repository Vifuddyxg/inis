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
}
