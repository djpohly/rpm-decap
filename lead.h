#ifndef LEAD_H_
#define LEAD_H_

#include <stdint.h>
#include <sys/types.h>

static const char LEAD_MAGIC[4] = {0xed, 0xab, 0xee, 0xdb};

// On-disk structure
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

// In-memory structure
struct lead {
	char major;
	char minor;
	uint16_t type;
	uint16_t arch;
	char name[66];
	uint16_t os;
	uint16_t sigtype;
};

off_t lead_init(struct lead *lead, int fd, off_t ofs);
void lead_destroy(struct lead *lead);
off_t lead_write(const struct lead *lead, int fd, off_t ofs);

#endif
