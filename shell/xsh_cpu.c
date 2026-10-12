/* xsh_cpu.c - xsh_cpu */

#include <xinu.h>

extern	uint32	nullTicks;	/* ticks (1 ms) de CPU idle	*/
extern	uint32	count1000;	/* fraccion de segundo (0..999)	*/
extern	uint32	clktime;	/* segundos desde el arranque	*/
extern	int32	sleepms(int32);

#define	CPU_WINDOW_MS	1000	/* ventana de medicion (1 s)	*/

/* Reloj monotonic en ms: clktime*1000 + count1000. Soportan los
 * resets de count1000 (cada segundo clktime sube y count1000=0). */
static uint32 now_ms(void)
{
	return clktime * 1000 + count1000;
}

/*------------------------------------------------------------------------
 * xsh_cpu - mide el CPU disponible durante una ventana fija
 *
 *	Mientras este comando duerme, el ticker (TIM2) cuenta cada tick
 *	de 1 ms en que el CPU corria el proceso Null (idle). La relacion
 *	nullTicks/totalTicks en la ventana = % de CPU disponible.
 *------------------------------------------------------------------------
 */
shellcmd xsh_cpu(int nargs, char *args[])
{
	uint32	t0, n0, ticks, idle, avail, used;
	int32	remaining;

	if (nargs == 2 && strncmp(args[1], "--help", 7) == 0) {
		fprintf(stdout,"Use: %s\n\n", args[0]);
		fprintf(stdout,"Description:\n");
		fprintf(stdout,"\tMide el CPU disponible (idle) durante 1 segundo\n");
		fprintf(stdout,"Options:\n");
		fprintf(stdout,"\t--help\t display this help and exit\n");
		return 0;
	}
	if (nargs > 1) {
		fprintf(stderr, "%s: too many arguments\n", args[0]);
		fprintf(stderr, "Try '%s --help' for more information\n",
			args[0]);
		return 1;
	}

	t0 = now_ms();
	n0 = nullTicks;

	/* Ventana de 1 segundo en tramos de 50 ms */
	remaining = CPU_WINDOW_MS;
	while (remaining > 0) {
		sleepms(50);
		remaining -= 50;
	}

	/* Calcular con aritmetica sin signo (resiste wrap de 32 bits) */
	ticks = (uint32)(now_ms() - t0);
	idle  = (uint32)(nullTicks - n0);
	if (ticks == 0)
		ticks = 1;
	avail = (idle * 100) / ticks;
	used  = 100 - avail;

	fprintf(stdout,"CPU (ventana %u ms)\r\n", (unsigned)ticks);
	fprintf(stdout,"  disponible: %u%%\r\n", (unsigned)avail);
	fprintf(stdout,"  en uso:     %u%%\r\n", (unsigned)used);
	return 0;
}