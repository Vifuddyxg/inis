#include "rules.h"

#include "window.h"

#include <stdio.h>
#include <string.h>

static int
parse_action(const char *name, enum inis_rule_action *action)
{
	if (strcmp(name, "float") == 0)
		*action = INIS_RULE_FLOAT;
	else if (strcmp(name, "tile") == 0)
		*action = INIS_RULE_TILE;
	else if (strcmp(name, "fullscreen") == 0)
		*action = INIS_RULE_FULLSCREEN;
	else if (strcmp(name, "center") == 0)
		*action = INIS_RULE_CENTER;
	else if (strcmp(name, "workspace") == 0)
		*action = INIS_RULE_WORKSPACE;
	else if (strcmp(name, "monitor") == 0)
		*action = INIS_RULE_MONITOR;
	else if (strcmp(name, "size") == 0)
		*action = INIS_RULE_SIZE;
	else if (strcmp(name, "move") == 0)
		*action = INIS_RULE_MOVE;
	else if (strcmp(name, "noborder") == 0)
		*action = INIS_RULE_NOBORDER;
	else if (strcmp(name, "noanim") == 0)
		*action = INIS_RULE_NOANIM;
	else
		return -1;
	return 0;
}

int
inis_rule_parse_line(struct inis_rule *rule, const char *line)
{
	char copy[INIS_MAX_LINE];
	char *tokens[8];
	char *tok;
	char *colon;
	size_t ntokens = 0;
	size_t matcher_index;

	memset(rule, 0, sizeof(*rule));
	snprintf(copy, sizeof(copy), "%s", line);

	tok = strtok(copy, " \t\r\n");
	while (tok != NULL && ntokens < sizeof(tokens) / sizeof(tokens[0])) {
		tokens[ntokens++] = tok;
		tok = strtok(NULL, " \t\r\n");
	}

	if (ntokens < 3 || strcmp(tokens[0], "windowrule") != 0)
		return -1;
	if (parse_action(tokens[1], &rule->action) != 0)
		return -1;

	if (rule->action == INIS_RULE_WORKSPACE ||
	    rule->action == INIS_RULE_MONITOR) {
		if (ntokens < 4)
			return -1;
		snprintf(rule->args, sizeof(rule->args), "%s", tokens[2]);
		matcher_index = 3;
	} else if (rule->action == INIS_RULE_SIZE ||
	    rule->action == INIS_RULE_MOVE) {
		if (ntokens < 5)
			return -1;
		snprintf(rule->args, sizeof(rule->args), "%s %s", tokens[2], tokens[3]);
		matcher_index = 4;
	} else {
		matcher_index = 2;
	}

	colon = strchr(tokens[matcher_index], ':');
	if (colon == NULL)
		return -1;
	*colon = '\0';
	snprintf(rule->key, sizeof(rule->key), "%s", tokens[matcher_index]);
	snprintf(rule->value, sizeof(rule->value), "%s", colon + 1);
	return 0;
}

bool
inis_rule_matches(const struct inis_rule *rule, const struct inis_window *window)
{
	if (strcmp(rule->key, "app_id") == 0)
		return strcmp(rule->value, window->app_id) == 0;
	if (strcmp(rule->key, "title") == 0)
		return strcmp(rule->value, window->title) == 0;
	return false;
}
