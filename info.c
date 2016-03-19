#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <endian.h>
#include <rpm/rpmtag.h>
#include <assert.h>

#define	TAG_IS_REGION(tag) \
	(((tag) >= RPMTAG_HEADERIMAGE) && ((tag) < RPMTAG_HEADERREGIONS))

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
	uint32_t len;
} __attribute__((packed));

static const char *TYPENAME[] = {
	[RPM_NULL_TYPE] = "null",
	[RPM_CHAR_TYPE] = "char",
	[RPM_INT8_TYPE] = "int8",
	[RPM_INT16_TYPE] = "int16",
	[RPM_INT32_TYPE] = "int32",
	[RPM_INT64_TYPE] = "int64",
	[RPM_STRING_TYPE] = "string",
	[RPM_BIN_TYPE] = "bin",
	[RPM_STRING_ARRAY_TYPE] = "string",
	[RPM_I18NSTRING_TYPE] = "i18nstring",
};

struct idxentry {
	uint32_t tag;
	uint32_t type;
	int32_t offset;
	uint32_t count;
} __attribute__((packed));

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

static void print_value(int fd, const struct idxentry *hi)
{
	char c;
	int8_t s8;
	int16_t s16;
	int32_t s32;
	int64_t s64;
	char *string;
	int len;

	if (hi->count > 1) {
		printf("%s[%d] @ 0x%lx", TYPENAME[hi->type], hi->count, hi->offset);
		return;
	}
	switch (hi->type) {
		case RPM_NULL_TYPE:
			printf("null");
			break;
		case RPM_CHAR_TYPE:
			pread(fd, &c, sizeof(c), hi->offset);
			printf("%c", c);
			break;
		case RPM_INT8_TYPE:
			pread(fd, &s8, sizeof(s8), hi->offset);
			printf("0x%02x", s8);
			break;
		case RPM_INT16_TYPE:
			pread(fd, &s16, sizeof(s16), hi->offset);
			printf("0x%04x", be16toh(s16));
			break;
		case RPM_INT32_TYPE:
			pread(fd, &s32, sizeof(s32), hi->offset);
			printf("0x%08x", be32toh(s32));
			break;
		case RPM_INT64_TYPE:
			pread(fd, &s64, sizeof(s64), hi->offset);
			printf("0x%016x", be64toh(s64));
			break;
		case RPM_STRING_TYPE:
			string = read_alloc_string(fd, hi->offset);
			printf("\"%s\"", string);
			free(string);
			break;
		case RPM_BIN_TYPE:
			printf("bin[%d] @ 0x%x", hi->count, hi->offset);
			break;
		case RPM_STRING_ARRAY_TYPE:
			string = read_alloc_string(fd, hi->offset);
			printf("[\"%s\"]", string);
			free(string);
			break;
		case RPM_I18NSTRING_TYPE:
			string = read_alloc_string(fd, hi->offset);
			printf("i18n \"%s\"", string);
			free(string);
			break;
	}
}

const char *hdr_tag_name(uint32_t tag)
{
	switch (tag) {
#define TAG(n) case RPMTAG_##n: return #n
		TAG(HEADERIMAGE);
		TAG(HEADERSIGNATURES);
		TAG(HEADERIMMUTABLE);
		TAG(HEADERREGIONS);

		TAG(HEADERI18NTABLE);
#undef TAG
		case RPMSIGTAG_SIZE: return "Header+Payload size (32bit)";
		case RPMSIGTAG_PGP: return "PGP 2.6.3 signature";
		case RPMSIGTAG_MD5: return "MD5 signature";
		case RPMSIGTAG_GPG: return "GnuPG signature";
		case RPMSIGTAG_PGP5: return "PGP5 signature";
		case RPMSIGTAG_PAYLOADSIZE: return "uncompressed payload size (32bit)";
		case RPMSIGTAG_RESERVEDSPACE: return "space reserved for signatures";
		case RPMSIGTAG_SHA1: return "sha1 header digest";
		case RPMSIGTAG_DSA: return "DSA header signature";
		case RPMSIGTAG_RSA: return "RSA header signature";
		case RPMSIGTAG_LONGSIZE: return "Header+Payload size (64bit)";
		case RPMSIGTAG_LONGARCHIVESIZE: return "uncompressed payload size (64bit)";
	}
	return NULL;
}

const char *sig_tag_name(uint32_t tag)
{
	switch (tag) {
#define TAG(n) case RPMTAG_##n: return #n
		// 64
		TAG(HEADERIMAGE);
		TAG(HEADERSIGNATURES);
		TAG(HEADERIMMUTABLE);
		TAG(HEADERREGIONS);

		// 100
		TAG(HEADERI18NTABLE);

		// 256
		TAG(SIGSIZE);
		TAG(SIGPGP);
		TAG(SIGMD5);
		TAG(RSAHEADER);
		TAG(SHA1HEADER);

		// 1000
		TAG(NAME);
		TAG(VERSION);
		TAG(RELEASE);
		TAG(EPOCH);
		TAG(SUMMARY);
		TAG(DESCRIPTION);
		TAG(BUILDTIME);
		TAG(BUILDHOST);
		TAG(INSTALLTIME);
		TAG(SIZE);
		TAG(DISTRIBUTION);
		TAG(VENDOR);
		TAG(GIF);
		TAG(XPM);
		TAG(LICENSE);
		TAG(PACKAGER);
		TAG(GROUP);
		TAG(CHANGELOG);
		TAG(SOURCE);
		TAG(PATCH);
		TAG(URL);
		TAG(OS);
		TAG(ARCH);
		TAG(PREIN);
		TAG(POSTIN);
		TAG(PREUN);
		TAG(POSTUN);
		TAG(OLDFILENAMES);
		TAG(FILESIZES);
		TAG(FILESTATES);
		TAG(FILEMODES);
		TAG(FILEUIDS);
		TAG(FILEGIDS);
		TAG(FILERDEVS);
		TAG(FILEMTIMES);
		TAG(FILEDIGESTS);
		TAG(FILELINKTOS);
		TAG(FILEFLAGS);
		TAG(ROOT);
		TAG(FILEUSERNAME);
		TAG(FILEGROUPNAME);
		TAG(EXCLUDE);
		TAG(EXCLUSIVE);
		TAG(ICON);
		TAG(SOURCERPM);
		TAG(FILEVERIFYFLAGS);
		TAG(ARCHIVESIZE);
		TAG(PROVIDES);
		TAG(REQUIREFLAGS);
		TAG(REQUIRES);
		TAG(REQUIREVERSION);
		TAG(NOSOURCE);
		TAG(NOPATCH);
		TAG(CONFLICTFLAGS);
		TAG(CONFLICTS);
		TAG(CONFLICTVERSION);
		TAG(DEFAULTPREFIX);
		TAG(BUILDROOT);
		TAG(INSTALLPREFIX);
		TAG(EXCLUDEARCH);
		TAG(EXCLUDEOS);
		TAG(EXCLUSIVEARCH);
		TAG(EXCLUSIVEOS);
		TAG(AUTOREQPROV);
		TAG(RPMVERSION);
		TAG(TRIGGERSCRIPTS);
		TAG(TRIGGERNAME);
		TAG(TRIGGERVERSION);
		TAG(TRIGGERFLAGS);
		TAG(TRIGGERINDEX);
		TAG(VERIFYSCRIPT);
		TAG(CHANGELOGTIME);
		TAG(CHANGELOGNAME);
		TAG(CHANGELOGTEXT);
		TAG(BROKENMD5);
		TAG(PREREQ);
		TAG(PREINPROG);
		TAG(POSTINPROG);
		TAG(PREUNPROG);
		TAG(POSTUNPROG);
		TAG(BUILDARCHS);
		TAG(OBSOLETES);
		TAG(VERIFYSCRIPTPROG);
		TAG(TRIGGERSCRIPTPROG);
		TAG(DOCDIR);
		TAG(COOKIE);
		TAG(FILEDEVICES);
		TAG(FILEINODES);
		TAG(FILELANGS);
		TAG(PREFIXES);
		TAG(INSTPREFIXES);
		TAG(TRIGGERIN);
		TAG(TRIGGERUN);
		TAG(TRIGGERPOSTUN);
		TAG(AUTOREQ);
		TAG(AUTOPROV);
		TAG(CAPABILITY);
		TAG(SOURCEPACKAGE);
		TAG(OLDORIGFILENAMES);
		TAG(BUILDPREREQ);
		TAG(BUILDREQUIRES);
		TAG(BUILDCONFLICTS);
		TAG(BUILDMACROS);
		TAG(PROVIDEFLAGS);
		TAG(PROVIDEVERSION);
		TAG(OBSOLETEFLAGS);
		TAG(OBSOLETEVERSION);
		TAG(DIRINDEXES);
		TAG(BASENAMES);
		TAG(DIRNAMES);
		TAG(ORIGDIRINDEXES);
		TAG(ORIGBASENAMES);
		TAG(ORIGDIRNAMES);
		TAG(OPTFLAGS);
		TAG(DISTURL);
		TAG(PAYLOADFORMAT);
		TAG(PAYLOADCOMPRESSOR);
		TAG(PAYLOADFLAGS);
		TAG(INSTALLCOLOR);
		TAG(INSTALLTID);
		TAG(REMOVETID);
		TAG(SHA1RHN);
		TAG(RHNPLATFORM);
		TAG(PLATFORM);
		TAG(PATCHESNAME);
		TAG(PATCHESFLAGS);
		TAG(PATCHESVERSION);
		TAG(CACHECTIME);
		TAG(CACHEPKGPATH);
		TAG(CACHEPKGSIZE);
		TAG(CACHEPKGMTIME);
		TAG(FILECOLORS);
		TAG(FILECLASS);
		TAG(CLASSDICT);
		TAG(FILEDEPENDSX);
		TAG(FILEDEPENDSN);
		TAG(DEPENDSDICT);
		TAG(SOURCEPKGID);
		TAG(FILECONTEXTS);
		TAG(FSCONTEXTS);
		TAG(RECONTEXTS);
		TAG(POLICIES);
		TAG(PRETRANS);
		TAG(POSTTRANS);
		TAG(PRETRANSPROG);
		TAG(POSTTRANSPROG);
		TAG(DISTTAG);
		TAG(OLDSUGGESTS);
		TAG(OLDSUGGESTSVERSION);
		TAG(OLDSUGGESTSFLAGS);
		TAG(OLDENHANCES);
		TAG(OLDENHANCESVERSION);
		TAG(OLDENHANCESFLAGS);
		TAG(PRIORITY);
		TAG(SVNID);
		TAG(BLINKPKGID);
		TAG(BLINKHDRID);
		TAG(BLINKNEVRA);
		TAG(FLINKPKGID);
		TAG(FLINKHDRID);
		TAG(FLINKNEVRA);
		TAG(PACKAGEORIGIN);
		TAG(TRIGGERPREIN);
		TAG(BUILDSUGGESTS);
		TAG(BUILDENHANCES);
		TAG(SCRIPTSTATES);
		TAG(SCRIPTMETRICS);
		TAG(BUILDCPUCLOCK);
		TAG(FILEDIGESTALGOS);
		TAG(VARIANTS);
		TAG(XMAJOR);
		TAG(XMINOR);
		TAG(REPOTAG);
		TAG(KEYWORDS);
		TAG(BUILDPLATFORMS);
		TAG(PACKAGECOLOR);
		TAG(PACKAGEPREFCOLOR);
		TAG(XATTRSDICT);
		TAG(FILEXATTRSX);
		TAG(DEPATTRSDICT);
		TAG(CONFLICTATTRSX);
		TAG(OBSOLETEATTRSX);
		TAG(PROVIDEATTRSX);
		TAG(REQUIREATTRSX);
		TAG(BUILDPROVIDES);
		TAG(BUILDOBSOLETES);
		TAG(DBINSTANCE);
		TAG(NVRA);

		// 5000
		TAG(FILENAMES);
		TAG(FILEPROVIDE);
		TAG(FILEREQUIRE);
		TAG(FSNAMES);
		TAG(FSSIZES);
		TAG(TRIGGERCONDS);
		TAG(TRIGGERTYPE);
		TAG(ORIGFILENAMES);
		TAG(LONGFILESIZES);
		TAG(LONGSIZE);
		TAG(FILECAPS);
		TAG(FILEDIGESTALGO);
		TAG(BUGURL);
		TAG(EVR);
		TAG(NVR);
		TAG(NEVR);
		TAG(NEVRA);
		TAG(HEADERCOLOR);
		TAG(VERBOSE);
		TAG(EPOCHNUM);
		TAG(PREINFLAGS);
		TAG(POSTINFLAGS);
		TAG(PREUNFLAGS);
		TAG(POSTUNFLAGS);
		TAG(PRETRANSFLAGS);
		TAG(POSTTRANSFLAGS);
		TAG(VERIFYSCRIPTFLAGS);
		TAG(TRIGGERSCRIPTFLAGS);
		TAG(COLLECTIONS);
		TAG(POLICYNAMES);
		TAG(POLICYTYPES);
		TAG(POLICYTYPESINDEXES);
		TAG(POLICYFLAGS);
		TAG(VCS);
		TAG(ORDERNAME);
		TAG(ORDERVERSION);
		TAG(ORDERFLAGS);
		TAG(MSSFMANIFEST);
		TAG(MSSFDOMAIN);
		TAG(INSTFILENAMES);
		TAG(REQUIRENEVRS);
		TAG(PROVIDENEVRS);
		TAG(OBSOLETENEVRS);
		TAG(CONFLICTNEVRS);
		TAG(FILENLINKS);
		TAG(RECOMMENDS);
		TAG(RECOMMENDVERSION);
		TAG(RECOMMENDFLAGS);
		TAG(SUGGESTS);
		TAG(SUGGESTVERSION);
		TAG(SUGGESTFLAGS);
		TAG(SUPPLEMENTS);
		TAG(SUPPLEMENTVERSION);
		TAG(SUPPLEMENTFLAGS);
		TAG(ENHANCES);
		TAG(ENHANCEVERSION);
		TAG(ENHANCEFLAGS);
		TAG(RECOMMENDNEVRS);
		TAG(SUGGESTNEVRS);
		TAG(SUPPLEMENTNEVRS);
		TAG(ENHANCENEVRS);
#undef TAG
	}
	return NULL;
}

int main(int argc, char **argv)
{
	int rv;

	int fd = open(argv[1], O_RDWR);
	struct lead lead;
	read(fd, &lead, sizeof(lead));

	// First header
	struct header hdr;
	read(fd, &hdr, sizeof(hdr));
	hdr.nindex = be32toh(hdr.nindex);
	hdr.len = be32toh(hdr.len);

	long hdrofs = sizeof(struct lead);
	long idxofs = hdrofs + sizeof(struct header);
	long storeofs = idxofs + hdr.nindex * sizeof(struct idxentry);

	printf("== Header 1 ==\nHeader offset: 0x%lx\nIndex offset: 0x%lx\n"
			"Store: 0x%lx - 0x%lx\nIndex entries (%u):\n",
			hdrofs, idxofs, storeofs, storeofs + hdr.len, hdr.nindex);

	int i;
	int64_t remsize = -1;
	for (i = 0; i < hdr.nindex; i++) {
		struct idxentry ent;
		pread(fd, &ent, sizeof(ent), idxofs + i * sizeof(struct idxentry));
		ent.tag = be32toh(ent.tag);
		ent.type = be32toh(ent.type);
		ent.offset = storeofs + be32toh(ent.offset);
		ent.count = be32toh(ent.count);

		if (ent.tag == RPMSIGTAG_LONGSIZE) {
			assert(ent.type == RPM_INT64_TYPE);
			assert(ent.count == 1);

			pread(fd, &remsize, sizeof(remsize), ent.offset);
			remsize = be64toh(remsize);
		} else if (ent.tag == RPMSIGTAG_SIZE) {
			assert(ent.type == RPM_INT32_TYPE);
			assert(ent.count == 1);

			uint32_t remsize32 = 0;
			pread(fd, &remsize32, sizeof(remsize32), ent.offset);
			remsize = be32toh(remsize32);
		}

		const char *name = hdr_tag_name(ent.tag);
		if (name)
			printf("  %s: ", name);
		else
			printf("  %d: ", ent.tag);
		print_value(fd, &ent);
		printf("\n");

		if (i == 0) {
			// Is this what "sealed" means?
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
	printf("\n");

	assert(remsize >= 0);

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

	printf("== Header 2 ==\nTotal filesize: 0x%lx\nHeader offset: 0x%lx\n"
			"Index offset: 0x%lx\nStore: 0x%lx - 0x%lx\nIndex entries (%u):\n",
			hdrofs + remsize, hdrofs, idxofs, storeofs, storeofs + hdr.len, hdr.nindex);

	for (i = 0; i < hdr.nindex; i++) {
		struct idxentry ent;
		pread(fd, &ent, sizeof(ent), idxofs + i * sizeof(struct idxentry));
		ent.tag = be32toh(ent.tag);
		ent.type = be32toh(ent.type);
		ent.offset = storeofs + be32toh(ent.offset);
		ent.count = be32toh(ent.count);
		const char *name = sig_tag_name(ent.tag);
		if (name)
			printf("  %s: ", name);
		else
			printf("  %d: ", ent.tag);
		print_value(fd, &ent);
		printf("\n");
	}
	printf("\n");

	close(fd);
	return 0;
}
