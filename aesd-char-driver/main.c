/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include "aesdchar.h"
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/types.h>
int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

MODULE_AUTHOR("Karim Yasser");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

/**
 * aesd_open - Called when a process opens the device file.
 * @inode: inode structure representing the device file on disk,
 *         contains i_cdev which links to our registered cdev.
 * @filp:  file structure representing this particular open instance.
 *
 * Uses container_of to recover the aesd_dev struct from the cdev member
 * embedded in inode->i_cdev, then stores it in filp->private_data so
 * subsequent read/write/release calls can access it.
 *
 * Return: 0 on success.
 */
int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /* Recover our device struct from the cdev embedded within inode */
    filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);
    return 0;
}

/**
 * aesd_release - Called when the last reference to an open file is closed.
 * @inode: inode associated with the device file.
 * @filp:  file structure for this open instance.
 *
 * No per-open resources are allocated in aesd_open, so nothing
 * needs to be freed here.
 *
 * Return: 0 on success.
 */
int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    return 0;
}

/**
 * aesd_read - Read data from the circular buffer at the given file position.
 * @filp:  file structure, private_data points to our aesd_dev.
 * @buf:   user-space buffer to copy data into.
 * @count: maximum number of bytes the user wants to read.
 * @f_pos: pointer to the current file offset; updated on success.
 *
 * Looks up the circular buffer entry containing byte *f_pos, then copies
 * the remaining bytes in that entry (up to @count) to user-space.
 * Only one entry's worth of data is returned per call; the caller
 * (VFS read loop) will call again if more data is needed.
 *
 * Return: number of bytes read on success, 0 at EOF, or negative errno.
 */
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t bytes_to_copy = 0;
    struct aesd_dev *aesd_dev_ptr = filp->private_data;
    struct aesd_buffer_entry *entry_ptr = NULL;
    size_t entry_pos = 0;

    PDEBUG("read %zu bytes with offset %lld", count, *f_pos);

    /* Acquire the mutex; return -ERESTARTSYS if interrupted by a signal */
    if(mutex_lock_interruptible(&aesd_dev_ptr->mutex_lock))
        return -ERESTARTSYS;

    /* Find the entry and byte offset within it for the current file position */
    entry_ptr = aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_dev_ptr->cir_buff, *f_pos,
                                                                &entry_pos);

    if(!entry_ptr)
    {
        /* No data at this offset — EOF */
        mutex_unlock(&aesd_dev_ptr->mutex_lock);
        return 0;
    }

    /* Copy at most 'count' bytes, but no more than what remains in this entry */
    bytes_to_copy = min(count, entry_ptr->size - entry_pos);

    if(copy_to_user(buf, &entry_ptr->buffptr[entry_pos], bytes_to_copy))
    {
        /* copy_to_user returns nonzero on failure (bytes NOT copied) */
        mutex_unlock(&aesd_dev_ptr->mutex_lock);
        return -EFAULT;
    }

    /* Advance the file position by the number of bytes successfully copied */
    *f_pos += bytes_to_copy;

    mutex_unlock(&aesd_dev_ptr->mutex_lock);
    return bytes_to_copy;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{

    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);
    struct aesd_dev *aesd_dev_ptr = filp->private_data;
    char *new_ptr = NULL;
    char *replaced_entry = NULL;
    if(mutex_lock_interruptible(&aesd_dev_ptr->mutex_lock))
        return -ERESTARTSYS;
    new_ptr =
        krealloc(aesd_dev_ptr->temp_buff.buffptr, aesd_dev_ptr->temp_buff.size + count, GFP_KERNEL);
    if(NULL == new_ptr)
    {
        mutex_unlock(&aesd_dev_ptr->mutex_lock);
        return -ENOMEM;
    }

    aesd_dev_ptr->temp_buff.buffptr = new_ptr;

    if(0 !=
       copy_from_user(&aesd_dev_ptr->temp_buff.buffptr[aesd_dev_ptr->temp_buff.size], buf, count))
    {
        mutex_unlock(&aesd_dev_ptr->mutex_lock);
        return -EFAULT;
    }

    aesd_dev_ptr->temp_buff.size += count;

    if(NULL !=
       memchr(&aesd_dev_ptr->temp_buff.buffptr[aesd_dev_ptr->temp_buff.size - count], '\n', count))
    {
        replaced_entry =
            aesd_circular_buffer_add_entry(&aesd_dev_ptr->cir_buff, &aesd_dev_ptr->temp_buff);
        if(replaced_entry != NULL)
        {
            kfree(replaced_entry);
        }
        aesd_dev_ptr->temp_buff.buffptr = NULL;
        aesd_dev_ptr->temp_buff.size = 0;
    }
    mutex_unlock(&aesd_dev_ptr->mutex_lock);
    return count;
}
struct file_operations aesd_fops = {
    .owner = THIS_MODULE,
    .read = aesd_read,
    .write = aesd_write,
    .open = aesd_open,
    .release = aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if(err)
    {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
    aesd_major = MAJOR(dev);
    if(result < 0)
    {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device, 0, sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    mutex_init(&aesd_device.mutex_lock);
    aesd_circular_buffer_init(&aesd_device.cir_buff);

    result = aesd_setup_cdev(&aesd_device);

    if(result)
    {
        unregister_chrdev_region(dev, 1);
    }
    return result;
}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    if(aesd_device.temp_buff.buffptr)
    {
        kfree(aesd_device.temp_buff.buffptr);
    }

    uint8_t index;
    struct aesd_buffer_entry *entry;
    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.cir_buff, index)
    {
        if(entry->buffptr)
        {
            kfree(entry->buffptr);
        }
    }

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
