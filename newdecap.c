#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <endian.h>
#include <rpm/rpmtag.h>

#include "list.h"
#include "lead.h"
#include "header.h"
#include "rpm.h"

int main(int argc, char **argv)
{
	int rv;
	char buf[BUFSIZ];

	if (argc < 3) {
		fprintf(stderr, "Usage: %s INFILE OUTFILE\n", argv[0]);
		return 1;
	}

	// Open input file
	int in = open(argv[1], O_RDONLY);
	if (in < 0) {
		perror("open");
		return 1;
	}

	// Load input file information
	struct rpm rpm;
	if (rpm_init(&rpm, in)) {
		close(in);
		return 1;
	}

	// Remove FILECAPS tag
	struct listnode *n;
	struct listnode *prev = NULL;
	for (n = rpm.taghdr.entrylist.head; n; n = n->next) {
		struct entry *ent = n->data;
		if (ent->tag == RPMTAG_FILECAPS) {
			if (prev)
				prev->next = n->next;
			else
				rpm.taghdr.entrylist.head = n->next;
			break;
		}
		prev = n;
	}

	// Open output file
	int out = open(argv[2], O_WRONLY | O_TRUNC | O_CREAT, 0644);
	if (out < 0) {
		perror("open");
		close(in);
		return 1;
	}

	lseek(in, rpm.arcofs, SEEK_SET);
	rpm_write(&rpm, in, out);

	close(out);

	rpm_destroy(&rpm);

	close(in);

	return 0;
}
