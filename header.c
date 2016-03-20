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

int entry_init(struct entry *ent, off_t store, int fd, off_t ofs)
{
	struct entry_f ef;
	pread(fd, &ef, sizeof(ef), ofs);
	ent->tag = be32toh(ef.tag);
	ent->type = be32toh(ef.type);
	ent->dataofs = store + (int32_t) be32toh(ef.dataofs);

	uint32_t count = be32toh(ef.count);
	entry_init_data(ent, fd, ent->dataofs, count);

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
	fprintf(f, "tag %d: type %d len %d\n", ent->tag,
			ent->type, ent->datalen);
}

static off_t entry_write(const struct entry *ent, int fd, off_t ofs)
{
	// Set up on-disk structure
	struct entry_f ef;
	ef.tag = htobe32(ent->tag);
	ef.type = htobe32(ent->type);
	ef.dataofs = htobe32(ent->dataofs);
	ef.count = htobe32(entry_count(ent));

	// Write out structure
	pwrite(fd, &ef, sizeof(ef), ofs);
	return ofs + sizeof(ef);
}

static off_t entry_write_data(const struct entry *ent, int fd, off_t ofs)
{
	// Get correct alignment for this entry's type
	ofs = entry_aligned_start(ent, ofs);
	pwrite(fd, ent->data, ent->datalen, ofs);
	return ofs + ent->datalen;
}


// Header blocks
off_t header_init(struct header *hdr, int fd, off_t ofs)
{
	// Align to 8-byte boundary
	ofs = ((ofs + 7) / 8) * 8;

	struct header_f hf;
	pread(fd, &hf, sizeof(hf), ofs);
	ofs += sizeof(struct header_f);
	uint32_t entries = be32toh(hf.entries);
	uint32_t datalen = be32toh(hf.datalen);

	// Precalculate store offset
	off_t storeofs = ofs + entries * sizeof(struct entry_f);

	// Read header-type entry
	struct entry hte;
	entry_init(&hte, storeofs, fd, ofs);
	hdr->type = hte.tag;
	ofs += sizeof(struct entry_f);

	// Read entries
	list_init(&hdr->entrylist);

	int i;
	for (i = 1; i < entries; i++) {
		struct entry *ent = malloc(sizeof(*ent));
		entry_init(ent, storeofs, fd, ofs);
		list_append(&hdr->entrylist, ent);
		ofs += sizeof(struct entry_f);
	}
	return ofs + datalen;
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
	fprintf(f, "== Header (type %d) ==\n", hdr->type);
	fprintf(f, " -- Entries --\n");

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
		e->dataofs = datalen;
		datalen += e->datalen;
		entries++;
	}

	// Add an entry slot for the HTE
	entries++;

	// Calculate store offset ahead of time
	off_t storeofs = ofs + sizeof(struct header_f) +
		entries * sizeof(struct entry_f);

	// Set up the reciprocal header-type entry, data first...
	struct entry_f ef;
	ef.tag = htobe32(hdr->type);
	ef.type = htobe32(RPM_BIN_TYPE);
	ef.dataofs = htobe32(-entries * sizeof(struct entry_f));
	ef.count = htobe32(16);

	// ... and then the entry
	struct entry hte;
	hte.tag = hdr->type;
	hte.type = RPM_BIN_TYPE;
	hte.dataofs = datalen;
	hte.datalen = 16;
	hte.data = &ef;

	// Account for header-type entry
	datalen += 16;
	
	// Set up on-disk structure
	struct header_f hf;
	memcpy(hf.magic, HEADER_MAGIC, sizeof(hf.magic));
	hf.version = HEADER_VERSION;
	memset(hf.reserved, 0, sizeof(hf.reserved));
	hf.entries = htobe32(entries);
	hf.datalen = htobe32(datalen);

	// Write the header information
	pwrite(fd, &hf, sizeof(hf), ofs);
	ofs += sizeof(hf);

	// First write the header-type entry
	ofs = entry_write(&hte, fd, ofs);

	// Then write the index entries
	for (n = hdr->entrylist.head; n; n = n->next)
		ofs = entry_write(n->data, fd, ofs);

	// Write the store
	for (n = hdr->entrylist.head; n; n= n->next)
		ofs = entry_write_data(n->data, fd, ofs);

	// Put data from header-type entry at the end
	ofs = entry_write_data(&hte, fd, ofs);

	return ofs;
}
