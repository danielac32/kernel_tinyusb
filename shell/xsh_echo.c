/* xsh_echo.c - xsh_echo */

#include <xinu.h>
#include <fat_filelib.h>
#include <string.h>

/*------------------------------------------------------------------------
 * xsh_echo - escribe texto en un fichero FAT (lo crea o sobreescribe)
 *
 *	echo <fichero> <texto...>
 *------------------------------------------------------------------------
 */
shellcmd xsh_echo(int nargs, char *args[])
{
	FL_FILE	*f;
	char	*p;
	int	i;

	if (nargs == 2 && strncmp(args[1], "--help", 7) == 0) {
		fprintf(stdout,"Use: %s <fichero> <texto...>\n\n", args[0]);
		fprintf(stdout,"Description:\n");
		fprintf(stdout,"\tEscribe el texto en el fichero, creandolo o\n");
		fprintf(stdout,"\tsobreescribiendolo\n");
		fprintf(stdout,"Options:\n");
		fprintf(stdout,"\t--help\tdisplay this help and exit\n");
		return SHELL_OK;
	}

	if (nargs < 3) {
		fprintf(stdout,"Uso: %s <fichero> <texto...>\n", args[0]);
		return SHELL_ERROR;
	}

	p = full_path(args[1]);
	if (p == NULL)
		return SHELL_ERROR;

	f = fopen(p, "w");
	if (f == NULL) {
		fprintf(stdout,"echo: no se pudo crear '%s'\n", p);
		return SHELL_ERROR;
	}

	for (i = 2; i < nargs; i++) {
		if (i > 2)
			fwrite(" ", 1, 1, f);
		fwrite(args[i], 1, strlen(args[i]), f);
	}
	fwrite("\n", 1, 1, f);
	fl_fclose(f);
	update_path();
	fprintf(stdout,"echo: escrito %s\n", p);
	return SHELL_OK;
}