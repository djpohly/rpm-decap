#ifndef RPM_H_
#define RPM_H_

#include <stdio.h>
#include <sys/types.h>

#include "lead.h"
#include "header.h"

// In-memory structure
struct rpm {
	struct lead lead;
	struct header sighdr;
	struct header taghdr;
	off_t arcofs;
};

int rpm_init(struct rpm *rpm, int fd);
void rpm_destroy(struct rpm *rpm);
off_t rpm_write(const struct rpm *rpm, int arcfd, int fd);

#endif
