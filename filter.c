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
#include <linux/ioctl.h>
#include <linux/mutex.h>

#include <linux/uaccess.h>	/* copy_*_user */
#include "filter.h"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Mustafa Rasit Ozdemir");

int filter_major = FILTER_MAJOR;
int filter_minor = 4;
int maxfilters = FILTER_MAXTAG;
int maxqsize = 10;
int maxmsgsize = 1024;

module_param(filter_major, int, S_IRUGO);
module_param(filter_minor, int, S_IRUGO);
module_param(maxfilters, int, S_IRUGO);
module_param(maxqsize, int, S_IRUGO);
module_param(maxmsgsize, int, S_IRUGO);

typedef struct message {
	unsigned char tag;
	char* body;
} message_t;

typedef struct reader {
	pid_t pid;
	int dev_index;
	unsigned char* tags;
	int numOfTags;
} reader_t;

struct filter_dev {
	wait_queue_head_t inq;
	message_t* queue;
	int* r_counters;
	int index;
	int numOfMsgs;
	reader_t* readers;
	int numOfReaders;
	struct mutex mutex;       /* Mutual exclusion semaphore. */
	struct cdev cdev;	  /* Char device structure. */
};

struct filter_dev *filter_devices;

/*
 * Open and close
 */

int filter_open(struct inode *inode, struct file *filp)
{
	struct filter_dev *dev; /* device information */
	unsigned long flags;

	dev = container_of(inode->i_cdev, struct filter_dev, cdev);

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	filp->private_data = dev; 					/* for other methods */

	if ( (filp->f_flags & O_ACCMODE) == O_RDONLY)
	{
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
		dev->readers[dev->numOfReaders].dev_index = dev->numOfMsgs;
		dev->readers[dev->numOfReaders].tags = kmalloc(maxfilters, GFP_KERNEL);
		dev->readers[dev->numOfReaders].numOfTags = 0;
		dev->numOfReaders++;
		
//		printk(KERN_ALERT "Some reader opened! PID:%d, MAJ:%d, MIN:%d, maxqsize:%d, maxmsgsize:%d, maxfilters:%d, numOfReaders:%d \n", 
//				dev->readers[dev->numOfReaders - 1].pid, filter_major, filter_minor, maxqsize, maxmsgsize, maxfilters, dev->numOfReaders);
	}
	
	
	mutex_unlock(&dev->mutex);
		
	return 0;
}

int filter_release(struct inode *inode, struct file *filp)
{
	int index = -1, i, cursor;
	
	struct filter_dev *dev; /* device information */
	dev = container_of(inode->i_cdev, struct filter_dev, cdev);

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;
	
	if ( (filp->f_flags & O_ACCMODE) == O_RDONLY)
	{
		if(dev->numOfReaders == 1)
		{
			cursor = dev->readers[0].dev_index;
			
			for(i = cursor; i < dev->numOfMsgs; i++)
			{
				if(dev->r_counters[i]>0)
					dev->r_counters[i]--;
			}
			
			kfree(dev->readers);
			dev->readers = '\0';
			dev->numOfReaders--;
		}
		else
		{
			for (i = 0; i < dev->numOfReaders; i++)
			{
				if(dev->readers[i].pid == current->pid)
				{
					index = i;
					break;
				}
			}
			
			if(index != -1)
			{
				cursor = dev->readers[index].dev_index;
			
				for(i = cursor; i < dev->numOfMsgs; i++)
				{
					if(dev->r_counters[i]>0)
						dev->r_counters[i]--;
				}
			
				reader_t* readers = kmalloc((dev->numOfReaders - 1) * sizeof(reader_t), GFP_KERNEL);
				
				memcpy(readers, &(dev->readers[0]), (index) * sizeof(reader_t));
				if(index < dev->numOfReaders - 1)
					memcpy(&(readers[index]), &(dev->readers[index+1]), (dev->numOfReaders - index - 1) * sizeof(reader_t));
				
				kfree(dev->readers);
				dev->readers = readers;
			}
			else
			{
				printk(KERN_ALERT "ERROR when closing: reader missing.\n");
				mutex_unlock(&dev->mutex);
				return 0;
			}
			
			dev->numOfReaders--;
		}
	}
	
	
//	printk(KERN_ALERT "NUM OF READERS %d.\n", dev->numOfReaders);
	mutex_unlock(&dev->mutex);
	
	return 0;
}

void eliminate_messages(struct filter_dev *dev)
{
	int numOfZeros = 0, i, j;

	if(dev->numOfMsgs == 0)
		return;
	
	for(i = 0; i < dev->numOfMsgs; i++)
	{
		if(dev->r_counters[i] != 0)
			break;
		else
			numOfZeros++;
	}
	
	message_t* msgs = kmalloc(maxqsize * sizeof(message_t), GFP_KERNEL);
	memset(msgs, 0, maxqsize);
	for(j = 0; j < maxqsize; j++)
		msgs[j].body = kmalloc(maxmsgsize, GFP_KERNEL);
	
	for(i = numOfZeros; i < dev->numOfMsgs; i++)
	{
		memcpy(msgs[i - numOfZeros].body, dev->queue[i].body, maxmsgsize);
		msgs[i - numOfZeros] = dev->queue[i];
	}
	
	kfree(dev->queue);
	dev->queue = msgs;
	
	dev->numOfMsgs = dev->numOfMsgs - numOfZeros;
}

void free_queue_capacity(struct filter_dev *dev)
{
	int i, j;

	if(dev->numOfMsgs != maxqsize)
		return;
	
	message_t* msgs = kmalloc(maxqsize * sizeof(message_t), GFP_KERNEL);
	memset(msgs, 0, maxqsize);
	for(j = 0; j < maxqsize; j++)
		msgs[j].body = kmalloc(maxmsgsize, GFP_KERNEL);
	
	for(i = 1; i < dev->numOfMsgs; i++)
	{
		memcpy(msgs[i - 1].body, dev->queue[i].body, maxmsgsize);
		msgs[i - 1] = dev->queue[i];
	}
	
	kfree(dev->queue);
	dev->queue = msgs;
	
	dev->numOfMsgs--;
}

ssize_t filter_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	struct filter_dev *dev = filp->private_data;
	ssize_t retval = -ENOMEM; /* Value used in "goto out" statements. */
	int tbf;

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;
	
	eliminate_messages(dev);	/* eliminate messages with 0 counters */
	free_queue_capacity(dev);	/* if queue is full, pop earliest */

	filter_message_t* received = kmalloc(sizeof(filter_message_t) + maxmsgsize, GFP_KERNEL);
	memset(received, 0, sizeof(filter_message_t) + maxmsgsize);
	
	if(maxmsgsize > count - 1)
	{
		tbf = count;
	}
	else
	{
		tbf = maxmsgsize + 1;
	}
	
	if (copy_from_user(received, buf, tbf)) /* truncate if it is bigger than maxmsgsize */
	{
		retval = -EFAULT;
		goto out;
	}
	
	retval = tbf - 1;
	
	dev->queue[dev->numOfMsgs].tag = received->tag;
	memcpy(dev->queue[dev->numOfMsgs].body, received->body, tbf-1);
	dev->r_counters[dev->numOfMsgs] = dev->numOfReaders;
	dev->numOfMsgs++;
	
  out:
	wake_up_interruptible(&dev->inq);
	mutex_unlock(&dev->mutex);
	return retval;
}

/*
 * The ioctl() implementation.
 */
long filter_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0, r_index = -1, i, j, retval = 0, min_index = -1;
	unsigned char tag, min = 255;
	unsigned char* tags;
	int* ctrl;
	struct filter_dev* dev; /* device information */
	reader_t* reader;
	 
	dev = filp->private_data;
	
	if (_IOC_TYPE(cmd) != FILTER_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > FILTER_IOCMAXNR)
		return -ENOTTY;

	/*
	 * The direction is a bitmask, and VERIFY_WRITE catches R/W
	 * transfers. `Type' is user-oriented, while access_ok is
	 * kernel-oriented, so the concept of "read" and "write" is reversed.
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;
	
	
	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;

	for (i = 0; i < dev->numOfReaders; i++)
	{
		if(dev->readers[i].pid == current->pid)
		{
			r_index = i;
			break;
		}
	}

	if(r_index == -1)
	{
		mutex_unlock(&dev->mutex);
		return -EFAULT;
	}

	switch(cmd)
	{
	  case FILTER_IOCCLRFILTER:
	  	for(i = 0; i < maxfilters; i++)
	  	{
	  		dev->readers[r_index].tags[i] = 0;
	  	}
	  	
	  	dev->readers[r_index].numOfTags = 0;
	  
		break;
        
	  case FILTER_IOCTADDTAG:  
	  	tag = (char __user *)arg;
	  	
	  	if(dev->readers[r_index].numOfTags == maxfilters)
	  	{
			retval = -ENOTTY;
	  		break;
	  	}
	  	
	  	dev->readers[r_index].tags[dev->readers[r_index].numOfTags] = tag;
	  	dev->readers[r_index].numOfTags++;
	  	
		break;

	  case FILTER_IOCTRMTAG:
	  	j = -1;
	  	
	  	for(i = 0; i < dev->readers[r_index].numOfTags; i++)
	  	{
	  		if(dev->readers[r_index].tags[i] == (char __user *)arg)
	  		{
	  			j = i;
	  		}
	  	}
	  	
	  	if(j == -1)
	  		break;
	  	
	  	for(i = 0; i < dev->readers[r_index].numOfTags; i++)
	  	{
	  		if (i > j)
	  			dev->readers[r_index].tags[i-1] = dev->readers[r_index].tags[i];
	  	}
	  	
	  	dev->readers[r_index].numOfTags--;
	  
		break;

	  case FILTER_IOCGTAGS:
	  	tags = kmalloc(maxfilters, GFP_KERNEL);
	  	ctrl = kmalloc(maxfilters * sizeof(int), GFP_KERNEL);
	  	memset(ctrl, 0,  maxfilters * sizeof(int));
	  	memset(tags, 0,  maxfilters);
	  	
//	  	printk(KERN_ALERT "unsorted_tags: %s\n", dev->readers[r_index].tags);
	  	
	  	for(i = 0; i < dev->readers[r_index].numOfTags; i++)
	  	{
	  		min = 255;
	  		min_index = -1;
	  		for(j = 0; j < dev->readers[r_index].numOfTags; j++)
	  		{
	  			if(dev->readers[r_index].tags[j] < min && ctrl[j] == 0)
	  			{
	  				min = dev->readers[r_index].tags[j];
	  				min_index = j;
	  			}
	  		}
	  		
	  		tags[i] = min;
	  		ctrl[min_index] = 1;
	  	}
	  	
//	  	printk(KERN_ALERT "sorted_tags: %s\n", tags);
	  	
		memcpy((char* __user *)arg, tags, dev->readers[r_index].numOfTags);
	  	
	  	retval = dev->readers[r_index].numOfTags;
		break;
	  default:  /* Redundant, as cmd was checked against MAXNR. */
	  	mutex_unlock(&dev->mutex);
		return -ENOTTY;
	}
	
	mutex_unlock(&dev->mutex);
	return retval;
}

int suitableMessageCheck(struct filter_dev *dev, reader_t reader)
{
	int i, j;
	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;
		
	if(reader.dev_index == dev->numOfMsgs)
	{
		mutex_unlock(&dev->mutex);
		return 0;
	}
	else if(reader.dev_index < dev->numOfMsgs && reader.numOfTags == 0)
	{
		mutex_unlock(&dev->mutex);
		return 1;
	}
	
	for(i = reader.dev_index; i < dev->numOfMsgs; i++)
	{
		unsigned char tag = dev->queue[i].tag;
		
		for(j = 0; j < reader.numOfTags; j++)
		{
			if(reader.tags[j] == tag)
			{
				mutex_unlock(&dev->mutex);
				return 1;
			}
		}
	}
	
	mutex_unlock(&dev->mutex);
	return 0;
}

int getSuitableMessage(struct filter_dev *dev, reader_t reader)
{
	int i, j;
	
	for(i = reader.dev_index; i < dev->numOfMsgs; i++)
	{
		unsigned char tag = dev->queue[i].tag;
		if(reader.numOfTags > 0)
		{
			for(j = 0; j < reader.numOfTags; j++)
			{
				if(reader.tags[j] == tag)
				{
					return i;
				}
			}
		}
		else
		{
			return i;
		}
	}
	
	return -1;
}

ssize_t filter_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	struct filter_dev *dev = filp->private_data;
	int r_index = -1, i, m_index, tbr;
	ssize_t retval = 0;

	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;
	
	for (i = 0; i < dev->numOfReaders; i++)
	{
		if(dev->readers[i].pid == current->pid)
		{
			r_index = i;
			break;
		}
	}

	if(r_index == -1)
	{
		mutex_unlock(&dev->mutex);
		return -EFAULT;
	}
	
	mutex_unlock(&dev->mutex);
	
	if (filp->f_flags & O_NONBLOCK)
	{
		return -EAGAIN;
	}
	
	if (wait_event_interruptible(dev->inq, suitableMessageCheck(dev, dev->readers[r_index]) == 1))
		return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
	
	if (mutex_lock_interruptible(&dev->mutex))
		return -ERESTARTSYS;
	dev = filp->private_data;
		
	m_index = getSuitableMessage(dev, dev->readers[r_index]);
	
	if(count > maxmsgsize + 1)
		tbr = maxmsgsize + 1;
	else
		tbr = count;

	if (copy_to_user(buf, &(dev->queue[m_index]), sizeof(unsigned char)))
	{
		retval = -EFAULT;
		goto out;
	}
	
	if (copy_to_user(((char*)buf) + 1, dev->queue[m_index].body, tbr - 1))
	{
		retval = -EFAULT;
		goto out;
	}
	
	dev->readers[r_index].dev_index = m_index + 1;
	dev->r_counters[m_index]--;
	
	retval = tbr;

  out:
	mutex_unlock(&dev->mutex);
	return retval;
}

struct file_operations filter_fops = {
	.owner =    THIS_MODULE,
	.read =     filter_read,
	.write =    filter_write,
	.unlocked_ioctl = filter_ioctl,
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
	if (filter_devices)
	{
		for (i = 0; i < filter_minor; i++)
		{
			filter_trim(filter_devices + i);
			cdev_del(&filter_devices[i].cdev);
		}
		kfree(filter_devices);
	}

	/* cleanup_module is never called if registering failed. */
	unregister_chrdev_region(devno, filter_minor);
//	printk(KERN_ALERT "Goodbye, filter world\n");
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
	
	if (!filter_devices)
	{
		result = -ENOMEM;
		goto fail;
	}
	memset(filter_devices, 0, filter_minor * sizeof(struct filter_dev));
	
	for(i = 0; i < filter_minor; i++)
	{
		filter_devices[i].queue = kmalloc(maxqsize * sizeof(message_t), GFP_KERNEL);
		for(j = 0; j < maxqsize; j++)
			filter_devices[i].queue[j].body = kmalloc(maxmsgsize, GFP_KERNEL);
		
		filter_devices[i].r_counters = kmalloc(maxqsize * sizeof(int), GFP_KERNEL);
		memset(filter_devices[i].r_counters, 0, maxqsize * sizeof(int));
		filter_devices[i].index = 0;
		filter_devices[i].numOfMsgs = 0;
		filter_devices[i].readers = '\0';
		filter_devices[i].numOfReaders = 0;
		init_waitqueue_head(&(filter_devices[i].inq));
		mutex_init(&filter_devices[i].mutex);
		filter_setup_cdev(&filter_devices[i], i);
	}

//	printk(KERN_ALERT "Filter, world\n");
	return 0;
	
fail:
	//filter_cleanup_module(); //TBD
	return result;
}

module_init(filter_init);
module_exit(filter_cleanup_module);
