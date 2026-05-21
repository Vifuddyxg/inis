#ifndef INIS_BIND_H
#define INIS_BIND_H

#include "inis.h"

struct inis_binding {
	char mods[INIS_MAX_NAME];
	char key[INIS_MAX_NAME];
	char dispatcher[INIS_MAX_NAME];
	char args[INIS_MAX_ARGS];
	bool mouse;
};

int inis_bind_parse_line(struct inis_binding *binding, const char *line);
unsigned int inis_bind_modifier_mask(const struct inis_binding *binding);
int inis_bind_numeric_value(const struct inis_binding *binding,
    unsigned int *value);

#endif
