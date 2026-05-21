#ifndef INIS_LOG_H
#define INIS_LOG_H

enum inis_log_level {
	INIS_LOG_DEBUG,
	INIS_LOG_INFO,
	INIS_LOG_WARN,
	INIS_LOG_ERROR
};

void inis_log_set_level(enum inis_log_level level);
void inis_log(enum inis_log_level level, const char *fmt, ...);

#define inis_debug(...) inis_log(INIS_LOG_DEBUG, __VA_ARGS__)
#define inis_info(...) inis_log(INIS_LOG_INFO, __VA_ARGS__)
#define inis_warn(...) inis_log(INIS_LOG_WARN, __VA_ARGS__)
#define inis_error(...) inis_log(INIS_LOG_ERROR, __VA_ARGS__)

#endif
