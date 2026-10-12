/* xsh_mv.c - xsh_mv */

#include <xinu.h>
#include <fat_filelib.h>
#include <string.h>

#define	MV_BLKSIZE	512

shellcmd xsh_mv(int nargs, char *args[])
{
	FILE	*srcf, *dstf;
	char	src[128], dst[128];
	char	buf[MV_BLKSIZE];
	char	*p;
	int	rd;

	if (nargs != 3) {
		fprintf(stdout,"Use: %s <src> <dst>\n", args[0]);
		return 1;
	}

	p = full_path(args[1]);
	if (p == NULL)
		return 1;
	strcpy(src, p);
	p = full_path(args[2]);
	if (p == NULL)
		return 1;
	strcpy(dst, p);

	if (strcmp(src, dst) == 0) {
		fprintf(stdout,"%s: source and destination are the same\n", args[0]);
		return 1;
	}

	srcf = fopen(src, "r");
	if (srcf == NULL) {
		fprintf(stdout,"%s: %s not found\n", args[0], args[1]);
		return 1;
	}
	dstf = fopen(dst, "w");
	if (dstf == NULL) {
		fclose(srcf);
		fprintf(stdout,"%s: cannot open %s for output\n", args[0], args[2]);
		return 1;
	}

	while ((rd = fread(buf, 1, sizeof(buf), srcf)) > 0) {
		if (fwrite(buf, 1, rd, dstf) != rd) {
			fprintf(stdout,"%s: write error\n", args[0]);
			fclose(srcf);
			fclose(dstf);
			return 1;
		}
	}

	fclose(srcf);
	fclose(dstf);

	if (remove(src) < 0) {
		fprintf(stdout,"%s: could not remove %s after copy\n", args[0], args[1]);
		return 1;
	}
	fprintf(stdout,"moved %s -> %s\n", args[1], args[2]);
	return 0;
}