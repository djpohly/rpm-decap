#ifndef HEADER_H_
#define HEADER_H_

#include <stdio.h>
#include <stdint.h>

#include "list.h"

// Forward-declare
struct rpm;

// On-disk structures
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

// In-memory structures
struct header {
	const struct rpm *rpm;
	uint32_t entries;
	uint32_t datalen;
	uint32_t ofs;
	uint32_t idxofs;
	uint32_t storeofs;
	struct list entrylist;
};

struct entry {
	const struct header *hdr;
	uint32_t tag;
	uint32_t type;
	int32_t dataofs;
	uint32_t count;
};

int entry_init(struct entry *ent, const struct header *hdr, int i);
void entry_destroy(struct entry *ent);
int entry_dump(const struct entry *ent, FILE *f);

int header_init_first(struct header *hdr, const struct rpm *rpm);
int header_init_next(struct header *hdr, const struct rpm *rpm,
		const struct header *prev);
void header_destroy(struct header *hdr);
void header_dump(const struct header *hdr, FILE *f);

#endif
