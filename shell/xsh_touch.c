/* xsh_touch.c - xsh_touch */

#include <xinu.h>
#include <fat_filelib.h>

shellcmd xsh_touch(int nargs, char *args[])
{
	FILE* fd;
	char *tmp;

	if (nargs != 2) {
		fprintf(stdout,"Use: %s <file>\n", args[0]);
		return 1;
	}
	tmp = full_path(args[1]);
	if (tmp == NULL)
		return 1;
	if ((fd = fopen(tmp, "r"))) {
		fprintf(stdout,"file exists: %s\n", tmp);
		fclose(fd);
		return 0;
	}
	fd = fopen(tmp, "w");
	if (fd)
		fclose(fd);
	return 0;
}