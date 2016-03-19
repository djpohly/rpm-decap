#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <endian.h>
#include <sys/types.h>

#include "lead.h"

// Lead
int lead_init(struct lead *lead, int fd)
{
	struct lead_f lf;
	pread(fd, &lf, sizeof(lf), 0);
	lead->major = lf.major;
	lead->minor = lf.minor;
	lead->type = be16toh(lf.type);
	lead->arch = be16toh(lf.arch);
	memcpy(lead->name, lf.name, 66);
	lead->os = be16toh(lf.os);
	lead->sigtype = be16toh(lf.sigtype);

	return 0;
}

void lead_destroy(struct lead *lead)
{
}

void lead_dump(const struct lead *lead, FILE *f)
{
	fprintf(f, "== Lead ==\n");
	fprintf(f, "Version: %d.%d\n", lead->major, lead->minor);
	fprintf(f, "Type: %d\n", lead->type);
	fprintf(f, "Architecture: %d\n", lead->arch);
	fprintf(f, "Package name: %-65s\n", lead->name);
	fprintf(f, "OS: %d\n", lead->os);
	fprintf(f, "Signature type: %d\n", lead->sigtype);
}
