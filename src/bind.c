#include "bind.h"

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static bool
is_modifier_token(const char *token)
{
	return strcmp(token, "SHIFT") == 0 ||
	    strcmp(token, "CTRL") == 0 ||
	    strcmp(token, "CONTROL") == 0 ||
	    strcmp(token, "ALT") == 0 ||
	    strcmp(token, "SUPER") == 0;
}

static void
append_token(char *dst, size_t dst_size, const char *token)
{
	size_t len;

	if (dst_size == 0 || token == NULL || token[0] == '\0')
		return;

	len = strlen(dst);
	if (len > 0 && len + 1 < dst_size) {
		dst[len++] = '+';
		dst[len] = '\0';
	}
	if (len < dst_size - 1)
		strncat(dst, token, dst_size - len - 1);
}

static void
append_arg(char *dst, size_t dst_size, const char *token)
{
	size_t len;

	if (dst_size == 0 || token == NULL || token[0] == '\0')
		return;

	len = strlen(dst);
	if (len > 0 && len + 1 < dst_size) {
		dst[len++] = ' ';
		dst[len] = '\0';
	}
	if (len < dst_size - 1)
		strncat(dst, token, dst_size - len - 1);
}

int
inis_bind_parse_line(struct inis_binding *binding, const char *line)
{
	char copy[INIS_MAX_LINE];
	char *tokens[32];
	char *tok;
	size_t ntokens = 0;
	size_t i;

	memset(binding, 0, sizeof(*binding));
	snprintf(copy, sizeof(copy), "%s", line);

	tok = strtok(copy, " \t\r\n");
	while (tok != NULL && ntokens < sizeof(tokens) / sizeof(tokens[0])) {
		tokens[ntokens++] = tok;
		tok = strtok(NULL, " \t\r\n");
	}

	if (ntokens < 4)
		return -1;

	if (strcmp(tokens[0], "bindm") == 0)
		binding->mouse = true;
	else if (strcmp(tokens[0], "bind") != 0)
		return -1;

	append_token(binding->mods, sizeof(binding->mods), tokens[1]);

	i = 2;
	while (i < ntokens && is_modifier_token(tokens[i])) {
		append_token(binding->mods, sizeof(binding->mods), tokens[i]);
		i++;
	}

	if (i + 1 >= ntokens)
		return -1;

	snprintf(binding->key, sizeof(binding->key), "%s", tokens[i++]);
	snprintf(binding->dispatcher, sizeof(binding->dispatcher), "%s", tokens[i++]);

	while (i < ntokens) {
		append_arg(binding->args, sizeof(binding->args), tokens[i]);
		i++;
	}

	return 0;
}

static bool
token_in_mods(const char *mods, const char *needle)
{
	char copy[INIS_MAX_NAME];
	char *tok;

	snprintf(copy, sizeof(copy), "%s", mods);
	tok = strtok(copy, "+");
	while (tok != NULL) {
		if (strcmp(tok, needle) == 0)
			return true;
		tok = strtok(NULL, "+");
	}
	return false;
}

unsigned int
inis_bind_modifier_mask(const struct inis_binding *binding)
{
	unsigned int mask = 0;

	if (token_in_mods(binding->mods, "CTRL") ||
	    token_in_mods(binding->mods, "CONTROL"))
		mask |= INIS_BIND_MOD_CTRL;
	if (token_in_mods(binding->mods, "ALT"))
		mask |= INIS_BIND_MOD_ALT;
	if (token_in_mods(binding->mods, "SUPER"))
		mask |= INIS_BIND_MOD_LOGO;
	if (token_in_mods(binding->mods, "SHIFT"))
		mask |= INIS_BIND_MOD_SHIFT;
	if (token_in_mods(binding->mods, "$mainMod"))
		mask |= INIS_BIND_MOD_LOGO;

	return mask;
}

int
inis_bind_numeric_value(const struct inis_binding *binding, unsigned int *value)
{
	const char *key = binding->key;
	char *end;
	long parsed;

	if (binding->mouse) {
		if (strcmp(key, "mouse_up") == 0) {
			*value = INIS_BIND_BUTTON_SCROLL_UP;
			return 0;
		}
		if (strcmp(key, "mouse_down") == 0) {
			*value = INIS_BIND_BUTTON_SCROLL_DOWN;
			return 0;
		}
		if (strncmp(key, "mouse:", 6) != 0)
			return -1;
		parsed = strtol(key + 6, &end, 10);
		if (*end != '\0' || parsed < 0)
			return -1;
		*value = (unsigned int)parsed;
		return 0;
	}

	if (strlen(key) == 1) {
		*value = (unsigned int)tolower((unsigned char)key[0]);
		return 0;
	}

	if (strcmp(key, "Return") == 0 || strcmp(key, "Enter") == 0) {
		*value = INIS_BIND_KEY_RETURN;
		return 0;
	}
	if (strcmp(key, "Escape") == 0 || strcmp(key, "Esc") == 0) {
		*value = INIS_BIND_KEY_ESCAPE;
		return 0;
	}
	if (strcmp(key, "Left") == 0) {
		*value = INIS_BIND_KEY_LEFT;
		return 0;
	}
	if (strcmp(key, "Right") == 0) {
		*value = INIS_BIND_KEY_RIGHT;
		return 0;
	}
	if (strcmp(key, "Up") == 0) {
		*value = INIS_BIND_KEY_UP;
		return 0;
	}
	if (strcmp(key, "Down") == 0) {
		*value = INIS_BIND_KEY_DOWN;
		return 0;
	}
	if (strcmp(key, "Tab") == 0) {
		*value = 0xff09;
		return 0;
	}
	if (strcmp(key, "BackSpace") == 0) {
		*value = 0xff08;
		return 0;
	}
	if (strcmp(key, "Delete") == 0) {
		*value = 0xffff;
		return 0;
	}
	if (strcmp(key, "Space") == 0) {
		*value = 0x0020;
		return 0;
	}
	if (strcmp(key, "F1")  == 0) { *value = 0xffbe; return 0; }
	if (strcmp(key, "F2")  == 0) { *value = 0xffbf; return 0; }
	if (strcmp(key, "F3")  == 0) { *value = 0xffc0; return 0; }
	if (strcmp(key, "F4")  == 0) { *value = 0xffc1; return 0; }
	if (strcmp(key, "F5")  == 0) { *value = 0xffc2; return 0; }
	if (strcmp(key, "F6")  == 0) { *value = 0xffc3; return 0; }
	if (strcmp(key, "F7")  == 0) { *value = 0xffc4; return 0; }
	if (strcmp(key, "F8")  == 0) { *value = 0xffc5; return 0; }
	if (strcmp(key, "F9")  == 0) { *value = 0xffc6; return 0; }
	if (strcmp(key, "F10") == 0) { *value = 0xffc7; return 0; }
	if (strcmp(key, "F11") == 0) { *value = 0xffc8; return 0; }
	if (strcmp(key, "F12") == 0) { *value = 0xffc9; return 0; }
	if (strcmp(key, "Print") == 0 || strcmp(key, "PrintScreen") == 0) {
		*value = 0xff61;
		return 0;
	}

	return -1;
}
