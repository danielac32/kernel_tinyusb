/* xsh_format.c - xsh_format */

#include <xinu.h>
#include <fat_filelib.h>
#include <w25qxxx.h>
#include "sd-spi.h"
#include <string.h>

/*------------------------------------------------------------------------
 * xsh_format - formatea el filesystem FAT32 en la flash W25Q128
 *
 *	IMPORTANTE: destructivo. Requiere confirmacion explicita:
 *	format yes
 *------------------------------------------------------------------------
 */

extern const sd_card_drv_t sd_card_drv;
shellcmd xsh_format(int nargs, char *args[])
{
	flash_info_t *flash_info;

	if (nargs == 2 && strncmp(args[1], "--help", 7) == 0) {
		fprintf(stdout,"Use: %s yes\n\n", args[0]);
		fprintf(stdout,"Description:\n");
		fprintf(stdout,"\tFormatea el filesystem FAT32 de la flash (DESTRUCTIVO)\n");
		fprintf(stdout,"Options:\n");
		fprintf(stdout,"\tyes\tconfirmacion obligatoria\n");
		fprintf(stdout,"\t--help\tdisplay this help and exit\n");
		return 0;
	}

	if (nargs != 2 || strncmp(args[1], "yes", 3) != 0) {
		fprintf(stderr, "%s: peligro, borra TODO el contenido\n", args[0]);
		fprintf(stderr, "Para confirmar: %s yes\n", args[0]);
		return 1;
	}

	flash_info = w25qxxx_drv.getcardinfo();
    //sd_info_t *sd_info;
    //sd_info = sd_card_drv.getcardinfo();

	fprintf(stdout,"formating...\n");
	//if (fl_format(sd_info->card_size, "")) {
	if (fl_format(flash_info->card_size, "")) {
		fprintf(stdout,"format ok\n");
		return 0;
	}
	fprintf(stdout,"error formating\n");
	return 1;
}