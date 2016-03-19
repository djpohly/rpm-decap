#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <endian.h>
#include <rpm/rpmtag.h>
#include <assert.h>

#include "decap.h"

int main(int argc, char **argv)
{
	int rv;

	// Open file and get length
	int fd = open(argv[1], O_RDWR);
	off_t filelen = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);

	// Read and ignore the lead
	struct lead lead;
	read(fd, &lead, sizeof(lead));

	// Signature header
	struct header hdr;
	read(fd, &hdr, sizeof(hdr));
	hdr.nindex = be32toh(hdr.nindex);
	hdr.len = be32toh(hdr.len);

	long hdrofs = sizeof(struct lead);
	long idxofs = hdrofs + sizeof(struct header);
	long storeofs = idxofs + hdr.nindex * sizeof(struct idxentry);

	int i;
	int64_t remsize = filelen - hdrofs;
	for (i = 0; i < hdr.nindex; i++) {
		struct idxentry ent;
		pread(fd, &ent, sizeof(ent), idxofs + i * sizeof(struct idxentry));
		ent.tag = be32toh(ent.tag);
		ent.type = be32toh(ent.type);
		ent.offset = storeofs + be32toh(ent.offset);
		ent.count = be32toh(ent.count);

		// Update size signature
		if (ent.tag == RPMSIGTAG_LONGSIZE) {
			assert(ent.type == RPM_INT64_TYPE);
			assert(ent.count == 1);

			remsize = htobe64(filelen - hdrofs);
			pwrite(fd, &remsize, sizeof(remsize), ent.offset);
		} else if (ent.tag == RPMSIGTAG_SIZE) {
			assert(ent.type == RPM_INT32_TYPE);
			assert(ent.count == 1);

			uint32_t remsize32 = remsize;
			remsize32 = htobe32(remsize32);
			pwrite(fd, &remsize32, sizeof(remsize32), ent.offset);
		}

		if (i == 0) {
			// Is this what "sealed" means?
			// This one shouldn't have changed
			uint32_t tag = ent.tag;
			assert(TAG_IS_REGION(tag));
			assert(ent.type == RPM_BIN_TYPE);
			assert(ent.count == 16);

			pread(fd, &ent, sizeof(ent), ent.offset);
			ent.tag = be32toh(ent.tag);
			ent.type = be32toh(ent.type);
			ent.offset = storeofs + be32toh(ent.offset);
			ent.count = be32toh(ent.count);

			assert(ent.tag == tag);
			assert(ent.type == RPM_BIN_TYPE);
			assert(ent.count == 16);
			assert(ent.offset == idxofs);
		}
	}

	// Pad to 8-byte alignment and seek past pad
	hdrofs = storeofs + hdr.len;
	hdrofs = ((hdrofs + 7) / 8) * 8;
	lseek(fd, hdrofs, SEEK_SET);

	// Second header
	pread(fd, &hdr, sizeof(hdr), hdrofs);
	hdr.nindex = be32toh(hdr.nindex);
	hdr.len = be32toh(hdr.len);

	idxofs = hdrofs + sizeof(struct header);
	storeofs = idxofs + hdr.nindex * sizeof(struct idxentry);

	for (i = 0; i < hdr.nindex; i++) {
		struct idxentry ent;
		pread(fd, &ent, sizeof(ent), idxofs + i * sizeof(struct idxentry));
		ent.tag = be32toh(ent.tag);
		ent.type = be32toh(ent.type);
		ent.offset = storeofs + be32toh(ent.offset);
		ent.count = be32toh(ent.count);

		if (i == 0) {
			// This one may need fixing
			uint32_t tag = ent.tag;
			assert(TAG_IS_REGION(tag));
			assert(ent.type == RPM_BIN_TYPE);
			assert(ent.count == 16);

			struct idxentry newent;
			newent.tag = htobe32(ent.tag);
			newent.type = htobe32(ent.type);
			newent.offset = be32toh(idxofs - storeofs);
			newent.count = htobe32(ent.count);
			pwrite(fd, &newent, sizeof(ent), ent.offset);
		}
	}

	close(fd);
	return 0;
}
