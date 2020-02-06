#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include<sys/ioctl.h>
 

#define SEL_CHNL _IOW('a','a',int32_t*)
#define SEL_ALIN _IOW('a','b',int32_t*)



int main()
{
        int fd;
        u_int32_t chnl,align;
	u_int16_t buffer;

 printf("\nOpening Driver\n");
        fd = open("/dev/adc8", O_RDWR);
        if(fd < 0) {
                printf("Cannot open device file...\n");
                return 0;
        }


printf("enter the channel for ADC:");
scanf("%d",&chnl);
printf("\n");

printf("enter the alignment:\n 1.right \n 2.left\n");
scanf("%d",&align);

ioctl(fd, SEL_CHNL,(int32_t*) &chnl);
ioctl(fd, SEL_ALIN,(int32_t*) &align); 


read(fd,&buffer,2);
printf("The ADC output is :%d\n",buffer);
printf("Closing Driver\n");
close(fd);
}
