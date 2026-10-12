/* xsh_cat.c - xsh_cat */

#include <xinu.h>
#include <fat_filelib.h>
#include <string.h>

shellcmd xsh_cat(int nargs, char *args[])
{
    FILE *fd;
    FILE *dst = NULL;
    char *tmp;
    int   i, c;
    int   retval = 0;

    /* Redirección: cat <file>... > <outfile> */
    if (nargs >= 3 && strcmp(args[nargs - 2], ">") == 0) {
        tmp = full_path(args[nargs - 1]);
        if (tmp == NULL)
            return 1;

        dst = fopen(tmp, "w");
        if (dst == NULL) {
            fprintf(stdout,"%s: cannot create %s\n", args[0], args[nargs - 1]);
            return 1;
        }
        nargs -= 2;                 /* '>' y outfile ya procesados */
    }

    if (nargs < 2) {
        fprintf(stdout,"Use: %s <file>... [> outfile]\n", args[0]);
        if (dst)
            fclose(dst);
        return 1;
    }

    for (i = 1; i < nargs; i++) {
        tmp = full_path(args[i]);
        if (tmp == NULL) {
            fprintf(stdout,"%s: invalid path: %s\n", args[0], args[i]);
            retval = 1;
            continue;
        }

        fd = fopen(tmp, "r");
        if (fd == NULL) {
            fprintf(stdout,"%s: %s not found\n", args[0], args[i]);
            retval = 1;
            continue;
        }

        /* CLAVE: no confiar en fgetc()==EOF con fat_filelib.
           feof() compara fileptr con filesize y corta seguro. */
        while (!feof(fd)) {
            c = fgetc(fd);
            if (c == EOF)           /* por si hay error a mitad */
                break;
            if (dst)
                fputc(c, dst);
            else
                putc(stdout, c);
        }

        fclose(fd);
    }

    if (dst)
        fclose(dst);

    update_path();
    return retval;
}