#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/random.h>

#define SEL_CHNL _IOW('a','a',int32_t*)
#define SEL_ALIN _IOW('a','b',int32_t*)

u_int32_t adc_data;
u_int32_t i;
u_int32_t chnl;
u_int32_t align;
//int32_t arg;
 
static dev_t first; // variable for device number
static struct cdev c_dev; // variable for the character device structure
static struct class *cls; // variable for the device class

/*****************************************************************************
STEP 4 as discussed in the lecture, 
my_close(), my_open(), my_read(), my_write() functions are defined here
these functions will be called for close, open, read and write system calls respectively. 
*****************************************************************************/

static int my_open(struct inode *i, struct file *f)
{
	printk(KERN_INFO "Device file opened...!!!\n");
	return 0;
}

static int my_close(struct inode *i, struct file *f)
{
	printk(KERN_INFO "Device file closed...!!!\n");
	return 0;
}

static ssize_t my_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{

        get_random_bytes(&i,sizeof(int));
        adc_data=i & 0x3ff;
	if(align==2)
	{
	adc_data=adc_data<<6;
	}
        copy_to_user(buf, &adc_data,2);
	printk(KERN_INFO "Mychar : read()\n");
	return 0;
}




static ssize_t my_write(struct file *f, const char __user *buf, size_t len, loff_t *off)
{
	printk(KERN_INFO "Mychar : write()\n");
	return len;
}

static long adc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch(cmd) {
		case SEL_CHNL:
			copy_from_user(&chnl,(int32_t*) arg, sizeof(chnl));
			printk(KERN_INFO "Selected channel is %d\n",chnl);
			break;
		case SEL_ALIN:
			copy_from_user(&align,(int32_t*) arg, sizeof(align));
			// printk(KERN_INFO "The alignment value is  = %d\n", align);
                        switch(align)
                        {
                        case 1:
                           printk(KERN_INFO "Selected alignment is right aligned");
			   break;
                        case 2:
                           printk(KERN_INFO "Selected alignment is left aligned");
                           break;
                        }
	}
	return 0;
}

//###########################################################################################


static struct file_operations fops =
{
  .owner 	= THIS_MODULE,
  .open 	= my_open,
  .release 	= my_close,
  .read 	= my_read,
  .write 	= my_write,
  .unlocked_ioctl = adc_ioctl,
};
 
//########## INITIALIZATION FUNCTION ##################
// STEP 1,2 & 3 are to be executed in this function ### 
static int __init mychar_init(void) 
{
	printk(KERN_INFO "Namaste: mychar driver registered");
	
	// STEP 1 : reserve <major, minor>
	if (alloc_chrdev_region(&first, 0, 1, "BITS-PILANI") < 0)
	{
		return -1;
	}
	
	// STEP 2 : dynamically create device node in /dev directory
    if ((cls = class_create(THIS_MODULE, "chardrv")) == NULL)
	{
		unregister_chrdev_region(first, 1);
		return -1;
	}
    if (device_create(cls, NULL, first, NULL, "adc8") == NULL)
	{
		class_destroy(cls);
		unregister_chrdev_region(first, 1);
		return -1;
	}
	
	// STEP 3 : Link fops and cdev to device node
    cdev_init(&c_dev, &fops);
    if (cdev_add(&c_dev, first, 1) == -1)
	{
		device_destroy(cls, first);
		class_destroy(cls);
		unregister_chrdev_region(first, 1);
		return -1;
	}
	return 0;
}
 
static void __exit mychar_exit(void) 
{
	cdev_del(&c_dev);
	device_destroy(cls, first);
	class_destroy(cls);
	unregister_chrdev_region(first, 1);
	printk(KERN_INFO "Bye: mychar driver unregistered\n\n");
}
 
module_init(mychar_init);
module_exit(mychar_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("EEE G547");
MODULE_DESCRIPTION("ADC");













 


