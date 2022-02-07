typedef struct filter_message {
	unsigned char tag;
	char body[0];
} filter_message_t;

#define FILTER_MAJOR 124
#define FILTER_MAXTAG 256
#define FILTER_IOC_MAGIC 221
#define FILTER_IOCMAXNR 4

#define FILTER_IOCCLRFILTER	_IO(FILTER_IOC_MAGIC, 1)
#define FILTER_IOCTADDTAG	_IO(FILTER_IOC_MAGIC, 2, char)
#define FILTER_IOCTRMTAG	_IO(FILTER_IOC_MAGIC, 3, char)
#define FILTER_IOCGTAGS	_IOR(FILTER_IOC_MAGIC, 4, setchar_t *)
