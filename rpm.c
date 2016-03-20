#include <stdio.h>

#include "lead.h"
#include "header.h"
#include "rpm.h"

// RPM file
int rpm_init(struct rpm *rpm, int fd)
{
	off_t ofs = 0;
	ofs = lead_init(&rpm->lead, fd, ofs);
	ofs = header_init(&rpm->sighdr, fd, ofs);
	ofs = header_init(&rpm->taghdr, fd, ofs);
	rpm->arcofs = ofs;
	return 0;
}

void rpm_destroy(struct rpm *rpm)
{
	header_destroy(&rpm->taghdr);
	header_destroy(&rpm->sighdr);
	lead_destroy(&rpm->lead);
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

off_t rpm_write(const struct rpm *rpm, int fd)
{
	off_t ofs = 0;
	ofs = lead_write(&rpm->lead, fd, ofs);
	ofs = header_write(&rpm->sighdr, fd, ofs);

	return ofs;
}
