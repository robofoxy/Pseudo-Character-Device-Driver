#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>	/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>

#include <linux/uaccess.h>	/* copy_*_user */

#define FILTER_MAJOR 124
#define FILTER_MAXTAG 256
#define FILTER_IOC_MAGIC 221
#define FILTER_IOCMAXNR 4

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Mustafa Rasit Ozdemir");

int filter_major = FILTER_MAJOR;
int filter_minor = 4;
int maxfilters = 20;
int maxqsize = 10;
int maxmsgsize = 1024;

module_param(filter_major, int, S_IRUGO);
module_param(filter_minor, int, S_IRUGO);
module_param(maxfilters, int, S_IRUGO);
module_param(maxqsize, int, S_IRUGO);
module_param(maxmsgsize, int, S_IRUGO);


typedef struct reader {
	pid_t pid;
	int dev_index;
	// add filters here.
} reader_t;

typedef struct filter_message {
	unsigned char tag;
	char* body;
} filter_message_t;

struct filter_dev {
	filter_message_t** queue;  /* Pointer to first quantum set. */
	int* r_counters;
	int index;
	int numOfMsgs;
	reader_t* readers;
	int numOfReaders;
	struct mutex mutex;       /* Mutual exclusion semaphore. */
	struct cdev cdev;	  /* Char device structure. */
	int deb;
};

struct filter_dev *filter_devices;

/*
 * Open and close
 */

int filter_open(struct inode *inode, struct file *filp)
{
	struct filter_dev *dev; /* device information */

	dev = container_of(inode->i_cdev, struct filter_dev, cdev);
	filp->private_data = dev; 					/* for other methods */
	
	if(dev->numOfReaders == 0)
	{
		dev->readers = kmalloc(sizeof(reader_t), GFP_KERNEL);
	}
	else
	{
		reader_t* readers = kmalloc((dev->numOfReaders + 1) * sizeof(reader_t), GFP_KERNEL);
		memcpy(readers, dev->readers, (dev->numOfReaders) * sizeof(reader_t));
		kfree(dev->readers);
		dev->readers = readers;
	}
	
	dev->readers[dev->numOfReaders].pid = current->pid;
	dev->numOfReaders++;
	
	//printk(KERN_ALERT "Somebody was opened! PID:%d dev_deb:%d, MAJ:%d, MIN:%d, maxqsize:%d, maxmsgsize:%d, maxfilters:%d\n", dev->readers[0].pid, dev->deb, filter_major, filter_minor, maxqsize, maxmsgsize, maxfilters);
	
	return 0;
}

int filter_release(struct inode *inode, struct file *filp)
{
	struct filter_dev *dev; /* device information */
	dev = container_of(inode->i_cdev, struct filter_dev, cdev);
	
	if(dev->numOfReaders == 1)
	{
		kfree(dev->readers);
		dev->numOfReaders--;
	}
	else
	{
		reader_t* readers = kmalloc((dev->numOfReaders + 1) * sizeof(reader_t), GFP_KERNEL);
		memcpy(readers, dev->readers, (dev->numOfReaders) * sizeof(reader_t));
		kfree(dev->readers);
		dev->readers = readers;
	}
	
	
	
	return 0;
}

struct file_operations filter_fops = {
	.owner =    THIS_MODULE,
	.read =     '\0',//scull_read,
	.write =    '\0',//scull_write,
	.unlocked_ioctl = '\0',//scull_ioctl,
	.open =     filter_open,
	.release =  filter_release,
};

static void filter_setup_cdev(struct filter_dev *dev, int index)
{
	int err, devno = MKDEV(filter_major, index);
    
	cdev_init(&dev->cdev, &filter_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &filter_fops;
	err = cdev_add (&dev->cdev, devno, 1);
	/* Fail gracefully if need be. */
	if (err)
		printk(KERN_NOTICE "Error %d adding filter%d", err, index);
}

int filter_trim(struct filter_dev *dev)
{
	int j;
	
	for(j = 0; j < maxqsize; j++)
	{
		kfree(dev->queue[j]->body);
		kfree(dev->queue[j]);
	}
	
	kfree(dev->queue);
	kfree(dev->r_counters);
	dev->index = 0;
	dev->numOfMsgs = 0;
	kfree(dev->readers);
	dev->numOfReaders = 0;
	
	return 0;
}

void filter_cleanup_module(void)
{
	int i;
	dev_t devno = MKDEV(filter_major, 0);

	/* Get rid of our char dev entries. */
	if (filter_devices) {
		for (i = 0; i < filter_minor; i++) {
			filter_trim(filter_devices + i);
			cdev_del(&filter_devices[i].cdev);
		}
		kfree(filter_devices);
	}

	/* cleanup_module is never called if registering failed. */
	unregister_chrdev_region(devno, filter_minor);
	printk(KERN_ALERT "Goodbye, filter world\n");
}

static int filter_init(void)
{
	int result, i, j;
	dev_t dev = 0;
	dev = MKDEV(filter_major, 0);
	result = register_chrdev_region(dev, filter_minor, "filter");
	
	if (result < 0)
	{
		printk(KERN_WARNING "filter: can't get major %d\n", filter_major);
		return result;
	}
	
	filter_devices = kmalloc(filter_minor * sizeof(struct filter_dev), GFP_KERNEL);
	
	if (!filter_devices) {
		result = -ENOMEM;
		goto fail;
	}
	memset(filter_devices, 0, filter_minor * sizeof(struct filter_dev));
	
	for(i = 0; i < filter_minor; i++)
	{
		filter_devices[i].queue = kmalloc(maxqsize * sizeof(filter_message_t*), GFP_KERNEL);
		
		for(j = 0; j < maxqsize; j++)
		{
			filter_devices[i].queue[j] = kmalloc(sizeof(filter_message_t), GFP_KERNEL);
			memset(filter_devices[i].queue[j], 0, sizeof(filter_message_t));
			filter_devices[i].queue[j]->body = kmalloc(maxmsgsize, GFP_KERNEL);
			memset(filter_devices[i].queue[j]->body, 0, maxmsgsize);
		}
		
		filter_devices[i].r_counters = kmalloc(maxqsize * sizeof(int), GFP_KERNEL);
		memset(filter_devices[i].r_counters, 0, maxqsize * sizeof(int));
		filter_devices[i].index = 0;
		filter_devices[i].numOfMsgs = 0;
		filter_devices[i].readers = '\0';
		filter_devices[i].numOfReaders = 0;
		filter_devices[i].deb = i;
		mutex_init(&filter_devices[i].mutex);
		filter_setup_cdev(&filter_devices[i], i);
	}

	printk(KERN_ALERT "Filter, world\n");
	return 0;
	
fail:
	//filter_cleanup_module(); //TBD
	return result;
}

module_init(filter_init);
module_exit(filter_cleanup_module);
