#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "sys/ioctl.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include <poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>


int main(int argc,char **argv)
{
	int fd;
    int i,j;
	char *filename;
    char databuf[5];
	int ret = 0;

	if (argc != 2) {
		printf("Error Usage!\r\n");
		return -1;
	}

	filename = argv[1];
	fd = open(filename, O_RDWR);
	if(fd < 0) {
		printf("can't open file %s\r\n", filename);
		return -1;
	}

    while(1)
    {
        ret = read(fd, databuf, sizeof(databuf));
        if(ret == 5)
        {
            printf("read data is 0x%x 0x%x 0x%x 0x%x 0x%x\r\n",databuf[0],databuf[1],databuf[2],databuf[3],databuf[4]);
            for(i=1;i<=10;i++)
            {
                if(i<=4)
                    printf("ch %d short state:%d open state :%d\r\n",i,(databuf[0]>>(2*i-2)) & 0x1,(databuf[0]>>(2*i-1)) & 0x1);
                else if(i>8)
                    printf("ch %d short state:%d open state :%d\r\n",i,(databuf[2]>>(2*(i-8)-2)) & 0x1,(databuf[2]>>(2*(i-8)-1)) & 0x1);
                else
                    printf("ch %d short state:%d open state :%d\r\n",i,(databuf[1]>>(2*(i-4)-2)) & 0x1,(databuf[1]>>(2*(i-4)-1)) & 0x1);
            }
        }
        usleep(1000000); 
    }
    close(fd);
    return 0;

}