#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <endian.h>
#include <sys/types.h>

#include "lead.h"

// Lead
off_t lead_init(struct lead *lead, int fd, off_t ofs)
{
	struct lead_f lf;
	pread(fd, &lf, sizeof(lf), ofs);
	lead->major = lf.major;
	lead->minor = lf.minor;
	lead->type = be16toh(lf.type);
	lead->arch = be16toh(lf.arch);
	memcpy(lead->name, lf.name, 66);
	lead->os = be16toh(lf.os);
	lead->sigtype = be16toh(lf.sigtype);

	return ofs + sizeof(lf);
}

void lead_destroy(struct lead *lead)
{
}

off_t lead_write(const struct lead *lead, int fd, off_t ofs)
{
	// Set up on-disk structure
	struct lead_f lf;
	memcpy(lf.magic, LEAD_MAGIC, sizeof(lf.magic));
	lf.major = lead->major;
	lf.minor = lead->minor;
	lf.type = htobe16(lead->type);
	lf.arch = htobe16(lead->arch);
	memset(lf.name, 0, sizeof(lf.name));
	strncpy(lf.name, lead->name, sizeof(lf.name));
	lf.os = htobe16(lead->os);
	lf.sigtype = htobe16(lead->sigtype);
	memset(lf.reserved, 0, sizeof(lf.reserved));

	pwrite(fd, &lf, sizeof(lf), ofs);

	return ofs + sizeof(lf);
}
