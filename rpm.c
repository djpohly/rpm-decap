#include <stdio.h>
#include <unistd.h>

#include "lead.h"
#include "header.h"
#include "rpm.h"

// RPM file
static int copy_to_eof(int out, int in)
{
	int to_eof = 1;
	int b = 1;
	while (b > 0) {
		char buf[BUFSIZ];
		b = read(in, buf, BUFSIZ);
		if (b < 0) {
			perror("read");
			return 1;
		}
		write(out, buf, b);
	}

	return 0;
}

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

off_t rpm_write(const struct rpm *rpm, int arcfd, int fd)
{
	off_t ofs = 0;
	ofs = lead_write(&rpm->lead, fd, ofs);
	ofs = header_write(&rpm->sighdr, fd, ofs);
	ofs = header_write(&rpm->taghdr, fd, ofs);
	lseek(fd, ofs, SEEK_SET);
	copy_to_eof(fd, arcfd);

	return ofs;
}
