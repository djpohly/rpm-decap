#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <endian.h>
#include <rpm/rpmtag.h>

#include "list.h"

#include "rpm.h"
#include "lead.h"
#include "header.h"

// Index entries
static char *alloc_string_array(int fd, off_t ofs, int n, int *out_len)
{
	// Create buffer and prepopulate
	int sz = 32;
	char *buf = malloc(sz);
	if (!buf)
		return NULL;
	pread(fd, buf, sz, ofs);

	// Pointer to next character to search
	char *next = buf;

	while (n > 0) {
		if (next == buf + sz) {
			buf = realloc(buf, sz * 2);
			if (!buf)
				return NULL;
			next = buf + sz;
			pread(fd, next, sz, ofs + sz);
			sz *= 2;
		}

		// Find zero byte if any
		next = memchr(next, '\0', buf + sz - next);
		if (!next) {
			next = buf + sz;
		} else {
			n--;
			next++;
		}
	}
	if (out_len)
		*out_len = next - buf;
	return buf;
}

static inline int entry_init_data(struct entry *ent, int fd, off_t dataofs,
		uint32_t count)
{
	int len;
	switch (ent->type) {
		case RPM_STRING_TYPE:
		case RPM_STRING_ARRAY_TYPE:
		case RPM_I18NSTRING_TYPE:
			ent->data = alloc_string_array(fd, dataofs, count,
					&ent->datalen);
			if (!ent->data)
				return 1;
			break;
		default:
			len = TYPE_SIZE[ent->type] * count;
			ent->data = malloc(len);
			ent->datalen = len;
			pread(fd, ent->data, len, dataofs);
			break;
	}
	return 0;
}

static uint32_t entry_count(const struct entry *ent)
{
	uint32_t count;
	int len;
	char *s = ent->data;
	switch (ent->type) {
		case RPM_STRING_TYPE:
			return 1;
		case RPM_STRING_ARRAY_TYPE:
		case RPM_I18NSTRING_TYPE:
			// Count successive zero-terminated strings
			count = 0;
			len = 0;
			while (len < ent->datalen) {
				count++;
				len += strnlen(s + len, ent->datalen - len) + 1;
			}
			return count;
		default:
			if (ent->datalen % TYPE_SIZE[ent->type] != 0)
				return -1;
			return ent->datalen / TYPE_SIZE[ent->type];
	}
}

int entry_init(struct entry *ent, const struct header *hdr, int i)
{
	struct entry_f ef;
	pread(hdr->rpm->srcfd, &ef, sizeof(ef),
			hdr->idxofs + i * sizeof(struct entry_f));
	ent->hdr = hdr;
	ent->tag = be32toh(ef.tag);
	ent->type = be32toh(ef.type);
	ent->dataofs = hdr->storeofs + (int32_t) be32toh(ef.dataofs);

	uint32_t count = be32toh(ef.count);
	entry_init_data(ent, hdr->rpm->srcfd, ent->dataofs, count);

	return 0;
}

void entry_destroy(struct entry *ent)
{
}

static inline off_t entry_aligned_start(const struct entry *ent, off_t ofs)
{
	int align = TYPE_ALIGN[ent->type];
	return ((ofs + align - 1) / align) * align;
}

int entry_dump(const struct entry *ent, FILE *f)
{
	fprintf(f, "tag %d: type %d len %d at 0x%lx\n", ent->tag,
			ent->type, ent->datalen, ent->dataofs);
}

static off_t entry_write(const struct entry *ent, int fd, off_t ofs)
{
	// Set up on-disk structure
	struct entry_f ef;
	ef.tag = htobe32(ent->tag);
	ef.type = htobe32(ent->type);
	ef.dataofs = htobe32(ent->dataofs - ent->hdr->storeofs);
	ef.count = htobe32(entry_count(ent));

	// Write out structure
	pwrite(fd, &ef, sizeof(ef), ofs);
	return ofs + sizeof(ef);
}


// Header blocks
static int header_init_common(struct header *hdr, const struct rpm *rpm,
		off_t ofs)
{
	struct header_f hf;
	pread(rpm->srcfd, &hf, sizeof(hf), ofs);
	hdr->entries = be32toh(hf.entries);
	hdr->datalen = be32toh(hf.datalen);
	hdr->ofs = ofs;
	hdr->idxofs = ofs + sizeof(struct header_f);
	hdr->storeofs = hdr->idxofs + hdr->entries * sizeof(struct entry_f);
	hdr->rpm = rpm;

	// Read entries
	list_init(&hdr->entrylist);

	int i;
	for (i = 0; i < hdr->entries; i++) {
		struct entry *ent = malloc(sizeof(*ent));
		entry_init(ent, hdr, i);
		list_append(&hdr->entrylist, ent);
	}
	return 0;
}

int header_init_first(struct header *hdr, const struct rpm *rpm)
{
	return header_init_common(hdr, rpm, sizeof(struct lead_f));
}

int header_init_next(struct header *hdr, const struct rpm *rpm,
		const struct header *prev)
{
	off_t ofs = prev->storeofs + prev->datalen;
	ofs = ((ofs + 7) / 8) * 8;
	return header_init_common(hdr, rpm, ofs);
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

off_t header_write(const struct header *hdr, int fd, off_t ofs)
{
	// Align header to 8 bytes
	ofs = ((ofs + 7) / 8) * 8;
	
	// Calculate updated values for entries and datalen
	uint32_t entries = 0;
	uint32_t datalen = 0;

	struct listnode *n;
	for (n = hdr->entrylist.head; n; n = n->next) {
		struct entry *e = n->data;
		datalen = entry_aligned_start(e, datalen);
		datalen += e->datalen;
		entries++;
	}
	
	// Set up on-disk structure
	struct header_f hf;
	memcpy(hf.magic, HEADER_MAGIC, sizeof(hf.magic));
	hf.version = HEADER_VERSION;
	memset(hf.reserved, 0, sizeof(hf.reserved));
	hf.entries = htobe32(hdr->entries);
	hf.datalen = htobe32(hdr->datalen);

	// Write the header information
	pwrite(fd, &hf, sizeof(hf), ofs);
	ofs += sizeof(hf);

	// Write the index entries
	for (n = hdr->entrylist.head; n; n = n->next) {
		ofs = entry_aligned_start(n->data, ofs);
		ofs = entry_write(n->data, fd, ofs);
	}

	return ofs;
}
