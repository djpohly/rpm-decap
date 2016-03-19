#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <endian.h>
#include <assert.h>
#include <rpm/rpmtag.h>

#include "decap.h"

static int string_array_size(int fd, int ofs, int n)
{
	int sz = 0;
	while (n > 0) {
		char buf[BUFSIZ];
		pread(fd, buf, BUFSIZ, ofs);
		int i;
		for (i = 0; i < BUFSIZ && n > 0; i++)
			if (!buf[i])
				n--;
		sz += i;
		ofs += BUFSIZ;
	}
}

static int copy_bytes(int out, int in, unsigned int len)
{
	char buf[BUFSIZ];
	int to_eof = 0;
	if (!len) {
		to_eof = 1;
		len = BUFSIZ;
	}
	int b = 1;
	while (len > 0 && b > 0) {
		b = read(in, buf, len < BUFSIZ ? len : BUFSIZ);
		if (b < 0) {
			perror("read");
			return 1;
		} else if (b == 0 && !to_eof) {
			fprintf(stderr, "read: unexpected EOF\n");
			return 1;
		}
		write(out, buf, b);
		if (!to_eof)
			len -= b;
	}

	return 0;
}

int main(int argc, char **argv)
{
	int rv;
	char buf[BUFSIZ];

	if (argc < 3) {
		fprintf(stderr, "Usage: %s INFILE OUTFILE\n", argv[0]);
		return 1;
	}

	// Open input and output files
	int in = open(argv[1], O_RDONLY);
	if (in < 0) {
		perror("open");
		return 1;
	}
	int out = open(argv[2], O_WRONLY | O_TRUNC | O_CREAT, 0644);
	if (out < 0) {
		perror("open");
		close(in);
		return 1;
	}

	// Copy the lead (legacy header)
	struct lead lead;
	read(in, &lead, sizeof(lead));
	write(out, &lead, sizeof(lead));

	// Locate the signature header
	long sigofs = sizeof(struct lead);

	// Copy the signature header
	struct header sig;
	read(in, &sig, sizeof(sig));
	write(out, &sig, sizeof(sig));

	// Parse the signature header
	sig.nindex = be32toh(sig.nindex);
	sig.len = be32toh(sig.len);
	long idxofs = sigofs + sizeof(struct header);
	long storeofs = idxofs + sig.nindex * sizeof(struct idxentry);

	// Copy the signature index
	int i;
	int64_t remsize = -1;
	for (i = 0; i < sig.nindex; i++) {
		// Copy entry
		struct idxentry ent;
		read(in, &ent, sizeof(ent));
		write(out, &ent, sizeof(ent));

		// Parse entries for SIZE signatures only
		ent.tag = be32toh(ent.tag);
		ent.type = be32toh(ent.type);
		ent.offset = storeofs + be32toh(ent.offset);
		ent.count = be32toh(ent.count);

		if (ent.tag == RPMSIGTAG_LONGSIZE) {
			assert(ent.type == RPM_INT64_TYPE);
			assert(ent.count == 1);

			pread(in, &remsize, sizeof(remsize), ent.offset);
			remsize = be64toh(remsize);
		} else if (ent.tag == RPMSIGTAG_SIZE) {
			assert(ent.type == RPM_INT32_TYPE);
			assert(ent.count == 1);

			uint32_t remsize32 = 0;
			pread(in, &remsize32, sizeof(remsize32), ent.offset);
			remsize = be32toh(remsize32);
		}
	}

	assert(remsize >= 0);

	// Copy the store and padding up to 8-byte alignment
	long hdrofs = storeofs + sig.len;
	hdrofs = ((hdrofs + 7) / 8) * 8;
	long filelen = hdrofs + remsize;
	long len = hdrofs - lseek(in, 0, SEEK_CUR);
	if (copy_bytes(out, in, len))
		return 1;
	fprintf(stderr, "in: 0x%lx\n", lseek(in, 0, SEEK_CUR));
	fprintf(stderr, "out: 0x%lx\n", lseek(out, 0, SEEK_CUR));

	// Read the immutable header
	struct header hdr;
	read(in, &hdr, sizeof(hdr));
	hdr.nindex = be32toh(hdr.nindex);
	hdr.len = be32toh(hdr.len);

	// Prepare the new immutable header
	struct header newhdr = hdr;

	// Calculate relevant offsets
	idxofs = hdrofs + sizeof(struct header);
	storeofs = idxofs + hdr.nindex * sizeof(struct idxentry);

	// First pass: calculate size of index and store in new header, find
	// RPMTAG_FILECAPS if there is one
	long capofs = -1;
	for (i = 0; i < hdr.nindex; i++) {
		// Read entry
		struct idxentry ent;
		pread(in, &ent, sizeof(ent), idxofs + i * sizeof(struct idxentry));
		ent.tag = be32toh(ent.tag);
		ent.type = be32toh(ent.type);
		ent.offset = storeofs + be32toh(ent.offset);
		ent.count = be32toh(ent.count);

		// Adjust if caps found
		if (ent.tag == RPMTAG_FILECAPS) {
			assert(ent.type == RPM_STRING_ARRAY_TYPE);
			capofs = ent.offset;
			newhdr.nindex--;
			int capsize = string_array_size(in, capofs, ent.count);
			newhdr.len -= capsize;
			remsize -= sizeof(struct idxentry) + capsize;
			fprintf(stderr, "found caps @ 0x%x\n", capofs);
		}
	}

	// Write (possibly adjusted) second header
	newhdr.nindex = htobe32(newhdr.nindex);
	newhdr.len = htobe32(newhdr.len);
	write(out, &newhdr, sizeof(newhdr));
	newhdr.nindex = htobe32(newhdr.nindex);
	newhdr.len = htobe32(newhdr.len);

	// Second pass: write second index, adjusting for omitted entry and data
	for (i = 0; i < hdr.nindex; i++) {
		// Read entry
		struct idxentry ent;
		read(in, &ent, sizeof(ent));
		ent.tag = be32toh(ent.tag);
		ent.type = be32toh(ent.type);
		ent.offset = storeofs + be32toh(ent.offset);
		ent.count = be32toh(ent.count);

		// Skip FILECAPS
		if (ent.tag == RPMTAG_FILECAPS)
			continue;

		// Write entry, adjusting offset if was after caps
		if (ent.offset > capofs)
			ent.offset -= (hdr.len - newhdr.len);

		ent.tag = be32toh(ent.tag);
		ent.type = be32toh(ent.type);
		ent.offset = be32toh(ent.offset - storeofs);
		ent.count = be32toh(ent.count);
		write(out, &ent, sizeof(ent));
	}

	assert(lseek(in, 0, SEEK_CUR) == storeofs);
	assert(lseek(out, 0, SEEK_CUR) == storeofs - (capofs >= 0) * sizeof(struct idxentry));

	// Copy store up to the capabilities
	if (capofs >= 0) {
		copy_bytes(out, in, capofs - storeofs);

		// Skip over capabilities data
		lseek(in, hdr.len - newhdr.len, SEEK_CUR);
	}

	// Copy the rest of the file
	copy_bytes(out, in, 0);

	close(out);
	close(in);
	return 0;
}
