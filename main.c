#define _GNU_SOURCE
#define __USE_XOPEN2KXSI
#define __USE_XOPEN

#include <stdio.h>
#include "../ftd2xx.h"

#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int term = posix_openpt(O_RDWR|O_NOCTTY);
    unlockpt(term);
//     int slaveterm = open(ptsname(term), O_RDWR|O_NOCTTY);
    char* pts = ptsname(term);
    fprintf(stderr, "%s\n", pts);

	FT_STATUS	ftStatus = FT_OK;
	FT_HANDLE	ftHandle;
	int         portNumber;
	
	if (argc > 1) 
	{
		sscanf(argv[1], "%d", &portNumber);
	}
	else 
	{
		portNumber = 0;
	}
	
	ftStatus = FT_Open(portNumber, &ftHandle);
	if (ftStatus != FT_OK) 
	{
		/* FT_Open can fail if the ftdi_sio module is already loaded. */
		printf("FT_Open(%d) failed (error %d).\n", portNumber, (int)ftStatus);
		printf("Use lsmod to check if ftdi_sio (and usbserial) are present.\n");
		printf("If so, unload them using rmmod, as they conflict with ftd2xx.\n");
		return 1;
	}

	/* Enable bit-bang mode, where 8 UART pins (RX, TX, RTS etc.) become
	 * general-purpose I/O pins.
	 */
	printf("Selecting cbus bit-bang mode.\n");
	ftStatus = FT_SetBitMode(ftHandle, 
	                         0xFF,
	                         FT_BITMODE_CBUS_BITBANG);
	if (ftStatus != FT_OK) 
	{
		printf("FT_SetBitMode failed (error %d).\n", (int)ftStatus);
        goto exit;
	}

    while ( 1 ) {
        char buf[255];
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(term, &rfds);
        struct timeval timeout = {0, 100000};
        int ret = select(FD_SETSIZE, &rfds, NULL, NULL, &timeout);
        printf("select returned %i\n", ret);
        if ( ret == 1 ) {
            int size_read = read(term, buf, 255);
            buf[size_read] = 0;
            if ( size_read == -1 && errno == EIO ) {
                printf("I/O error in read() from pty\n");
                goto exit;
            }
            printf("Got %i bytes of data (errno %i): '%s'\n", size_read, errno, buf);
            unsigned int written = 0;
            /* Write */
            ftStatus = FT_Write(ftHandle, buf, size_read, &written);
            if (ftStatus != FT_OK) {
                printf("Error FT_Write(%d)\n", (int)ftStatus);
                goto exit;
            }
            if ((int)written != size_read) {
                printf("FT_Write only wrote %d (of %d) bytes\n",
                        (int)written,
                        (int)size_read);
                goto exit;
            }
            printf("wrote %i bytes to uart\n", written);
        }

        /* Read */
        unsigned int avail = 0;
        ftStatus = FT_GetQueueStatus(ftHandle, &avail);
        if(avail > 0 && ftStatus == FT_OK) {
            char reply[255];
            unsigned int read_size = 0;
            printf("Calling FT_Read, avail: %i\n", avail);
            ftStatus = FT_Read(ftHandle, reply, avail, &read_size);
            if (ftStatus != FT_OK) {
                printf("Error FT_Read(%d)\n", (int)ftStatus);
                goto exit;
            }
            if (read_size != avail) {
                printf("FT_Read only read %d (of %d) bytes\n",
                        (int)read_size,
                        (int)avail);
                goto exit;
            }
            printf("FT_Read read %d bytes.  Read-buffer is now: %s\n",
                    (int)read_size, reply);
            write(term, buf, read_size);
        }

        usleep(1000);
    }

exit:
	/* Return chip to default (UART) mode. */
	(void)FT_SetBitMode(ftHandle, 
	                    0, /* ignored with FT_BITMODE_RESET */
	                    FT_BITMODE_RESET);

	(void)FT_Close(ftHandle);
	return 0;
}
