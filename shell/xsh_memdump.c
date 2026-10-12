/* xsh_memdump.c - xsh_memdump */

#include <xinu.h>
#include <stdio.h>
#include <string.h>

/*------------------------------------------------------------------------
 * xsh_memdump - dump a region of memory by displaying values in hex
 *			 and ascii
 *
 *	memdump <addr-hex> [bytes]   (default 256 bytes)
 *	Los bytes se muestran desde la direccion dada.
 *------------------------------------------------------------------------
 */
shellcmd xsh_memdump(int nargs, char *args[])
{
	uint32	base;
	uint32	len;
	uint32	dummy;

	if (nargs == 2 && strncmp(args[1], "--help", 7) == 0) {
		fprintf(stdout,"Use: %s <addr-hex> [bytes]\n\n", args[0]);
		fprintf(stdout,"Description:\n");
		fprintf(stdout,"\tDumps a memory region in hex and ascii\n");
		fprintf(stdout,"Options:\n");
		fprintf(stdout,"\taddr-hex\tstart address (e.g. 0x20000000)\n");
		fprintf(stdout,"\tbytes\t\tlength in bytes (default 256)\n");
		fprintf(stdout,"\t--help\t\tdisplay this help and exit\n");
		return 0;
	}

	if (nargs < 2 || nargs > 3) {
		fprintf(stderr, "%s: use: memdump <addr-hex> [bytes]\n",
			args[0]);
		return 1;
	}

	/* Parse la direccion base en hexadecimal */
	base = 0;
	{
		char *p = args[1];
		if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
			p += 2;
		while (*p != NULLCH) {
			char c = *p++;
			uint32 v;
			if      (c >= '0' && c <= '9') v = (uint32)(c - '0');
			else if (c >= 'a' && c <= 'f') v = (uint32)(c - 'a' + 10);
			else if (c >= 'A' && c <= 'F') v = (uint32)(c - 'A' + 10);
			else {
				fprintf(stderr, "%s: invalid hex address\n",
					args[0]);
				return 1;
			}
			base = (base << 4) | v;
		}
	}

	len = 256;
	if (nargs == 3)
		len = (uint32)atoi(args[2]);

	if (len > 4096) {
		fprintf(stderr, "%s: max 4096 bytes\n", args[0]);
		return 1;
	}

	fprintf(stdout,"Dumping 0x%08X (%u bytes)\n", base, len);

	/* Pequena comprobacion: no volar por una direccion nula */
	dummy = 0;
	if (base == 0 || base > 0xEFFFFFFF)
		base = (uint32)&dummy;

	hexDump2(base, (void *)base, (int)len);
	return 0;
}