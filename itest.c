#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include "filter.h"

int main()
{
	int fd, result, len;
	char buf[10];
	char *str;
	
	if ((fd = open("/dev/filter1", O_RDONLY)) == -1)
	{
		perror("1. open failed");
		return -1;
	}
	
	ioctl(fd, FILTER_IOCTADDTAG, 2, 'A');
	
	close(fd);
	return 0;
}
	

