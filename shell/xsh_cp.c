/* xsh_cp.c - xsh_cp */

#include <xinu.h>
#include <fat_filelib.h>
#include <string.h>

#define	CP_BLKSIZE	512	/* Tamano de bloque para la copia	*/

shellcmd xsh_cp(int nargs, char *args[])
{
	FILE	*srcf, *dstf;
	char	src[128], dst[128];
	char	buf[CP_BLKSIZE];
	char	*p;
	int	rd;
	uint32	total = 0;

	if (nargs != 3) {
		fprintf(stdout,"Use: %s <src> <dst>\n", args[0]);
		return 1;
	}

	/* full_path() devuelve puntero al buffer global 'path': copiar
	 * antes de pedir la segunda ruta para no pisarse. */
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
		total += rd;
	}

	fclose(srcf);
	fclose(dstf);
	fprintf(stdout,"copied %s -> %s (%u bytes)\n", args[1], args[2],
		(unsigned)total);
	return 0;
}