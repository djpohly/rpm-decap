#ifndef LEAD_H_
#define LEAD_H_

#include <stdint.h>
#include <sys/types.h>

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

int lead_init(struct lead *lead, int fd);
void lead_dump(const struct lead *lead, FILE *f);
#endif
