/* vi: set sw=4 ts=4: */
/*
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */
#include "common.h"
#include "dhcpd.h"
#include "unicode.h"

#if BB_LITTLE_ENDIAN
static inline uint64_t hton64(uint64_t v)
{
        return (((uint64_t)htonl(v)) << 32) | htonl(v >> 32);
}
#else
#define hton64(v) (v)
#endif
#define ntoh64(v) hton64(v)


int dumpleases_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int dumpleases_main(int argc UNUSED_PARAM, char **argv)
{
	int fd;
	int i;
	unsigned opt;
	int64_t written_at, curr, expires_abs;
	const char *file = LEASES_FILE;
	struct dyn_lease lease;
	struct in_addr addr;
	char *unknown_string = "unknown";

	enum {
		OPT_a	= 0x1,	// -a
		OPT_r	= 0x2,	// -r
		OPT_f	= 0x4,	// -f
		OPT_s	= 0x8,	// -s
	};
#if ENABLE_LONG_OPTS
	static const char dumpleases_longopts[] ALIGN1 =
		"absolute\0"  No_argument       "a"
		"remaining\0" No_argument       "r"
		"file\0"      Required_argument "f"
		"seconds\0"   No_argument       "s"
		;

	applet_long_options = dumpleases_longopts;
#endif
	init_unicode();

	opt_complementary = "=0:a--r:r--a";
	opt = getopt32(argv, "arf:s", &file);

	fd = xopen(file, O_RDONLY);

	printf("Mac Address       IP Address      Host Name           Expires %s\n", (opt & OPT_a) ? "at" : "in");
	/*     "00:00:00:00:00:00 255.255.255.255 ABCDEFGHIJKLMNOPQRS Wed Jun 30 21:49:08 1993" */
	/*     "123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 */

	xread(fd, &written_at, sizeof(written_at));
	written_at = ntoh64(written_at);
	curr = time(NULL);
	if (curr < written_at)
		written_at = curr; /* lease file from future! :) */

	while (full_read(fd, &lease, sizeof(lease)) == sizeof(lease)) {
		const char *fmt = ":%02x" + 1;
		for (i = 0; i < 6; i++) {
			printf(fmt, lease.lease_mac[i]);
			fmt = ":%02x";
		}
		addr.s_addr = lease.lease_nip;
#if ENABLE_UNICODE_SUPPORT
		{
			char *uni_name = unicode_conv_to_printable_fixedwidth(NULL, lease.hostname, 19);
			printf(" %-16s%s ", inet_ntoa(addr), uni_name);
			free(uni_name);
		}
#else
		/* actually, 15+1 and 19+1, +1 is a space between columns */
		/* lease.hostname is char[20] and is always NUL terminated */
		if ( *lease.hostname)
		printf(" %-16s%-20s", inet_ntoa(addr), lease.hostname);
		else
			printf(" %-16s%-20s", inet_ntoa(addr),unknown_string);
#endif
		expires_abs = ntohl(lease.expires) + written_at;
		if (expires_abs <= curr) {
			puts("expired");
			continue;
		}
		if (!(opt & OPT_a)) { /* no -a */
			unsigned d, h, m;
			unsigned expires = expires_abs - curr;
			if (opt & OPT_s) {
				printf ("%d seconds\n",expires);
			} else {
			d = expires / (24*60*60); expires %= (24*60*60);
			h = expires / (60*60); expires %= (60*60);
			m = expires / 60; expires %= 60;
			if (d)
				printf("%u days ", d);
			printf("%02u:%02u:%02u\n", h, m, (unsigned)expires);
			}
		} else { /* -a */
			time_t t = expires_abs;
			fputs(ctime(&t), stdout);
		}
	}
	/* close(fd); */

	return 0;
}
