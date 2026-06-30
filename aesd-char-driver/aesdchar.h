/*
 * aesdchar.h
 *
 *  Created on: Oct 23, 2019
 *      Author: Dan Walkes
 */

#ifndef AESD_CHAR_DRIVER_AESDCHAR_H_
#define AESD_CHAR_DRIVER_AESDCHAR_H_

#include "aesd-circular-buffer.h"
#include <linux/cdev.h>
#include <linux/mutex.h>
#define AESD_DEBUG 1 // Remove comment on this line to enable debug

#undef PDEBUG /* undef it, just in case */
#ifdef AESD_DEBUG
#ifdef __KERNEL__
/* This one if debugging is on, and kernel space */
#define PDEBUG(fmt, args...) printk(KERN_DEBUG "aesdchar: " fmt, ##args)
#else
/* This one for user space */
#define PDEBUG(fmt, args...) fprintf(stderr, fmt, ##args)
#endif
#else
#define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

struct aesd_dev {
  /**
   * TODO: Add structure(s) and locks needed to complete assignment requirements
   */
  struct mutex mutex_lock;
  struct aesd_circular_buffer cir_buff;
  size_t cir_buff_size;
  char *working_buff;       /* Non-const buffer for accumulating partial writes */
  size_t working_buff_size; /* Current size of working_buff */
  struct cdev cdev;         /* Char device structure      */
};

#endif /* AESD_CHAR_DRIVER_AESDCHAR_H_ */
