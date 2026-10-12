/* kprintf.c -  kputc, kgetc, kprintf */
/* Kprintf uses a direct method to write to serial
 * Used for debugging purposes. Should in normal cases
 * use printf that does a syscall
 * */

#include <xinu.h>
#include <stdarg.h>
#include <syscall.h>

#include "tusb.h"
/*------------------------------------------------------------------------
 * kputc - use polled I/O to write a character to the console serial line
 *------------------------------------------------------------------------
 */
syscall kputc(
	  byte	c			/* character to write		*/
	)
{
	intmask mask;
	int32 tries;

	mask = disable();
	if (tud_ready()) {
		/* Esperar (acotado) a que el FIFO TX del CDC acepte el
		 * byte. Se re-habilitan IRQs entre intentos para que el
		 * OTG_FS pueda drenar el buffer; si el host no lee, se
		 * descarta el byte tras el timeout (best-effort). */
		for (tries = 0; tries < 10000; tries++) {
			if (tud_cdc_write(&c, 1) == 1) {
				tud_cdc_write_flush();
				break;
			}
			restore(mask);
			disable();
		}
	}
	restore(mask);
	return OK;
}

/*------------------------------------------------------------------------
 * kgetc - use polled I/O to read a character from the console serial line
 *------------------------------------------------------------------------
 */
syscall kgetc(void)
{
	// Not used
}

extern	void	_doprnt(char *, va_list, int (*)(int));

/*------------------------------------------------------------------------
 * kprintf  -  use polled I/O to print formatted output on the console
 *------------------------------------------------------------------------
 */

 

syscall kprintf(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    _doprnt(fmt, ap, (int (*)(int))kputc);
    va_end(ap);
    return OK;
}


