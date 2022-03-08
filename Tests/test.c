#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include "filter.h"


/* msleep(): Sleep for the requested number of milliseconds. */
int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}


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
	strncpy(p->body, buf, 11);
	p->tag = 'A';
	
	printf("sent: %c %s\n", p->tag, p->body);

	if ((result = write (fd, p, sizeof(struct filter_message) + 10)) != 10)
	{
		perror("writing");
		return -1;
	}
	printf("result: %d\n", result);
	msleep(500);
	struct filter_message *p2 = malloc(sizeof(struct filter_message) + 10);
	strncpy(p2->body, buf, 11);
	p2->tag = 'A';
	printf("sent: %c %s\n", p2->tag, p2->body);

	if ((result = write (fd, p2, sizeof(struct filter_message) + 10)) != 10)
	{
		perror("writing");
		return -1;
	}
	printf("result: %d\n", result);
	msleep(500);
	struct filter_message *p3 = malloc(sizeof(struct filter_message) + 10);
	strncpy(p3->body, buf, 11);
	p3->tag = 'A';
	printf("sent: %c %s\n", p3->tag, p3->body);

	if ((result = write (fd, p3, sizeof(struct filter_message) + 10)) != 10)
	{
		perror("writing");
		return -1;
	}
	printf("result: %d\n", result);

	close(fd);
	return 0;
}
	

