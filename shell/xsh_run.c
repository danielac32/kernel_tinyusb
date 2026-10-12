/* xsh_echo.c - xsh_echo */

#include <xinu.h>
#include <fat_filelib.h>
#include <elf.h>

shellcmd xsh_run(int nargs, char *args[])
{
	char *tmp;
	exec_img ximg;
	int32	tmparg;
	int child;

	if (nargs < 2) {
		fprintf(stdout,"Use: %s <elf-file>\n", args[0]);
		return 1;
	}
	tmp = full_path(args[1]);
	if (tmp==NULL)return -1;

    int ret = elf_execve(tmp,&ximg);
    if(ret > 0){
        int (*p) = (int *)ret;
        child = create(p,
			SHELL_CMDSTK, SHELL_CMDPRIO,
			tmp, 2, nargs, &args[0]);
        
        struct	procent *prptr;
        prptr = &proctab[child];
        prptr->elf = TRUE;
        prptr->img = (void *)ximg.start;

        resume(child);

    }else {
        fprintf(stdout,"error loading elf process  %d %s\n",ret,tmp);
    }

    update_path();


	 
	return 0;
}
