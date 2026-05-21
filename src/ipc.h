#ifndef INIS_IPC_H
#define INIS_IPC_H

#include "inis.h"

struct inis_ipc {
	char socket_name[INIS_MAX_NAME];
	char socket_path[INIS_MAX_ARGS];
	int fd;
	bool enabled;
};

void inis_ipc_init(struct inis_ipc *ipc, const char *socket_name);
int inis_ipc_start(struct inis_server *server);
int inis_ipc_get_fd(const struct inis_ipc *ipc);
void inis_ipc_accept(struct inis_server *server);
void inis_ipc_finish(struct inis_ipc *ipc);

#endif
