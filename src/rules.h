#ifndef INIS_RULES_H
#define INIS_RULES_H

#include "inis.h"

enum inis_rule_action {
	INIS_RULE_FLOAT,
	INIS_RULE_TILE,
	INIS_RULE_FULLSCREEN,
	INIS_RULE_CENTER,
	INIS_RULE_WORKSPACE,
	INIS_RULE_MONITOR,
	INIS_RULE_SIZE,
	INIS_RULE_MOVE,
	INIS_RULE_NOBORDER,
	INIS_RULE_NOANIM
};

struct inis_rule {
	enum inis_rule_action action;
	char key[INIS_MAX_NAME];
	char value[INIS_MAX_NAME];
	char args[INIS_MAX_ARGS];
};

int inis_rule_parse_line(struct inis_rule *rule, const char *line);
bool inis_rule_matches(const struct inis_rule *rule, const struct inis_window *window);

#endif
