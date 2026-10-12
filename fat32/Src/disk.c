#if 0
#include <disk.h>
#include <w25qxxx.h>
#include "sd-spi.h"
#include <stdio.h>
#include <xinu.h>

 
extern const sd_card_drv_t sd_card_drv;
extern const w25qxxx_drv_t w25qxxx_drv;




int sd_init(void)
{
    

    w25qxxx_drv.init();

    flash_info_t *flash_info;
    flash_info = w25qxxx_drv.getcardinfo();
    return flash_info->card_size;


    sd_card_drv.init();
    if (sd_card_drv.reset() != sd_err_ok)
    {
        kprintf("error init memory\n");
        while(1);
    }
    sd_info_t *sd_info;
    sd_info = sd_card_drv.getcardinfo();
    return sd_info->card_size;

    
}
//-----------------------------------------------------------------
// sd_readsector: Read a number of blocks from SD card
//-----------------------------------------------------------------
int sd_readsector(unsigned int start_block, unsigned char *buffer, unsigned int sector_count)
{
    //disk_read (0, buffer, start_block, sector_count);
   //sd_card_drv.read(buffer, start_block, sector_count);
    w25qxxx_drv.read(buffer, start_block, sector_count);
    //readSector(FLASHSPI,buffer,start_block,sector_count);
    return 1;
}
//-----------------------------------------------------------------
// sd_writesector: Write a number of blocks to SD card
//-----------------------------------------------------------------
int sd_writesector(unsigned int start_block, unsigned char *buffer, unsigned int sector_count)
{
    //disk_write (0, buffer, start_block, sector_count);
   //sd_card_drv.write(buffer, start_block, sector_count);
    
    w25qxxx_drv.write(buffer, start_block, sector_count);
    //writeSector(FLASHSPI,buffer,start_block,sector_count);
    return 1;
}

#else
#include <disk.h>
#include <w25qxxx.h>
#include "sd-spi.h"
#include <stdio.h>
#include <xinu.h>

extern const sd_card_drv_t sd_card_drv;
extern const w25qxxx_drv_t w25qxxx_drv;

/* ------------------------------------------------------------------ */
/*  Gate de propiedad de la flash (LUN 0 MSC)                          */
/*                                                                     */
/*  msc_flash_is_detached() == 1  -> la flash es de Xinu: escribir OK   */
/*  msc_flash_is_detached() == 0  -> el host USB la tiene: BLOQUEAR     */
/*                                                                     */
/*  Este es el UNICO punto por donde pasa toda escritura de la         */
/*  libreria FAT (fopen "w", fputc, fwrite, remove, mkdir, format...), */
/*  porque fl_attach_media() registra estas funciones como diskio.     */
/* ------------------------------------------------------------------ */
extern int msc_flash_is_detached(void);

/* Diagnostico: escrituras rechazadas. Lo muestra 'fsstat'.
 * Definido aqui (global), declarado extern en xsh_fsstat.c */
uint32 fs_writes_blocked = 0;

int sd_init(void)
{
    flash_info_t *flash_info;

    w25qxxx_drv.init();

    flash_info = w25qxxx_drv.getcardinfo();
    if (flash_info == NULL)
        return 0;   /* antes dereferenciabas sin chequear: posible fault */

    return flash_info->card_size;

    /* NOTA: todo el codigo de la SD de aqui abajo es inalcanzable
     * (esta tras el return). Lo dejo comentado por si algun dia
     * queres reactivar la SD como medio de la libreria FAT:

    sd_card_drv.init();
    if (sd_card_drv.reset() != sd_err_ok)
    {
        kprintf("error init memory\n");
        while(1);
    }
    {
        sd_info_t *sd_info = sd_card_drv.getcardinfo();
        return sd_info->card_size;
    }
    */
}

//-----------------------------------------------------------------
// sd_readsector: Read a number of blocks from flash (W25Q128)
//
// La lectura NUNCA se bloquea: leer cuando el host es dueno no
// corrompe nada, y fsstat necesita fl_opendir() para reportar estado.
//-----------------------------------------------------------------
int sd_readsector(unsigned int start_block, unsigned char *buffer, unsigned int sector_count)
{
    w25qxxx_drv.read(buffer, start_block, sector_count);
    return 1;
}

//-----------------------------------------------------------------
// sd_writesector: Write a number of blocks to flash (W25Q128)
//
// GATE: si la flash esta montada por el host USB (Linux), rechazar
// toda escritura. Aunque un comando del shell se olvide de chequear
// el estado, es imposible corromper la FAT desde este lado.
//-----------------------------------------------------------------
int sd_writesector(unsigned int start_block, unsigned char *buffer, unsigned int sector_count)
{
    if (!msc_flash_is_detached()) {
        fs_writes_blocked++;    /* visible con 'fsstat' */
        return 0;               /* fallo: la libreria FAT aborta limpio */
    }

    w25qxxx_drv.write(buffer, start_block, sector_count);
    return 1;
}
#endif


