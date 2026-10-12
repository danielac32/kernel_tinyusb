/* debug.c  -  hexdump_print, hexdump */

#include <xinu.h>
//#include <stdio.h>
//#include <debug.h>

#define DEBUG_ASCII 0x01
#define DEBUG_HEX   0x02


static void hexdump_print(byte, byte);
static void hexdump_print(
	 byte	data,			/* Item to print		*/
	 byte	mode			/* ASCII or hex mode		*/
	)
{
    switch (mode)
    {
    case DEBUG_ASCII:
        data = (' ' <= data && data <= '~') ? data : '.';
        fprintf(stdout, "%c", data);
        break;
    case DEBUG_HEX:
        fprintf(stdout, "%02x ", data);
        break;
    default:
        break;
    }
}


void	hexdump(
	 void	*buffer,		/* Addresss of memory area	*/
	 uint32	length,			/* Length in bytes		*/
	 bool	canon			/* Print in ASCII or hex	*/
	)
{
    uint32 m, n, remain;

    byte *b = (byte *)buffer;

    for (n = 0; n < length; n += 0x10) {
        fprintf(stdout, "%08x ",  n);

        remain = length - n;

        for (m = 0; m < remain && m < 0x10; m++) {
            if (m % 0x08 == 0) {
                fprintf(stdout, " ");
            }
            hexdump_print(b[n + m], DEBUG_HEX);
        }

        /* Pad the rest if needed */
        if (remain < 0x10) {
            for (m = 0; m < 0x10 - remain; m++) {
                if ((0 != m) && (0 == m % 0x08)) {
                    fprintf(stdout, " ");
                }
                fprintf(stdout, "   ");
            }
        }

        if (canon == TRUE) {
            fprintf(stdout, " |");
            for (m = 0; m < remain && m < 0x10; m++) {
                hexdump_print(b[n + m], DEBUG_ASCII);
            }
            fprintf(stdout, "|");
        }
        fprintf(stdout, "\n");
    }
}



void    hexdump2(
     void   *buffer,        /* Addresss of memory area  */
     uint32 length,         /* Length in bytes      */
     uint32 offset,
     bool  canon           /* Print in ASCII or hex    */
    )
{
    uint32 m, n, remain;

    byte *b = (byte *)buffer;

    for (n = 0; n < length; n += 0x10) {
        fprintf(stdout, "%08x ",  n+offset);

        remain = length - n;

        for (m = 0; m < remain && m < 0x10; m++) {
            if (m % 0x08 == 0) {
                fprintf(stdout, " ");
            }
            hexdump_print(b[n + m], DEBUG_HEX);
        }

        /* Pad the rest if needed */
        if (remain < 0x10) {
            for (m = 0; m < 0x10 - remain; m++) {
                if ((0 != m) && (0 == m % 0x08)) {
                    fprintf(stdout, " ");
                }
                fprintf(stdout, "   ");
            }
        }

        if (canon == TRUE) {
            fprintf(stdout, " |");
            for (m = 0; m < remain && m < 0x10; m++) {
                hexdump_print(b[n + m], DEBUG_ASCII);
            }
            fprintf(stdout, "|");
        }
        fprintf(stdout, "\n");
    }
}

void hexDump2(uint32 offset, void *addr, int len) 
{
    int i;
    unsigned char buff[17];
    unsigned char *pc = addr;

    
    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0){
                fprintf(stdout,"  %s\n", buff);
                
            }

            // Output the offset.
            fprintf(stdout,"  %04x ", offset+i);
            
        }

        // Now the hex code for the specific character.
        fprintf(stdout," %02x", pc[i]);
        

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        fprintf(stdout,"   ");
        
        i++;
    }

    // And print the final ASCII bit.
    fprintf(stdout,"  %s\n", buff);
    
}