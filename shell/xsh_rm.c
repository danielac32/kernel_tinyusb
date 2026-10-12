/* xsh_rm.c - xsh_rm */

#include <xinu.h>
#include <fat_filelib.h>

shellcmd xsh_rm(int nargs, char *args[])
{
	char *tmp;

	if (nargs != 2) {
		fprintf(stdout,"Use: %s <file>\n", args[0]);
		return 1;
	}
	tmp = full_path(args[1]);
	if (tmp == NULL)
		return 1;
	if (remove(tmp) < 0) {
		fprintf(stdout,"remove %s failed\n", tmp);
		return 1;
	}
	fprintf(stdout,"%s removed\n", tmp);
	return 0;
}