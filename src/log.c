#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static enum inis_log_level min_level = INIS_LOG_INFO;

static const char *
level_name(enum inis_log_level level)
{
	switch (level) {
	case INIS_LOG_DEBUG: return "debug";
	case INIS_LOG_INFO: return "info";
	case INIS_LOG_WARN: return "warn";
	case INIS_LOG_ERROR: return "error";
	}
	return "unknown";
}

void
inis_log_set_level(enum inis_log_level level)
{
	min_level = level;
}

void
inis_log(enum inis_log_level level, const char *fmt, ...)
{
	time_t now;
	struct tm *tm;
	char stamp[32];
	va_list ap;

	if (level < min_level)
		return;

	now = time(NULL);
	tm = localtime(&now);
	if (tm != NULL)
		strftime(stamp, sizeof(stamp), "%H:%M:%S", tm);
	else
		snprintf(stamp, sizeof(stamp), "--:--:--");

	fprintf(stderr, "[%s] inis %s: ", stamp, level_name(level));
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}
