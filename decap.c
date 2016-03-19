#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <endian.h>
#include <assert.h>
#include <sys/sendfile.h>
#include <rpm/rpmtag.h>

#include "decap.h"

static char *read_alloc_string(int fd, long offset)
{
	int len = 0;
	int b = 0;
	int p = 0;
	char *s = NULL;

	while (!memchr(s + p, '\0', len - p)) {
		p += b;
		if (p)
			len = p * 2;
		else
			len = 32;
		s = realloc(s, len);
		if (!s)
			return NULL;

		b = pread(fd, s + p, len - p, offset + p);
		if (b < 0) {
			perror("pread");
			free(s);
			return NULL;
		}
		if (b == 0) {
			fprintf(stderr, "hit EOF looking for end of string\n");
			free(s);
			return NULL;
		}
	}
	return s;
}

int main(int argc, char **argv)
{
	int rv;

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

	// Locate the first header
	long hdrofs = sizeof(struct lead);

	// Copy the first header
	struct header hdr;
	read(in, &hdr, sizeof(hdr));
	write(out, &hdr, sizeof(hdr));

	// Parse the first header
	hdr.nindex = be32toh(hdr.nindex);
	hdr.len = be32toh(hdr.len);

	// Copy up to the start of the next header (8-byte aligned)
	long len = hdr.nindex * sizeof(struct idxentry) + hdr.len;
	len = ((len + 7) / 8) * 8;
	sendfile(out, in, NULL, len);

	// Read the second header
	read(in, &hdr, sizeof(hdr));
	hdr.nindex = be32toh(hdr.nindex);
	hdr.len = be32toh(hdr.len);

	// Prepare the new header
	struct header newhdr;
	newhdr.nindex = 0;
	newhdr.len = 0;

	long idxofs = hdrofs + sizeof(struct header);
	long storeofs = idxofs + hdr.nindex * sizeof(struct idxentry);

	printf("== Header 2 ==\nHeader offset: 0x%lx\nIndex offset: 0x%lx\n"
			"Store offset: 0x%lx\nIndex entries (%u):\n",
			hdrofs, idxofs, storeofs, hdr.nindex);

	int i;
	for (i = 0; i < hdr.nindex; i++) {
		struct idxentry ent;
		pread(in, &ent, sizeof(ent), idxofs + i * sizeof(struct idxentry));
		ent.tag = be32toh(ent.tag);
		ent.type = be32toh(ent.type);
		ent.offset = storeofs + be32toh(ent.offset);
		ent.count = be32toh(ent.count);
	}

	close(out);
	close(in);
	return 0;
}
