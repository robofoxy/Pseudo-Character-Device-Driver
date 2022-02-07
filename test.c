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
	
	if ((fd = open("/dev/filter1", O_WRONLY)) == -1)
	{
		perror("1. open failed");
		return -1;
	}
	
	memset(buf, 0, 10);
	memset(buf, 'a', 5);

	struct filter_message *p = malloc(sizeof(struct filter_message) + 10);
	strncpy(p->body, buf, 10);
	
	str = p->body;
	
	printf("BODY: %s %d\n", p->body, strlen(p->body));

	if ((result = write (fd, p, sizeof(struct filter_message) + 10)) != 5)
	{
		perror("writing");
		return -1;
	}
	
	close(fd);
	return 0;
}
	

