/* xsh_fsstat.c - xsh_fsstat: estado del FS + propiedad de la flash
 *
 * Control de propiedad del W25Q128 (LUN 0 MSC):
 *   - Al encender: la flash es del HOST USB (default seguro).
 *     Xinu NO puede escribir (gate en sd_writesector activo).
 *   - 'fsstat xinu': retira la flash del host, Xinu puede escribir.
 *   - 'fsstat host': devuelve la flash al host (sincroniza FAT antes).
 */

#include <xinu.h>
#include <fat_filelib.h>
#include <w25qxxx.h>

extern uint32 fs_writes_blocked;          /* definido en el diskio */
extern void   msc_flash_detach(void);     /* flash fuera del host -> Xinu escribe */
extern void   msc_flash_attach(void);     /* flash al host -> gate bloquea */
extern int    msc_flash_is_detached(void);
extern uint32_t usb_sd_size(void);

/* ------------------------------------------------------------------ */
/*  Help                                                              */
/* ------------------------------------------------------------------ */
static void print_help(char *cmd)
{
    fprintf(stdout,"Use: %s [comando]\r\n\r\n", cmd);

    fprintf(stdout,"Description:\r\n");
    fprintf(stdout,"\tMuestra el estado del sistema de archivos y controla quien\r\n");
    fprintf(stdout,"\tes dueno de la flash W25Q128: el host USB (Linux) o Xinu.\r\n");
    fprintf(stdout,"\tSolo uno de los dos puede escribirla a la vez.\r\n\r\n");

    fprintf(stdout,"Comandos:\r\n");
    fprintf(stdout,"\t(sin argumentos)  muestra el estado completo\r\n");
    fprintf(stdout,"\txinu              retira la flash del host: Xinu PUEDE escribir\r\n");
    fprintf(stdout,"\thost              devuelve la flash al host: Xinu NO puede escribir\r\n");
    fprintf(stdout,"\thelp              muestra esta ayuda\r\n\r\n");

    fprintf(stdout,"Estado por defecto al encender:\r\n");
    fprintf(stdout,"\tDueno: HOST USB. Escritura desde Xinu: BLOQUEADA.\r\n");
    fprintf(stdout,"\t(Es el default seguro: si Linux tiene la unidad montada,\r\n");
    fprintf(stdout,"\t las escrituras de Xinu son rechazadas y no hay corrupcion.)\r\n\r\n");

    fprintf(stdout,"Flujo tipico:\r\n");
    fprintf(stdout,"\t1. En Linux:  sync && umount /media/...\r\n");
    fprintf(stdout,"\t2. En Xinu:   fsstat xinu\r\n");
    fprintf(stdout,"\t3. En Xinu:   cat, del, mkdir, ...  (escrituras permitidas)\r\n");
    fprintf(stdout,"\t4. En Xinu:   fsstat host   (sincroniza la FAT y devuelve al host)\r\n");
    fprintf(stdout,"\t5. En Linux:  partprobe o re-montar para ver los cambios\r\n\r\n");

    fprintf(stdout,"Notas:\r\n");
    fprintf(stdout,"\t- La LECTURA siempre esta permitida (no corrompe nada).\r\n");
    fprintf(stdout,"\t- 'fsstat host' ejecuta update_path() automaticamente:\r\n");
    fprintf(stdout,"\t  no hace falta cerrar archivos a mano, pero es recomendable.\r\n");
    fprintf(stdout,"\t- Si eject /dev/sdX desde Linux sobre la flash, la propiedad\r\n");
    fprintf(stdout,"\t  pasa a Xinu automaticamente.\r\n");
}

/* ------------------------------------------------------------------ */
/*  Estado completo                                                   */
/* ------------------------------------------------------------------ */
static void print_status(void)
{
    flash_info_t	*info;
    FL_DIR		dirstat;
    uint32		total_bytes;
    uint32		sd_sectors;
    int		xinu_owns;

    xinu_owns = msc_flash_is_detached();

    fprintf(stdout,"--- Estado del sistema de archivos ---\r\n");
    fprintf(stdout,"Propiedad flash: %s%s\r\n",
        xinu_owns ? "XINU (cambiado por el usuario)"
              : "HOST USB",
        xinu_owns ? "" : "  [default al encender]");
    fprintf(stdout,"Escritura Xinu : %s\r\n",
        xinu_owns ? "PERMITIDA"
              : "BLOQUEADA (gate activo en sd_writesector)");

    fprintf(stdout,"Rechazos gate  : %u\r\n", (unsigned)fs_writes_blocked);

    info = flash_spi_getcardinfo();
    if (info) {
        total_bytes = info->card_size * info->sect_size;
        fprintf(stdout,"Flash          : %u sectors x %u bytes = %u bytes\r\n",
            (unsigned)info->card_size,
            (unsigned)info->sect_size,
            (unsigned)total_bytes);
    } else {
        fprintf(stdout,"Flash          : info unavailable\r\n");
    }

    sd_sectors = usb_sd_size();
    if (sd_sectors)
        fprintf(stdout,"SD (LUN1)      : %u bytes (solo host, Xinu no la toca)\r\n",
            (unsigned)sd_sectors * 512);
    else
        fprintf(stdout,"SD (LUN1)      : not present\r\n");

    if (fl_opendir("/", &dirstat)) {
        fprintf(stdout,"FAT32          : mounted, root readable\r\n");
        fl_closedir(&dirstat);
    } else {
        fprintf(stdout,"FAT32          : NOT mounted\r\n");
    }

    /* Sugerencia segun el estado */
    if (xinu_owns)
        fprintf(stdout,"\r\nSugerencia: al terminar usa 'fsstat host' para devolver\r\n"
               "la flash a Linux.\r\n");
    else
        fprintf(stdout,"\r\nSugerencia: para escribir desde Xinu, primero\r\n"
               "'sync && umount' en Linux y luego 'fsstat xinu'.\r\n");
}

/* ------------------------------------------------------------------ */
/*  Comando principal                                                 */
/* ------------------------------------------------------------------ */
shellcmd xsh_fsstat(int nargs, char *args[])
{
    /* Help */
    if (nargs == 2 && (strcmp(args[1], "help") == 0 ||
               strcmp(args[1], "--help") == 0 ||
               strcmp(args[1], "-h") == 0)) {
        print_help(args[0]);
        return 0;
    }

    /* Tomar propiedad (Xinu escribe) */
    if (nargs == 2 && strcmp(args[1], "xinu") == 0) {
        if (msc_flash_is_detached()) {
            fprintf(stdout,"flash: ya es propiedad de Xinu.\r\n");
            return 0;
        }
        msc_flash_detach();
        fprintf(stdout,"flash: retirada del host. Xinu PUEDE escribir.\r\n");
        fprintf(stdout,"(recorda: sync && umount en Linux ANTES de este comando)\r\n");
        return 0;
    }

    /* Devolver al host (gate bloquea) */
    if (nargs == 2 && strcmp(args[1], "host") == 0) {
        if (!msc_flash_is_detached()) {
            fprintf(stdout,"flash: ya esta en poder del host.\r\n");
            return 0;
        }
        /* CRITICO: sincronizar la FAT del lado Xinu ANTES de
         * devolver el medio, para que Linux lea datos frescos */
        update_path();
        msc_flash_attach();
        fprintf(stdout,"flash: devuelta al host. Xinu NO puede escribir.\r\n");
        fprintf(stdout,"(en Linux: partprobe o re-montar para ver cambios)\r\n");
        return 0;
    }

    if (nargs != 1) {
        fprintf(stdout, "Use: %s [xinu|host|help]\r\n", args[0]);
        return 1;
    }

    print_status();
    return 0;
}