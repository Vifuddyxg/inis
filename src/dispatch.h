#ifndef INIS_DISPATCH_H
#define INIS_DISPATCH_H

#include "inis.h"

struct inis_dispatcher {
	const char *name;
	void (*run)(struct inis_server *server, const char *args);
};

int inis_dispatch(struct inis_server *server, const char *name, const char *args);

#endif
