#include <linux/module.h>

#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/tty.h>
#include <linux/kmod.h>
#include <linux/gfp.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/poll.h>

#include <linux/i2c.h>
#include <asm/uaccess.h>

#define DEVICE_NUMBER 2
#define DEVICE_NAME   "pca9505"

//Input port register
#define IP0   0x00
#define IP1   0x01
#define IP2   0x02
#define IP3   0x03
#define IP4   0x04

//Output port register
#define OP0   0x08
#define OP1   0x09
#define OP2   0x0A
#define OP3   0x0B
#define OP4   0x0C


//i/o configuration register
#define IOC0  0x18
#define IOC1  0x19
#define IOC2  0x1A
#define IOC3  0x1B
#define IOC4  0x14


//mask interrupt register
#define MSK0 0X20
#define MSK1 0x21
#define MSK2 0x22
#define MSK3 0x23
#define MSK4 0x24

#define WRITE_ALL_POL 0X90
#define WRITE_ALL_CFG 0X98
#define WRITE_ALL_MSK 0XA0

#define READ_ALL_INP 0X80


struct pca9505_dev{
	struct i2c_client *pca9505_client;
    void *private_data;
    int major;
	dev_t devid;
	struct cdev cdev;
	char *name;
	int irq;
};

static struct class *pca9505_class;

static dev_t parent_devid;	

static struct pca9505_dev pca9505dev[DEVICE_NUMBER];

//读取pca9505数据
static int pca9505_read_regs(struct pca9505_dev *dev, u8 reg, void *val, int len)
{
	int ret;
	struct i2c_msg msg[2];
	struct i2c_client *client = (struct i2c_client *)dev->private_data;

	/* msg[0]为发送要读取的首地址 */
	msg[0].addr = client->addr;			
	msg[0].flags = 0;					
	msg[0].buf = &reg;					
	msg[0].len = 1;						

	/* msg[1]读取数据 */
	msg[1].addr = client->addr;			
	msg[1].flags = I2C_M_RD;		
	msg[1].buf = val;					
	msg[1].len = len;				

	ret = i2c_transfer(client->adapter, msg, 2);
	if(ret == 2) {
		ret = 0;
	} else {
		printk("i2c rd failed=%d reg=%06x len=%d\n",ret, reg, len);
		ret = -EREMOTEIO;
	}
	return ret;
}

//写入pca9505数据
static s32 pca9505_write_regs(struct pca9505_dev *dev, u8 reg, u8 *buf, u8 len)
{
	u8 b[256];
	struct i2c_msg msg;
	struct i2c_client *client = (struct i2c_client *)dev->private_data;
	
	b[0] = reg;					
	memcpy(&b[1],buf,len);		
		
	msg.addr = client->addr;	
	msg.flags = 0;				

	msg.buf = b;				
	msg.len = len + 1;			

	return i2c_transfer(client->adapter, &msg, 1);
}


//配置pca9505的输出模式
static int pca9505_cfg_init(uint8_t id)
{   
    int ret = -1;

    uint8_t cmd_input[5] = {0xff,0xff,0xff,0xff,0xff};

    ret = pca9505_write_regs(&pca9505dev[id],WRITE_ALL_CFG,cmd_input,5);
	printk("init pca9505-%d\r\n",id);
    return ret;
}

//pca9505的打开函数
static int pca9505_open(struct inode * node, struct file * filp)
{
    int err;
	filp->private_data = &pca9505dev[MINOR(node->i_rdev)];

    err = pca9505_cfg_init(MINOR(node->i_rdev));

    printk("open pca9505 minor is %d\r\n",MINOR(node->i_rdev));
    return 0;
}

ssize_t pca9505_read(struct file *file, char __user *buf, size_t size, loff_t * offset)
{
    char read_buf[5];
    int err;

	struct pca9505_dev *dev= (struct pca9505_dev *)file->private_data;

    pca9505_read_regs(dev,READ_ALL_INP,read_buf,5);
    err = copy_to_user(buf,read_buf,5);
	return 5;
}

static struct file_operations pca9505_fops = {
    .open  = pca9505_open,
    .read  = pca9505_read,
    .owner = THIS_MODULE,
};


static int pca9505_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
    int ret = -1;
    printk("pca9505 start probe !");
	printk("pca9505 device_addr is 0x%x\r\n",client->addr);
	uint16_t pca9505_id = client->addr-0x20;
    pca9505dev[pca9505_id].private_data = client;

	
	pca9505dev[pca9505_id].devid = parent_devid + pca9505_id;	
	pca9505dev[pca9505_id].cdev.owner = THIS_MODULE;
	cdev_init(&pca9505dev[pca9505_id].cdev, &pca9505_fops);

	ret = cdev_add(&pca9505dev[pca9505_id].cdev, pca9505dev[pca9505_id].devid, 1);

    device_create(pca9505_class,NULL,pca9505dev[pca9505_id].devid,NULL,"pca9505-%d",pca9505_id);

    return 0;

}
static int pca9505_remove(struct i2c_client *client)
{
	uint16_t pca9505_id = client->addr-0x20;
	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

	device_destroy(pca9505_class, pca9505dev[pca9505_id].devid);
	unregister_chrdev(pca9505dev[pca9505_id].major, "pca9505");

	cdev_del(&pca9505dev[pca9505_id].cdev);

	return 0;
}

static const struct of_device_id pca9505_of_match[] = {
    {.compatible = "feng,pca9505"},
    {},
};

static const struct i2c_device_id pca9505_ids[] = {
	{ "xxxxyyy",	(kernel_ulong_t)NULL },
	{ /* END OF LIST */ }
};

static struct i2c_driver pca9505_drv = {
    .driver = {
        .name = "pca9505",
        .of_match_table = pca9505_of_match,
    },
    .probe = pca9505_probe,
    .remove = pca9505_remove,
    .id_table = pca9505_ids,
};

static int pca9505_init(void)
{
    int ret;
	pca9505_class = class_create(THIS_MODULE,"pca9505_class");
	if (IS_ERR(pca9505_class)) {
		printk("class_create err\r\n");
		return -1;
	}
	ret = alloc_chrdev_region(&parent_devid, 0, DEVICE_NUMBER, DEVICE_NAME);

    ret = i2c_add_driver(&pca9505_drv);
    printk("%s %s line %d\r\n",__FILE__,__FUNCTION__,__LINE__);
    return ret;
}

static void pca9505_exit(void)
{
    i2c_del_driver(&pca9505_drv);
	unregister_chrdev_region(parent_devid, DEVICE_NUMBER);
	class_destroy(pca9505_class);
}

module_init(pca9505_init);
module_exit(pca9505_exit);
MODULE_LICENSE("GPL");