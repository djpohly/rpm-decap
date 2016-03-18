#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <arpa/inet.h>

struct lead {
	char magic[4];
	char major;
	char minor;
	uint16_t type;
	uint16_t arch;
	char name[66];
	uint16_t osnum;
	uint16_t sigtype;
	char reserved[16];
} __attribute__((packed));

struct header {
	char magic[3];
	char version;
	char reserved[4];
	uint32_t nindex;
	uint32_t hdrlen;
} __attribute__((packed));

#define IDX_NULL 0
#define IDX_CHAR 1
#define IDX_INT8 2
#define IDX_INT16 3
#define IDX_INT32 4
#define IDX_INT64 5
#define IDX_STRING 6
#define IDX_BIN 7
#define IDX_STRING_ARR 8
static const char *TYPENAME[] = {
	[IDX_NULL] = "null",
	[IDX_CHAR] = "char",
	[IDX_INT8] = "int8",
	[IDX_INT16] = "int16",
	[IDX_INT32] = "int32",
	[IDX_INT64] = "int64",
	[IDX_STRING] = "string",
	[IDX_BIN] = "bin",
	[IDX_STRING_ARR] = "string[]",
};

#define TAG_CAPS 0x1392

struct idxentry {
	uint32_t tag;
	uint32_t type;
	uint32_t offset;
	uint32_t count;
} __attribute__((packed));

int main(int argc, char **argv)
{
	int rv;

	int fd = open(argv[1], O_RDWR);
	struct lead lead;
	read(fd, &lead, sizeof(lead));

	struct header hdr;
	read(fd, &hdr, sizeof(hdr));
	hdr.nindex = ntohl(hdr.nindex);
	hdr.hdrlen = ntohl(hdr.hdrlen);

	long storeofs = sizeof(struct lead) + sizeof(struct header) +
		hdr.nindex *  sizeof(struct idxentry);

	printf("== Header ==\nStore offset: 0x%lx\nIndex entries (%u):\n",
			storeofs, hdr.nindex);

	int i;
	for (i = 0; i < hdr.nindex; i++) {
		struct idxentry ent;
		read(fd, &ent, sizeof(ent));
		ent.tag = htonl(ent.tag);
		ent.type = htonl(ent.type);
		ent.offset = storeofs + htonl(ent.offset);
		ent.count = htonl(ent.count);
		printf(" %d: %s, %d len @0x%x\n", ent.tag,
				TYPENAME[ent.type], ent.count, ent.offset);
	}

	long sigofs = storeofs + hdr.hdrlen;

	printf("== Signature ==\nStart offset: 0x%lx\n", sigofs);

	close(fd);
	return 0;
}
