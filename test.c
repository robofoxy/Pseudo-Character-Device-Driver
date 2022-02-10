#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include "filter.h"

int main()
{
	int fd, result, len;
	unsigned char buf[10];
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
	p->tag = 'D';
	
	printf("sent: %c %s\n", p->tag, p->body);

	if ((result = write (fd, p, sizeof(struct filter_message) + 10)) != 5)
	{
		perror("writing");
		return -1;
	}
	printf("result: %d\n", result);

	struct filter_message *p2 = malloc(sizeof(struct filter_message) + 10);
	strncpy(p2->body, buf, 10);
	p2->tag = 'E';
	printf("sent: %c %s\n", p2->tag, p2->body);

	if ((result = write (fd, p2, sizeof(struct filter_message) + 10)) != 5)
	{
		perror("writing");
		return -1;
	}
	printf("result: %d\n", result);
	
	struct filter_message *p3 = malloc(sizeof(struct filter_message) + 10);
	strncpy(p3->body, buf, 10);
	p3->tag = 'F';
	printf("sent: %c %s\n", p3->tag, p3->body);

	if ((result = write (fd, p3, sizeof(struct filter_message) + 10)) != 5)
	{
		perror("writing");
		return -1;
	}
	printf("result: %d\n", result);
	
	close(fd);
	return 0;
}
	

