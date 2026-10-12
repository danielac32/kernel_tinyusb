/* xsh_blink.c - xsh_blink */

#include <xinu.h>
#include <stdio.h>
#include <gpio.h>

/*------------------------------------------------------------------------
 * xsh_blink - parpadea el LED PC13 un numero de veces y sale
 *
 *	blink [veces]   (por defecto 10 toggles completos)
 *	Como no-builtin corre como hijo de la shell y termina, asi la
 *	shell no se queda esperando un proceso que nunca muere.
 *------------------------------------------------------------------------
 */
shellcmd xsh_blink(int nargs, char *args[])
{
	int32	times, i;

	if (nargs == 2 && strncmp(args[1], "--help", 7) == 0) {
		fprintf(stdout,"Use: %s [veces]\n\n", args[0]);
		fprintf(stdout,"Description:\n");
		fprintf(stdout,"\tParpadea el LED (PC13) el numero de veces indicado\n");
		fprintf(stdout,"Options:\n");
		fprintf(stdout,"\tveces\tnumber of times (default 10)\n");
		fprintf(stdout,"\t--help\tdisplay this help and exit\n");
		return 0;
	}

	times = 10;
	if (nargs >= 2) {
		times = atoi(args[1]);
		if (times < 1)
			times = 1;
		if (times > 1000)
			times = 1000;
	}

	for (i = 0; i < times; i++) {
		hw_toggle_pin(GPIOx(GPIO_C), 13);
		sleepms(200);
		hw_toggle_pin(GPIOx(GPIO_C), 13);
		sleepms(200);
	}
	return 0;
}