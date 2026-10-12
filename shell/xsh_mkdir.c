/* xsh_mkdir.c - xsh_mkdir */

#include <xinu.h>
#include <fat_filelib.h>

shellcmd xsh_mkdir(int nargs, char *args[])
{
	FL_DIR dirstat;
	char *tmp;

	if (nargs != 2) {
		fprintf(stdout,"Use: %s <dir>\n", args[0]);
		return 1;
	}
	tmp = full_path(args[1]);
	if (tmp == NULL)
		return 1;
	if (fl_opendir(tmp, &dirstat)) {
		fl_closedir(&dirstat);
		fprintf(stdout,"directory exists: %s\n", tmp);
		return 0;
	}
	if (mkdir(tmp) < 0) {
		fprintf(stdout,"mkdir %s failed\n", tmp);
		return 1;
	}
	fprintf(stdout,"%s created\n", tmp);
	return 0;
}