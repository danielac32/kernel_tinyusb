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

/* clkhandler.c - clkhandler */

#include <xinu.h>

extern	pid32	nullpid;	/* PID del proceso Null (idle)	*/
extern	uint32	nullTicks;	/* ticks de CPU idle (def. initialize.c) */
/*-----------------------------------------------------------------------
 * clkhandler - high level clock interrupt handler
 *-----------------------------------------------------------------------
 */

//void __attribute__ ((naked)) clkhandler()


extern	void	ttyhandler(uint32, char c, int tipo);
void TIM2_Handler()
{   

		/* Increment 1000ms counter */
	    TIM2->SR &= ~(1U << 0);

		count1000++;

		/* Si el CPU estaba corriendo el proceso Null, sumar tick
		 * de idle (usado por xsh_cpu para medir CPU disponible). */
		if (currpid == nullpid)
			nullTicks++;

		/* After 1 sec, increment clktime */

		if(count1000 >= 1000) {
			clktime++;
			count1000 = 0;

		}

		/* check if sleep queue is empty */
        if(ready_preemptive){
			if(!isempty(sleepq)) {
				/* sleepq nonempty, decrement the key of */
				/* topmost process on sleepq		 */

				if((--queuetab[firstid(sleepq)].qkey) == 0) {

					wakeup();
				}
			}

			/* Decrement the preemption counter */
			/* Reschedule if necessary	    */

			if((--preempt) == 0) {
				preempt = QUANTUM;
				// PendSV call
				PEND_SV();
			}
        }
}
