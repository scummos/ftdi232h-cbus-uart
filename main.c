#define _GNU_SOURCE
#define __USE_XOPEN2KXSI
#define __USE_XOPEN

#include <stdio.h>
#include "../ftd2xx.h"

#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#define MODE_ISP 1
#define MODE_RUN 0

void generate_reset(FT_HANDLE ftHandle, int isp) {
    printf("Resetting device\n");
    // Reset: CBUS 6; ISP: CBUS 5
    // reset
    FT_SetBitMode(ftHandle,
                  0xF0,
                  FT_BITMODE_CBUS_BITBANG);
    // ISP bit high or low
    FT_SetBitMode(ftHandle,
                  0xF0 + (isp ? 0 : 1),
                  FT_BITMODE_CBUS_BITBANG);
    usleep(1000);
    // continue
    FT_SetBitMode(ftHandle,
                  0xF2 + (isp ? 0 : 1),
                  FT_BITMODE_CBUS_BITBANG);
    printf("Device reset completed\n");
}

int main()
{
    int term = posix_openpt(O_RDWR|O_NOCTTY);
    unlockpt(term);
    char* pts = ptsname(term);
    fprintf(stderr, "%s\n", pts);

    FT_STATUS    ftStatus = FT_OK;
    FT_HANDLE    ftHandle;
    int         portNumber = 0;

    ftStatus = FT_Open(portNumber, &ftHandle);
    if (ftStatus != FT_OK)
    {
        /* FT_Open can fail if the ftdi_sio module is already loaded. */
        printf("FT_Open(%d) failed (error %d).\n", portNumber, (int)ftStatus);
        printf("Use lsmod to check if ftdi_sio (and usbserial) are present.\n");
        printf("If so, unload them using rmmod, as they conflict with ftd2xx.\n");
        return 1;
    }

    FT_SetBaudRate(ftHandle, 115200);

    // Enter ISP mode on startup.
    generate_reset(ftHandle, MODE_ISP);

    while ( 1 ) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(term, &rfds);
        struct timeval timeout = {0, 100};
        // wait for data from terminal
        int ret = select(FD_SETSIZE, &rfds, NULL, NULL, &timeout);
        if ( ret == 1 ) {
            char buf[256];
            int size_read = read(term, buf, 255);
            if ( size_read == -1 && errno == EIO ) {
                printf("I/O error in read() from pty\n");
                goto exit;
            }
            printf("Got %i bytes of data (errno %i): '%s'\n", size_read, errno, buf);
            unsigned int written = 0;

            // write data to UART
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

        // read data from UART
        unsigned int avail = 0;
        ftStatus = FT_GetQueueStatus(ftHandle, &avail);
        if(avail > 0 && ftStatus == FT_OK) {
            unsigned char reply[256];
            memset(reply, 0, 255);
            unsigned int read_size = 0;
            printf("Calling FT_Read, avail: %i\n", avail);
            ftStatus = FT_Read(ftHandle, reply, avail > 255 ? 255 : avail, &read_size);
            if (ftStatus != FT_OK) {
                printf("Error FT_Read(%d)\n", (int)ftStatus);
                goto exit;
            }
            printf("FT_Read read %d bytes.\n", read_size);
            // pass on data to terminal
            int result = write(term, reply, read_size);
            if ( result == -1 ) {
                printf("write failed, errno %i\n", errno);
            }
        }
    }

exit:
    // set uC to run.
    generate_reset(ftHandle, MODE_RUN);

    /* Return chip to default (UART) mode. */
    (void)FT_SetBitMode(ftHandle,
                        0, /* ignored with FT_BITMODE_RESET */
                        FT_BITMODE_RESET);

    (void)FT_Close(ftHandle);

    return 0;
}
