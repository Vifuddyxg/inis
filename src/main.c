#include "log.h"
#include "server.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* inis_server is too large (~150 KB) to safely place on the stack — heap only */

#define INIS_VERSION "0.0.0-dev"

static void
usage(const char *argv0)
{
	fprintf(stderr,
	    "usage: %s [-d]\n"
	    "       %s --version\n"
	    "       %s --features\n",
	    argv0, argv0, argv0);
}

static void
features(void)
{
	printf("inis %s\n", INIS_VERSION);
	printf("neuswc=yes\n");
	printf("swc=yes\n");
	printf("ipc=yes\n");
	printf("wlroots=no\n");
}

static int
reexec_via_swc_launch(int argc, char **argv)
{
	char **launch_argv;
	int i;

	launch_argv = malloc(((size_t)argc + 3) * sizeof(char *));
	if (launch_argv == NULL)
		return -1;

	launch_argv[0] = "swc-launch";
	launch_argv[1] = "--";
	for (i = 0; i < argc; i++)
		launch_argv[i + 2] = argv[i];
	launch_argv[argc + 2] = NULL;
	execvp("swc-launch", launch_argv);
	free(launch_argv);
	return -1;
}

int
main(int argc, char **argv)
{
	struct inis_server *server;
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-d") == 0) {
			inis_log_set_level(INIS_LOG_DEBUG);
		} else if (strcmp(argv[i], "-h") == 0) {
			usage(argv[0]);
			return 0;
		} else if (strcmp(argv[i], "--version") == 0) {
			printf("inis %s\n", INIS_VERSION);
			return 0;
		} else if (strcmp(argv[i], "--features") == 0) {
			features();
			return 0;
		} else {
			usage(argv[0]);
			return 2;
		}
	}

	/*
	 * swc requires being launched via the swc-launch setuid helper so it
	 * can open DRM/input devices.  If SWC_LAUNCH_SOCKET is not set we are
	 * not running under swc-launch, so re-exec ourselves through it.
	 */
	if (getenv("SWC_LAUNCH_SOCKET") == NULL) {
		reexec_via_swc_launch(argc, argv);
		fprintf(stderr,
		    "inis: could not exec swc-launch: %s\n"
		    "inis: install setuid helper with: sudo make install-launch\n"
		    "inis: or run directly as: swc-launch -- inis\n",
		    strerror(errno));
		return 1;
	}

	server = calloc(1, sizeof(*server));
	if (server == NULL) {
		fprintf(stderr, "inis: out of memory\n");
		return 1;
	}

	if (inis_server_init(server) != 0) {
		free(server);
		return 1;
	}

	if (inis_server_run(server) != 0) {
		inis_server_shutdown(server);
		free(server);
		return 1;
	}

	inis_server_shutdown(server);
	free(server);
	return 0;
}
