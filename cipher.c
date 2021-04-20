#include <linux/module.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#define MY_MAJOR  200
#define MY_MINOR  0
#define MY_DEV_COUNT 2

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ismael Elsharkawi");
MODULE_DESCRIPTION("Module for Character Devices");

static int     my_open( struct inode *, struct file * );
static ssize_t my_read( struct file * ,        char *  , size_t, loff_t *);
static ssize_t my_write(struct file * , const  char *  , size_t, loff_t *);
static int     my_close(struct inode *, struct file * );
struct file_operations my_fops = {
        read    :       my_read,
        write   :       my_write,
        open    :       my_open,
        release :       my_close,
        owner   :       THIS_MODULE
};
struct cdev my_cdev;
static char cipher_msg[4096];
static char unciphered_msg[4096];
static char cipher_key[4096];

static char cipher_msg_proc[4096];
static char cipher_key_proc[4096];



void rc4(unsigned char * p, unsigned char * k, unsigned char * c,int l)
{
        unsigned char s [256];
        unsigned char t [256];
        unsigned char temp;
        unsigned char kk;
        int i,j,x;
        for ( i  = 0 ; i  < 256 ; i ++ )
        {
                s[i] = i;
                t[i]= k[i % 4];
        }
        j = 0 ;
        for ( i  = 0 ; i  < 256 ; i ++ )
        {
                j = (j+s[i]+t[i])%256;
                temp = s[i];
                s[i] = s[j];
                s[j] = temp;
        }

        i = j = -1;
        for ( x = 0 ; x < l ; x++ )
        {
                i = (i+1) % 256;
                j = (j+s[i]) % 256;
                temp = s[i];
                s[i] = s[j];
                s[j] = temp;
                kk = (s[i]+s[j]) % 256;
                c[x] = p[x] ^ s[kk];
        }
}



static int proc_show(struct seq_file *m, void *v) {
    memset(cipher_msg_proc, 0, 4096);
    rc4(cipher_msg, cipher_key_proc, cipher_msg_proc,  4096);

    seq_printf(m, "%s\n", cipher_msg_proc);
    return 0;
}

static int proc_open(struct inode *inode, struct  file *file) {
    return single_open(file, proc_show, NULL);
}

static ssize_t proc_write(struct file *file, const char __user *ubuf,size_t count, loff_t *ppos){
        return count;
}
static const struct file_operations proc_file_ops = {
  .owner = THIS_MODULE,
  .open = proc_open,
  .read = seq_read,
  .write = proc_write,
  .llseek = seq_lseek,
  .release = single_release,
};

static int proc_key_dummy(struct seq_file *m, void *v){
    return 0;
}

static int proc_key_open(struct inode *inode, struct  file *file)
{
    return single_open(file, proc_key_dummy, NULL);
}

static ssize_t proc_key_write(struct file *file, const char __user *ubuf,size_t count, loff_t *ppos){
    strcpy(cipher_key_proc, ubuf);
    return count;
}
static const struct file_operations proc_key_file_ops = {
  .owner = THIS_MODULE,
  .open = proc_key_open,
  .read = seq_read,
  .write = proc_key_write,
  .llseek = seq_lseek,
  .release = single_release,
};




int init_module(void)
{
	dev_t devno;
	unsigned int count = MY_DEV_COUNT;
	int err;
	devno = MKDEV(MY_MAJOR, MY_MINOR);
    
	register_chrdev_region(devno, 1 , "cipher");
    register_chrdev_region(devno, 1 , "cipher_key");
	cdev_init(&my_cdev, &my_fops);
	my_cdev.owner = THIS_MODULE;
	err = cdev_add(&my_cdev, devno, count);

	if (err < 0)
	{
		printk("Device Add Error\n");
		return -1;
	}

	
    proc_create("cipher", 0, NULL, &proc_file_ops);
    proc_create("cipher_key", 0, NULL, &proc_key_file_ops);
    memset(cipher_key,0, 4096);
    memset(cipher_msg,0, 4096);
    memset(unciphered_msg, 0, 4096);
    memset(cipher_msg_proc,0, 4096);
    memset(cipher_key_proc,0, 4096);
    return 0;
}


void cleanup_module(void)
{
	dev_t devno;
	devno = MKDEV(MY_MAJOR, MY_MINOR);
	unregister_chrdev_region(devno, MY_DEV_COUNT);
	cdev_del(&my_cdev);
    remove_proc_entry("cipher", NULL);
    remove_proc_entry("cipher_key", NULL);

}



static int my_open(struct inode *inod, struct file *fil)
{
	int major;
	int minor;
	major = imajor(inod);
	minor = iminor(inod);
	return 0;
}


static ssize_t my_read(struct file *filp, char *buff, size_t len, loff_t *off)
{
	int major, minor;
	
    memset(buff, 0, len);
	major = MAJOR(filp->f_path.dentry->d_inode->i_rdev);
	minor = MINOR(filp->f_path.dentry->d_inode->i_rdev);
	switch(minor){
		case 0:
            strcpy(buff, cipher_msg);
            printk("%s\n", buff);
			break;
		case 1:
			printk("Why open a file when you know you don't have access????\n");
			break;
		default:
			len = 0;
	}
	return 0;
}


static ssize_t my_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
	int major,minor;
	short count;
	major = MAJOR(filp->f_path.dentry->d_inode->i_rdev);
	minor = MINOR(filp->f_path.dentry->d_inode->i_rdev);
    switch (minor)
    {
    case 0:
        memset(cipher_msg, 0, 4096);
        memset(unciphered_msg, 0, 4096);
        count = copy_from_user(unciphered_msg, buff, len);
        rc4(unciphered_msg, cipher_key, cipher_msg, 4096);
        break;
    case 1:
        memset(cipher_key, 0, 4096);
        count = copy_from_user( cipher_key, buff, len );
        break;
    default:
        break;
    }

	return len;
}



static int my_close(struct inode *inod, struct file *fil)
{
	int minor;
	minor = MINOR(fil->f_path.dentry->d_inode->i_rdev);
	return 0;
}