/* xsh_memstat.c - xsh_memstat */

#include <xinu.h>
#include <stdio.h>
#include <string.h>

static	void	printMemUse(void);
static	void	printFreeList(void);

/*------------------------------------------------------------------------
 * xsh_memstat - Print statistics about memory use and dump the free list
 *------------------------------------------------------------------------
 */
shellcmd xsh_memstat(int nargs, char *args[])
{

	/* For argument '--help', emit help about the 'memstat' command	*/

	if (nargs == 2 && strncmp(args[1], "--help", 7) == 0) {
		fprintf(stdout,"use: %s \n\n", args[0]);
		fprintf(stdout,"Description:\n");
		fprintf(stdout,"\tDisplays the current memory use and prints the\n");
		fprintf(stdout,"\tfree list.\n");
		fprintf(stdout,"Options:\n");
		fprintf(stdout,"\t--help\t\tdisplay this help and exit\n");
		return 0;
	}

	/* Check for valid number of arguments */

	if (nargs > 1) {
		fprintf(stderr, "%s: too many arguments\n", args[0]);
		fprintf(stderr, "Try '%s --help' for more information\n",
				args[0]);
		return 1;
	}



   struct	memblk	*memptr;	
	uint32	free_mem;	


    free_mem = 0;
	for (memptr = memlist.mnext; memptr != NULL;
						memptr = memptr->mnext) {
		free_mem += memptr->mlength;
	}
	fprintf(stdout,"free memory: %d \n", free_mem);


	
	printMemUse();
	printFreeList();

	return 0;
}


/*------------------------------------------------------------------------
 * printFreeList - Walk the list of free memory blocks and print the
 *			location and size of each
 *------------------------------------------------------------------------
 */
static void printFreeList(void)
{
	struct memblk *block;

	/* Output a heading for the free list */



	//fprintf(stdout,"Free List:\n");
	fprintf(stdout,"Block address  Length (dec)  Length (hex)\n");
	fprintf(stdout,"-------------  ------------  ------------\n");
	
	for (block = memlist.mnext; block != NULL; block = block->mnext) {
		fprintf(stdout,"  0x%08x    %9d     0x%08x\n", block,
			block->mlength, block->mlength);
	}
	fprintf(stdout,"\n");
}

extern void start(void);
extern void *_end;

/*------------------------------------------------------------------------
 * printMemUse - Print statistics about memory use
 *------------------------------------------------------------------------
 */
static void printMemUse(void)
{
	int i;				/* Index into process table	*/
	uint32 code = 0;		/* Total Xinu code memory	*/
	uint32 stack = 0;		/* Total used stack memory	*/
	uint32 mspstack = HANDLERSTACK; /* Total used handler stack mem */
	uint32 kheap = 0;		/* Free kernel heap memory	*/
	uint32 kfree = 0;		/* Total free memory		*/
	struct memblk *block;	 	/* Ptr to memory block		*/

	/* Calculate amount of text memory */

	code = (uint32)&_etext - (uint32)&_text;

	/* Calculate amount of allocated stack memory */
	/*  Skip the NULL process since it has a private stack */

	for (i = 0; i < NPROC; i++) {
		if (proctab[i].prstate != PR_FREE) {
			stack += (uint32)proctab[i].prstklen;
		}
	}

	/* Calculate the amount of memory on the free list */

	for (block = memlist.mnext; block != NULL; block = block->mnext) {
		kfree += block->mlength;
	}

	/* Calculate the amount of free kernel heap memory */

	kheap = kfree - stack;

	/* Output statistics on current memory use */

	fprintf(stdout,"Current system memory statistics:\n");
	fprintf(stdout,"---------------------------------\n");
	fprintf(stdout,"%10d bytes (0x%08x) of Xinu code\n", code, code);
	fprintf(stdout,"%10d bytes (0x%08x) of allocated stack space\n", stack, stack);
	fprintf(stdout,"%10d bytes (0x%08x) of allocated kernel stack space\n", mspstack, mspstack);
	fprintf(stdout,"%10d bytes (0x%08x) of available kernel heap space\n\n", kheap, kheap);
}
