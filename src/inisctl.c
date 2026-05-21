#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define INISCTL_BUFSIZE 4096

static const char *
socket_path(void)
{
	const char *path;
	const char *runtime;
	static char fallback[512];

	path = getenv("INIS_SOCKET");
	if (path != NULL && path[0] != '\0')
		return path;

	runtime = getenv("XDG_RUNTIME_DIR");
	if (runtime == NULL || runtime[0] == '\0')
		runtime = "/tmp";
	snprintf(fallback, sizeof(fallback), "%s/inis-0.sock", runtime);
	return fallback;
}

static int
write_all(int fd, const char *buf, size_t len)
{
	size_t off = 0;

	while (off < len) {
		ssize_t n = write(fd, buf + off, len - off);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		off += (size_t)n;
	}
	return 0;
}

static void
usage(const char *argv0)
{
	printf(
	    "usage: %s dispatch <name> [args...]\n"
	    "       %s clients|monitors|workspaces|activewindow|damage|backend|reload\n"
	    "\n"
	    "examples:\n"
	    "       %s dispatch workspace 2\n"
	    "       %s dispatch togglespecialworkspace magic\n",
	    argv0, argv0, argv0, argv0);
}

static bool
is_help_arg(const char *arg)
{
	return strcmp(arg, "-h") == 0 ||
	    strcmp(arg, "--help") == 0 ||
	    strcmp(arg, "help") == 0;
}

static bool
is_version_arg(const char *arg)
{
	return strcmp(arg, "-v") == 0 ||
	    strcmp(arg, "--version") == 0 ||
	    strcmp(arg, "version") == 0;
}

static void
version(void)
{
	printf("inisctl 0.0.0-dev\n");
}

static bool
is_known_command(const char *arg)
{
	return strcmp(arg, "dispatch") == 0 ||
	    strcmp(arg, "clients") == 0 ||
	    strcmp(arg, "monitors") == 0 ||
	    strcmp(arg, "workspaces") == 0 ||
	    strcmp(arg, "activewindow") == 0 ||
	    strcmp(arg, "damage") == 0 ||
	    strcmp(arg, "backend") == 0 ||
	    strcmp(arg, "reload") == 0;
}

static void
unknown_command(const char *argv0, const char *arg)
{
	fprintf(stderr,
	    "%s: unknown command: %s\n"
	    "run `%s --help` for usage\n",
	    argv0, arg, argv0);
}

int
main(int argc, char **argv)
{
	struct sockaddr_un addr;
	char command[INISCTL_BUFSIZE];
	char buf[INISCTL_BUFSIZE];
	int fd;
	ssize_t n;
	int i;

	if (argc < 2) {
		usage(argv[0]);
		return 2;
	}
	if (is_help_arg(argv[1])) {
		usage(argv[0]);
		return 0;
	}
	if (is_version_arg(argv[1])) {
		version();
		return 0;
	}
	if (!is_known_command(argv[1])) {
		unknown_command(argv[0], argv[1]);
		return 2;
	}

	command[0] = '\0';
	for (i = 1; i < argc; i++) {
		if (i > 1)
			strncat(command, " ", sizeof(command) - strlen(command) - 1);
		strncat(command, argv[i], sizeof(command) - strlen(command) - 1);
	}
	strncat(command, "\n", sizeof(command) - strlen(command) - 1);

	fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		fprintf(stderr, "inisctl: socket: %s\n", strerror(errno));
		return 1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path());
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		fprintf(stderr, "inisctl: connect %s: %s\n", socket_path(), strerror(errno));
		close(fd);
		return 1;
	}

	if (write_all(fd, command, strlen(command)) != 0) {
		fprintf(stderr, "inisctl: write: %s\n", strerror(errno));
		close(fd);
		return 1;
	}

	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		if (write_all(STDOUT_FILENO, buf, (size_t)n) != 0) {
			fprintf(stderr, "inisctl: stdout: %s\n", strerror(errno));
			close(fd);
			return 1;
		}
	}

	close(fd);
	return 0;
}
