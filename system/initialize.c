/* Xinu for STM32
 *
 * Original license applies
 * Modifications for STM32 by Robin Krens
 * Please see LICENSE and AUTHORS 
 * 
 * $LOG$
 * 2019/11/11 - ROBIN KRENS
 * Initial version 
 * 
 * $DESCRIPTION$
 *
 * */

/* initialize.c - nulluser, sysinit */

/* Handle system initialization and become the null process */

#include <xinu.h>
#include <gpio.h>
#include <disk.h>
#include <fat_filelib.h>
#include <string.h>
#include <syscall.h>
#include <w25qxxx.h>
#include "tusb.h"
#include "usb_app.h"

syscall_t syscallp;
extern void start(void);	/* Start of Xinu code			*/
extern	void	*_end;		/* End of Xinu code			*/

/* Function prototypes */

static	void sysinit(); 	/* Internal system initialization	*/
extern	void meminit(void);	/* Initializes the free memory list	*/
extern int ready_preemptive;

/* Declarations of major kernel variables */

struct	procent	proctab[NPROC];	/* Process table			*/
struct	sentry	semtab[NSEM];	/* Semaphore table			*/
struct	memblk	memlist;	/* List of free memory blocks		*/

/* Active system status */

int	prcount;		/* Total number of live processes	*/
pid32	currpid;		/* ID of currently executing process	*/

/* Descriptor/accounting for the idle (null) process */

uint32	idleTicks;		/* Count of iterations of null port	*/
pid32	nullpid;		/* PID del proceso Null (idle)		*/
uint32	nullTicks = 0;		/* ticks (1 ms) en que el CPU estuvo	*/
				/*   corriendo el proceso Null.		*/

/* DIAG: boot milestone marker (readable over SWD) */
volatile uint32 dbg_magic = 0;

/*------------------------------------------------------------------------
 * boot_read_config  -  Lee la primera linea de /config/kernel (si existe)
 *			de forma segura en un buffer del llamador.
 *			Devuelve el numero de caracteres, o -1 si falla.
 *------------------------------------------------------------------------
 */
static int boot_read_config(char *buf, size_t n)
{
	FILE	*fptr;
	char	*p;

	if (n == 0)
		return -1;
	buf[0] = NULLCH;
	if ((fptr = fopen("/config/kernel", "r")) == NULL)
		return -1;

	/* Leer el tamano del archivo y recortar la linea a n-1 chars */
	fseek(fptr, 0, SEEK_END);
	int lsize = ftell(fptr);
	fseek(fptr, 0, SEEK_SET);
	if (lsize < 0)
		lsize = 0;
	if (lsize > (int)n - 1)
		lsize = (int)n - 1;
	if (lsize > 0) {
		if (fread(buf, 1, lsize, fptr) != (size_t)lsize) {
			fclose(fptr);
			buf[0] = NULLCH;
			return -1;
		}
	}
	fclose(fptr);
	buf[lsize] = NULLCH;

	/* Quitar salto/saltos de linea finales */
	p = buf + strlen(buf);
	while (p > buf && (p[-1] == '\n' || p[-1] == '\r'))
		*--p = NULLCH;

	return (int)strlen(buf);
}

/*------------------------------------------------------------------------
 * initFat32  -  Inicializa la W25Q128 y monta FAT32. Se ejecuta como
 *			proceso one-shot al arrancar el sistema.
 *------------------------------------------------------------------------
 */
int initFat32()
{
	uint32 size = sd_init();
	(void)size;

	fl_init();

	/* Attach media access functions to library */
	if (fl_attach_media(sd_readsector, sd_writesector) != FAT_INIT_OK)
	{
		printf("ERROR: Failed to init file system\n");
		return -1;
	}

	/* Tarjeta SD: LUN 1 del MSC (la detecta el host como 2.º disco) */
	int sdsize = usb_sd_init();
	if (sdsize < 0)
		printf("sd: not present\r\n");
	else
		printf("sd: mounted %d sectors\r\n", sdsize);

	/* Mostrar el kernel configurado (si existe) */
	char cfg[80];
	if (boot_read_config(cfg, sizeof(cfg)) >= 0)
		printf("boot: %s\n", cfg);
	else
		printf("boot: (no /config/kernel)\r\n");

	return OK;
}

/*------------------------------------------------------------------------
 * blink  -  Tarea de demostracion: parpadea el LED PC13
 *------------------------------------------------------------------------
 */
int blink()
{
	while (1) {
		hw_toggle_pin(GPIOx(GPIO_C), 13);
		sleepms(500);
	}
	return OK;
}

/*------------------------------------------------------------------------
 * nullprocess  -  Proceso Null: arranca los servicios del sistema y
 *			queda haciendo idle.
 *------------------------------------------------------------------------
 */
int nullprocess(void)
{
	pid32	pid_usb, pid_blk, pid_sh, pid_fat;

	syscall_init(&syscallp);
	dbg_magic = 6;
    char num[10];
    sprintf(num, "%d", CONSOLE);
    /* Argumentos de la shell, igual que un comando: (nargs, arg[]).
     * arg[0] = device como cadena; los buffers viven mientras nullprocess
     * no retorne (nunca lo hace). */
    char *shell_argv[2];
    shell_argv[0] = num;
    shell_argv[1] = NULL;

	/* Crear (suspended) todas las tareas del sistema */
	pid_fat = create(initFat32, 1024, 50, "fat32", 0);
	pid_usb = create((void *)usbtask, 4096, 52, "usbtask", 0);
	pid_blk = create(blink, 4096, 52, "blink", 1, 0);
	pid_sh  = create(shell, 4096, 52, "shell", 2, 1, (uint32)shell_argv);

	/* Ponerlas en marcha. usbtask/blink/shell duermen enseguida,
	 * de modo que fat32 (prio 50) corre cuando ellas duermen; la shell
	 * se queda esperando la conexion CDC para mostrar el banner y el
	 * prompt. */
	ready(pid_usb);
	ready(pid_blk);
	ready(pid_sh);
	resume(pid_fat);

	while (1) {
		idleTicks++;
	}
	return OK;
}

/*------------------------------------------------------------------------
 * bootinfo  -  Informacion del sistema (build, memoria, codigo/datos).
 *			Se imprime por consola cuando el terminal se conecta.
 *------------------------------------------------------------------------
 */
void bootinfo(void)
{
    struct	memblk	*memptr;
    uint32	free_mem;

    printf("Build date: %s %s\n\n", __DATE__, __TIME__);
    free_mem = 0;
    for (memptr = memlist.mnext; memptr != NULL;
                        memptr = memptr->mnext) {
        free_mem += memptr->mlength;
    }
    printf("%10d bytes of free memory.  Free list:\n", free_mem);
    for (memptr = memlist.mnext; memptr != NULL;
                        memptr = memptr->mnext) {
        printf("           [0x%08X to 0x%08X]\n",
            (uint32)memptr, ((uint32)memptr) + memptr->mlength - 1);
    }

    printf("%10d bytes of Xinu code.\n",
        (uint32)&_etext - (uint32)&_text);
    printf("           [0x%08X to 0x%08X]\n",
        (uint32)&_text, (uint32)&_etext - 1);
    printf("%10d bytes of data.\n",
        (uint32)&_ebss - (uint32)&_sdata);
    printf("           [0x%08X to 0x%08X]\n\n",
        (uint32)&_sdata, (uint32)&_ebss - 1);

    /* ------------------------------------------------------------
     * Filesystem brief: el usuario debe saber que la flash es
     * compartida y quien es el dueno por defecto.
     * ----------------------------------------------------------- */
    printf("--- Sistema de archivos: FAT32 en flash W25Q128 ---\n");
    printf("La flash es COMPARTIDA: la PC (host USB) y Xinu se la\n");
    printf("pasan turnandose - nunca los dos a la vez.\n");
    printf("Al encender la flash pertenece al HOST USB, por lo que\n");
    printf("las escrituras desde este shell estan BLOQUEADAS\n");
    printf("(gate de proteccion activo).\n");
    printf("  fsstat           ver estado y dueno del sistema de archivos\n");
    printf("  fsstat xinu      tomar posesion (antes: sync && umount en la PC!)\n");
    printf("  fsstat host      devolverla a la PC y sincronizar\n");
    printf("La tarjeta SD (LUN 1) es solo de la PC; Xinu no la toca.\n");
    printf("Escriba 'help' para ver la lista de comandos.\n\n");
}
/*------------------------------------------------------------------------
 * start_null_process  -  Arranca el proceso Null sobre su propio contexto
 *
 * El proceso se crea con create() quedando en PR_CURR. Aqui se apunta PSP
 * al marco inicial que create() dejo en la pila, se precargan los registros
 * callee-saved r4-r11 (los que PendSV guarda/restaura en cada cambio de
 * contexto) y se pasa a modo thread sobre PSP (CONTROL=2). Despues se
 * invoca nullprocess(), que no retorna.
 *
 * Es el unico "salto inicial" al primer proceso del sistema; a partir de
 * aqui todos los cambios de contexto los hace el scheduler (PendSV).
 *------------------------------------------------------------------------
 */
static void start_null_process(struct procent *prptr)
{
	uint32	*stkp = prptr->prstkptr;

	asm volatile (
		"mov r0, %0\n"
		"msr psp, r0\n"			/* PSP = marco inicial        */
		"ldmia r0!, {r4-r11}\n"		/* precargar callee-saved     */
		"msr psp, r0\n"			/* PSP tras el marco de 8     */
		"mov r0, #2\n"			/* CONTROL=2: PSP, privilegiado */
		"msr control, r0\n"
		"isb\n"
		: : "r" (stkp) : "r0");
	nullprocess();			/* nunca retorna */
}

void	nulluser()
{	
    hw_cfg_pin(GPIOx(GPIO_C),13,GPIOCFG_MODE_OUT | GPIOCFG_OSPEED_VHIGH  | GPIOCFG_OTYPE_PUPD | GPIOCFG_PUPD_PUP);
    hw_cfg_pin(GPIOx(GPIO_A),0,GPIOCFG_MODE_INP | GPIOCFG_OSPEED_VHIGH  | GPIOCFG_OTYPE_OPEN | GPIOCFG_PUPD_PUP);

    meminit();
    dbg_magic = 2;
	platinit();
    dbg_magic = 3;
    /* Enable interrupts */
	enable();

	/* Initialize the system */
   
    /* Short wait for the boot (BOOT0/PA0) button; boot anyway after ~2 s */
	{
	     uint32 bcnt = 0;
      while(1){
        if(!hw_get_pin(GPIOx(GPIO_A),0)) break;
        hw_toggle_pin(GPIOx(GPIO_C),13);
        delay(50);
        if (++bcnt >= 40) break;
      }
	}
	
sysinit();
    dbg_magic = 4;
     
	/* Initialize the Null process entry */	
	int pid = create((void *)nullprocess, 1024, 10, "Null process", 0, NULL);
	nullpid = pid;			/* para medir CPU idle en el ticker	*/
	struct procent * prptr = &proctab[pid];
	prptr->prstate = PR_CURR;
    

	
	/* Initialize the real time clock */
	clkinit();

    TIM2->CR1 |= (1 << 0); 
    ready_preemptive=1;
    NVIC_EnableIRQ(TIM2_IRQn);

    /* Start the TinyUSB device (CDC console + MSC over W25Q128).
     * Same context/order as core_blank: boot context (MSP), right after
     * the clock tickers, before switching to the null process. */
    tusb_init();
    dbg_magic = 5;
    
    start_null_process(prptr);

	for(;;);

}

/*------------------------------------------------------------------------
 *
 * sysinit  -  Initialize all Xinu data structures and devices
 *
 *------------------------------------------------------------------------
 */

static	void	sysinit()
{
	int32	i;
	struct	procent	*prptr;		/* Ptr to process table entry	*/
	struct	sentry	*semptr;	/* Ptr to semaphore table entry	*/

	/* Initialize system variables */

	/* Count the Null process as the first process in the system */
	prcount = 0;

	/* Scheduling is not currently blocked */

	Defer.ndefers = 0;

	/* Initialize process table entries free */

	for (i = 0; i < NPROC; i++) {
		prptr = &proctab[i];
		prptr->prstate = PR_FREE;
		prptr->prname[0] = NULLCH;
		prptr->prstkbase = NULL;
		prptr->prprio = 0;
	}


	/* Initialize semaphores */

	for (i = 0; i < NSEM; i++) {
		semptr = &semtab[i];
		semptr->sstate = S_FREE;
		semptr->scount = 0;
		semptr->squeue = newqueue();
	}

	
	readylist = newqueue();

	
	for (i = 0; i < NDEVS; i++) {
		init(i);
	}
	return;
}