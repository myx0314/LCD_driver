#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/uaccess.h>

#define LCD_CNT	1
#define LCD_NAME	"myx_lcd"
#define LED_ON 1
#define LED_OFF 0

static void __iomem *IMX6U_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

struct lcd_dev {
	dev_t devid;						/* 设备号 	 */
	struct cdev cdev;					/* cdev 	*/
	struct class *class;				/* 类 		*/
	struct device *device;				/* 设备 	 */
	int major;							/* 主设备号	  */
	int minor;							/* 次设备号   */
	struct device_node *nd; 			/* 设备节点 */
};

struct lcd_dev lcd;

void led_switch(u8 sta) {
	u32 val = 0;

	if (sta == LED_ON) {
		printk("led on!");
		val = readl(GPIO1_DR);
		val &= ~(1 << 3); 
		writel(val, GPIO1_DR);
	} else if(sta == LED_OFF) {
		printk("led off!");
		val = readl(GPIO1_DR);
		val |= (1 << 3);
		writel(val, GPIO1_DR);
	} 
}

static int lcd_open(struct inode *inode, struct file *filp) {
	filp->private_data = &lcd; 	/* 设置私有数据 */
	return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int lcd_release(struct inode *inode, struct file *filp) {
	return 0;
}

static ssize_t lcd_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt) {
	return 0;
}

static ssize_t lcd_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt) {
	u8 cmd = 0;
	u8 ret = 0;
	
	printk("enter lcd_write!");

	ret = copy_from_user(&cmd, buf, sizeof(u8));
	if (ret < 0) {
		printk("copy_from_user failed.");
		return -EFAULT;
	}

	led_switch(cmd);

	return 0;
}


/* 设备操作函数 */
static struct file_operations lcd_fops = {
	.owner = THIS_MODULE,
	.open = lcd_open,
	.release = 	lcd_release,
	.read = lcd_read,
	.write = lcd_write,
};

static int __init lcd_driver_init(void) {
    int ret = 0;
	const char* str = NULL;
	struct property *proper = NULL;
	u32 val = 0;

	lcd.nd = of_find_node_by_path("/alphaled");
	if (lcd.nd == NULL) {
		printk("of_find_node_by_path failed!");
		return -EINVAL;
	}

	proper = of_find_property(lcd.nd, "compatible", NULL);
	if (proper == NULL) {
		printk("of_find_property failed!");
		return -EINVAL;
	}

	ret = of_property_read_string(lcd.nd, "status", &str);
	if (ret < 0) {
		printk("of_find_read_string failed!");
		return -EINVAL;
	}

	IMX6U_CCM_CCGR1 = of_iomap(lcd.nd, 0);
	SW_MUX_GPIO1_IO03 = of_iomap(lcd.nd, 1);
	SW_PAD_GPIO1_IO03 = of_iomap(lcd.nd, 2);
	GPIO1_DR = of_iomap(lcd.nd, 3);
	GPIO1_GDIR = of_iomap(lcd.nd, 4);

	/* 使能 GPIO1 时钟 */
	val = readl(IMX6U_CCM_CCGR1);
	val &= ~(3 << 26); /* 清楚以前的设置 */
	val |= (3 << 26); /* 设置新值 */
	writel(val, IMX6U_CCM_CCGR1);

 	/* 设置 GPIO1_IO03 的复用功能，将其复用为
	* GPIO1_IO03，最后设置 IO 属性。
	*/
	writel(5, SW_MUX_GPIO1_IO03);

	/* 寄存器 SW_PAD_GPIO1_IO03 设置 IO 属性 */
	writel(0x10B0, SW_PAD_GPIO1_IO03);

	/* 设置 GPIO1_IO03 为输出功能 */
	val = readl(GPIO1_GDIR);
	val &= ~(1 << 3); /* 清除以前的设置 */
	val |= (1 << 3); /* 设置为输出 */
	writel(val, GPIO1_GDIR);

	/* 5、默认关闭 LED */
	val = readl(GPIO1_DR);
	val |= (1 << 3); 
	writel(val, GPIO1_DR);

	/* 注册字符设备驱动 */
	/* 1、创建设备号 */
	if (lcd.major) {		/*  定义了设备号 */
		lcd.devid = MKDEV(lcd.major, 0);
		ret = register_chrdev_region(lcd.devid, LCD_CNT, LCD_NAME);
	} else {						/* 没有定义设备号 */
		ret = alloc_chrdev_region(&lcd.devid, 0, LCD_CNT, LCD_NAME);	/* 申请设备号 */
		lcd.major = MAJOR(lcd.devid);	/* 获取分配号的主设备号 */
		lcd.minor = MINOR(lcd.devid);	/* 获取分配号的次设备号 */
	}
	if(ret < 0) {
		goto fail_devid;
	}
	
	/* 2、初始化cdev */
	lcd.cdev.owner = THIS_MODULE;
	cdev_init(&lcd.cdev, &lcd_fops);
	
	/* 3、添加一个cdev */
	ret = cdev_add(&lcd.cdev, lcd.devid, LCD_CNT);
	if(ret < 0){
		goto fail_cdevadd;
	}

	/* 4、创建类 */
	lcd.class = class_create(THIS_MODULE, LCD_NAME);
	if (IS_ERR(lcd.class)) {
		ret = PTR_ERR(lcd.class);
		goto fail_class;
	}

	/* 5、创建设备 */
	lcd.device = device_create(lcd.class, NULL, lcd.devid, NULL, LCD_NAME);
	if (IS_ERR(lcd.device)) {
		ret = PTR_ERR(lcd.device);
		goto fail_device;
	}

	return 0;

fail_device:
	class_destroy(lcd.class);
fail_class:
	cdev_del(&lcd.cdev);
fail_cdevadd:
	unregister_chrdev_region(lcd.devid, LCD_CNT);
fail_devid:
	return ret;
}

static void __exit lcd_driver_exit(void) {
    /* 注销字符设备驱动 */
	cdev_del(&lcd.cdev);/*  删除cdev */
	unregister_chrdev_region(lcd.devid, LCD_CNT); /* 注销设备号 */

	device_destroy(lcd.class, lcd.devid);
	class_destroy(lcd.class);
}

module_init(lcd_driver_init);
module_exit(lcd_driver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("mayixiao");