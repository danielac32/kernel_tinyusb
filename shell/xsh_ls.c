/* xsh_ls.c - xsh_ls */

#include <xinu.h>
#include <fat_filelib.h>

shellcmd xsh_ls(int nargs, char *args[])
{
	FL_DIR dirstat;
	char *tmp;
	int count;

	if (nargs > 2) {
		fprintf(stdout,"Use: %s [dir]\n", args[0]);
		return 1;
	}
	tmp = full_path((nargs == 2) ? args[1] : "");
	if (tmp == NULL)
		return 1;

	fprintf(stdout,"Directory %s:\r\n", tmp);
	if (fl_opendir(tmp, &dirstat)) {
		struct fs_dir_ent dirent;
		count = 0;
		while (fl_readdir(&dirstat, &dirent) == 0) {
			if (dirent.is_dir) {
				fprintf(stdout,"  %s <DIR>\r\n", dirent.filename);
			} else {
				fprintf(stdout,"  %s [%d bytes]\r\n",
					dirent.filename, dirent.size);
			}
			count++;
		}
		fl_closedir(&dirstat);
		fprintf(stdout,"%d entrie(s)\r\n", count);
	} else {
		fprintf(stdout,"  (cannot open)\r\n");
		return 1;
	}
	update_path();
	return 0;
}