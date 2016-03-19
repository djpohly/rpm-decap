#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <endian.h>

#include "list.h"


// On-disk structures
struct lead_f {
	char magic[4];
	char major;
	char minor;
	uint16_t type;
	uint16_t arch;
	char name[66];
	uint16_t os;
	uint16_t sigtype;
	char reserved[16];
} __attribute__((packed));

struct entry_f {
	uint32_t tag;
	uint32_t type;
	uint32_t dataofs;
	uint32_t count;
} __attribute__((packed));

struct header_f {
	char magic[3];
	char version;
	char reserved[4];
	uint32_t entries;
	uint32_t datalen;
} __attribute__((packed));

struct lead {
	char major;
	char minor;
	uint16_t type;
	uint16_t arch;
	char name[66];
	uint16_t os;
	uint16_t sigtype;
};


// In-memory structures
struct entry {
	uint32_t tag;
	uint32_t type;
	int32_t dataofs;
	uint32_t count;
};

struct header {
	uint32_t entries;
	uint32_t datalen;
	uint32_t ofs;
	uint32_t idxofs;
	uint32_t storeofs;
	struct list entrylist;
};

struct rpm {
	struct lead lead;
	struct header sighdr;
	struct header taghdr;
	off_t arcofs;
};


// Lead
static int lead_init(struct lead *lead, int fd)
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

static void lead_dump(const struct lead *lead, FILE *f)
{
	fprintf(f, "== Lead ==\n");
	fprintf(f, "Version: %d.%d\n", lead->major, lead->minor);
	fprintf(f, "Type: %d\n", lead->type);
	fprintf(f, "Architecture: %d\n", lead->arch);
	fprintf(f, "Package name: %-65s\n", lead->name);
	fprintf(f, "OS: %d\n", lead->os);
	fprintf(f, "Signature type: %d\n", lead->sigtype);
}


// Index entries
static int entry_init(struct entry *ent, int fd, const struct header *hdr, int i)
{
	struct entry_f ef;
	pread(fd, &ef, sizeof(ef), hdr->idxofs + i * sizeof(struct entry_f));
	ent->tag = be32toh(ef.tag);
	ent->type = be32toh(ef.type);
	ent->dataofs = hdr->storeofs + (int32_t) be32toh(ef.dataofs);
	ent->count = be32toh(ef.count);
	return 0;
}

static int entry_dump(const struct entry *ent, FILE *f)
{
	fprintf(f, "tag %d: type %d count %d, data 0x%lx\n", ent->tag, ent->type,
			ent->count, ent->dataofs);
}


// Header blocks
static int _header_read_common(struct header *hdr, int fd, off_t ofs)
{
	struct header_f hf;
	pread(fd, &hf, sizeof(hf), ofs);
	hdr->entries = be32toh(hf.entries);
	hdr->datalen = be32toh(hf.datalen);
	hdr->ofs = ofs;
	hdr->idxofs = ofs + sizeof(struct header_f);
	hdr->storeofs = hdr->idxofs + hdr->entries * sizeof(struct entry_f);

	// Read entries
	list_init(&hdr->entrylist);

	int i;
	for (i = 0; i < hdr->entries; i++) {
		struct entry *ent = malloc(sizeof(*ent));
		entry_init(ent, fd, hdr, i);
		list_append(&hdr->entrylist, ent);
	}
	return 0;
}

static int header_read_first(struct header *hdr, int fd)
{
	return _header_read_common(hdr, fd, sizeof(struct lead_f));
}

static int header_read_next(struct header *hdr, int fd, const struct header *prev)
{
	off_t ofs = prev->storeofs + prev->datalen;
	ofs = ((ofs + 7) / 8) * 8;
	return _header_read_common(hdr, fd, ofs);
}

static void header_dump(const struct header *hdr, FILE *f)
{
	fprintf(f, "== Header ==\n");
	fprintf(f, "Offset: 0x%lx\n", hdr->ofs);
	fprintf(f, "Index offset: 0x%lx\n", hdr->idxofs);
	fprintf(f, "Store offset: 0x%lx\n", hdr->storeofs);
	fprintf(f, "Store length: 0x%lx\n", hdr->datalen);
	fprintf(f, " -- Entries (%d) --\n", hdr->entries);

	struct listnode *n;
	for (n = hdr->entrylist.head; n; n = n->next)
		entry_dump(n->data, f);
}


// RPM file
static int rpm_init(struct rpm *rpm, int fd)
{
	lead_init(&rpm->lead, fd);
	header_read_first(&rpm->sighdr, fd);
	header_read_next(&rpm->taghdr, fd, &rpm->sighdr);
	rpm->arcofs = rpm->taghdr.storeofs + rpm->taghdr.datalen;
	return 0;
}

static void rpm_dump(const struct rpm *rpm, FILE *f)
{
	lead_dump(&rpm->lead, f);
	fprintf(f, "\n");
	header_dump(&rpm->sighdr, f);
	fprintf(f, "\n");
	header_dump(&rpm->taghdr, f);
	fprintf(f, "\n");
	fprintf(f, "== Archive ==\nOffset: 0x%lx\n", rpm->arcofs);
}


int main(int argc, char **argv)
{
	int rv;
	char buf[BUFSIZ];

	if (argc < 3) {
		fprintf(stderr, "Usage: %s INFILE OUTFILE\n", argv[0]);
		return 1;
	}

	// Open input file
	int in = open(argv[1], O_RDONLY);
	if (in < 0) {
		perror("open");
		return 1;
	}

	// Load input file information
	struct rpm rpm;
	if (rpm_init(&rpm, in)) {
		close(in);
		return 1;
	}

	printf("RPM %s loaded\n\n", argv[1]);
	rpm_dump(&rpm, stdout);

	// Open output file
	int out = open(argv[2], O_WRONLY | O_TRUNC | O_CREAT, 0644);
	if (out < 0) {
		perror("open");
		close(in);
		return 1;
	}

	close(out);
	close(in);

	return 0;
}
