#include <stdio.h>

#include "lead.h"
#include "header.h"
#include "rpm.h"

// RPM file
int rpm_init(struct rpm *rpm, int fd)
{
	lead_init(&rpm->lead, fd);
	header_read_first(&rpm->sighdr, fd);
	header_read_next(&rpm->taghdr, fd, &rpm->sighdr);
	rpm->arcofs = rpm->taghdr.storeofs + rpm->taghdr.datalen;
	return 0;
}

void rpm_dump(const struct rpm *rpm, FILE *f)
{
	lead_dump(&rpm->lead, f);
	fprintf(f, "\n");
	header_dump(&rpm->sighdr, f);
	fprintf(f, "\n");
	header_dump(&rpm->taghdr, f);
	fprintf(f, "\n");
	fprintf(f, "== Archive ==\nOffset: 0x%lx\n", rpm->arcofs);
}
