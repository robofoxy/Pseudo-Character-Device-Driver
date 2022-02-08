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
	char tags[FILTER_MAXTAG];
	
	if ((fd = open("/dev/filter1", O_RDONLY)) == -1)
	{
		perror("1. open failed");
		return -1;
	}
	
	memset(tags, 0, FILTER_MAXTAG);
	
	ioctl(fd, FILTER_IOCTADDTAG, 'B');
	ioctl(fd, FILTER_IOCTADDTAG, 'C');
	ioctl(fd, FILTER_IOCTADDTAG, 'D');
	ioctl(fd, FILTER_IOCTADDTAG, 'E');
	ioctl(fd, FILTER_IOCTADDTAG, 'K');
	ioctl(fd, FILTER_IOCTADDTAG, 'F');
	ioctl(fd, FILTER_IOCTADDTAG, 'G');
	ioctl(fd, FILTER_IOCTADDTAG, 'A');
	ioctl(fd, FILTER_IOCTADDTAG, 'H');
	ioctl(fd, FILTER_IOCTADDTAG, 'I');
	ioctl(fd, FILTER_IOCTADDTAG, 'J');
	ioctl(fd, FILTER_IOCTADDTAG, 'L');
	ioctl(fd, FILTER_IOCTADDTAG, 'M');
	ioctl(fd, FILTER_IOCTADDTAG, 'N');
	ioctl(fd, FILTER_IOCTADDTAG, 'O');
	
	len = ioctl (fd, FILTER_IOCGTAGS,tags);
	
	str = tags;
	
	for(int i = 0; i < 10; i++)
		printf("TAGS: %c %d\n", str[i], len);
	
	ioctl(fd, FILTER_IOCTRMTAG, 'B');
	memset(tags, 0, FILTER_MAXTAG);
	ioctl (fd, FILTER_IOCGTAGS,tags);
	printf("TAGS: %s\n", tags);
	
	ioctl(fd, FILTER_IOCCLRFILTER);
	memset(tags, 0, FILTER_MAXTAG);
	ioctl (fd, FILTER_IOCGTAGS,tags);
	printf("TAGS: %s\n", tags);
	
	close(fd);
	return 0;
}
	









