#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <endian.h>

#include "list.h"

#include "lead.h"
#include "header.h"

// Index entries
int entry_init(struct entry *ent, int fd, const struct header *hdr, int i)
{
	struct entry_f ef;
	pread(fd, &ef, sizeof(ef), hdr->idxofs + i * sizeof(struct entry_f));
	ent->tag = be32toh(ef.tag);
	ent->type = be32toh(ef.type);
	ent->dataofs = hdr->storeofs + (int32_t) be32toh(ef.dataofs);
	ent->count = be32toh(ef.count);
	return 0;
}

void entry_destroy(struct entry *ent)
{
}

int entry_dump(const struct entry *ent, FILE *f)
{
	fprintf(f, "tag %d: type %d count %d, data 0x%lx\n", ent->tag, ent->type,
			ent->count, ent->dataofs);
}


// Header blocks
static int header_init_common(struct header *hdr, int fd, off_t ofs)
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

int header_init_first(struct header *hdr, int fd)
{
	return header_init_common(hdr, fd, sizeof(struct lead_f));
}

int header_init_next(struct header *hdr, int fd, const struct header *prev)
{
	off_t ofs = prev->storeofs + prev->datalen;
	ofs = ((ofs + 7) / 8) * 8;
	return header_init_common(hdr, fd, ofs);
}

void header_destroy(struct header *hdr)
{
	struct listnode *n;
	for (n = hdr->entrylist.head; n; n = n->next) {
		free(n->data);
		n->data = NULL;
	}
	list_destroy(&hdr->entrylist);
}

void header_dump(const struct header *hdr, FILE *f)
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
