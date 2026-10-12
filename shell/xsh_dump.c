/* xsh_echo.c - xsh_echo */

#include <xinu.h>
#include <fat_filelib.h>

shellcmd xsh_dump(int nargs, char *args[])
{
	 
	FILE* fd;
	int rd;
	char *tmp;

	if (nargs != 2) {
		fprintf(stdout,"Use: %s <file>\n", args[0]);
		return 1;
     }

    tmp = full_path(args[1]);
    if (tmp==NULL)return -1;
    fprintf(stdout,"%s\n",tmp );
    if (!(fd = fopen(tmp,"r"))){
        fprintf(stdout,"%s not found\n", tmp);
        return -1;
    }
    uint8 buff[32];
    int part=0;
    fseek(fd, 0, SEEK_END);
    unsigned int fileLength = ftell(fd);
    fseek(fd, 0, SEEK_SET);

    fprintf(stdout,"size file %d\n",fileLength);

    while(part<fileLength){
         memset(buff,0,32);
         rd = fread(buff, 1, 32, fd);
         if (rd <= 0)
             break;
         hexDump2(part,buff,rd);
         part += rd;
    }
    fclose(fd);


    update_path();
	return 0;
}
