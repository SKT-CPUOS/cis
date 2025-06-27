/*
scanner: scanner@43000000 {
    clock-names = "clk";
    clocks = <&misc_clk_0>;
    compatible = "ucu-scanner";
    reg = <0x43000000 0x1000>;
    interrupt-parent = <&intc>;
    interrupts = <0 29 4>;
}
*/;
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <linux/inet.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/init.h>


#define DEVICE_NAME		"scanner"		/* 名字 */
#define DRIVER_NAME		"scanner-driver"
#define DEVICE_COUNT	1	
#define FINISH_INTERRUPT_MASK 0x00000008
#define READY_INTERRUPT_MASK 0x00000100
#define SCAN_MODE_BIT_MASK 0x00000003

#define CMD_ALLOC_MEM   (_IO('a', 1))        /* 预分配dma内存 */  
#define CMD_SET_DPI     (_IO('a', 2))        /* 设置dpi模式 */

struct trandata{
  u32 number;
  u32 data;
};

struct memory_block {
    void *data;                  // 数据指针
    dma_addr_t paddr;
    struct list_head list;       // 链表节点
};

struct scanner_dev {
    dev_t devid;			/* 设备号 */
    u64 dma_mask;
    size_t cache_size;
    int dpi;
    struct cdev cdev;		/* cdev结构体 */
    struct class *class;		/* 类 */
    struct device *device;	/* 设备 */
    void __iomem *membase;
    u32 mapbase;
    int irq;
};

struct scanner_regs {
    volatile uint32_t control;
    volatile uint32_t cache_addr;
    volatile uint32_t cache_size;
    volatile uint32_t line_num;
    volatile uint32_t interval_time;
    volatile uint32_t chose;
    volatile uint32_t black_offset;
};

static LIST_HEAD(memory_pool);         // 动态内存池
static LIST_HEAD(ready_queue);         
static int finish_flag = 0;
static struct scanner_dev scanner;	/* 设备结构体 */

// 添加内存块到内存池
static int add_block_to_pool(void)
{
  struct memory_block *block = kmalloc(sizeof(*block), GFP_KERNEL);
  //pr_info("kmalloc block : %x\n",(u32)(block));

  if (!block)
      return -1;

  block->data = dma_alloc_coherent(scanner.device, scanner.cache_size, &(block->paddr), GFP_KERNEL); 
  if (!block->data) {
      pr_info("allocate dma cache failed\n");
      kfree(block);
      return -1;
  }

  list_add_tail(&block->list, &memory_pool); // 添加到链表尾部

  return 0;
}

// 发生数据就绪中断时，将旧的cache转入就绪队列，从dma池中获取新的cache块
static struct memory_block *ready_itr_func(void)
{
    struct memory_block *block = NULL;

    if (!list_empty(&memory_pool)) {
        block = list_first_entry(&memory_pool, struct memory_block, list);
        list_del(&block->list);                // 从链表中移除
        list_add_tail(&block->list, &ready_queue); // 添加到就绪链表尾部
        /* 从dma池中取出新的内存块 */
        if (!list_empty(&memory_pool)) {
            block = list_first_entry(&memory_pool, struct memory_block, list);
        }
        else
            block = NULL;
    }
    return block;
}

// 发生结束中断时，将最后一块cache转入就绪队列
static struct memory_block *finish_itr_func(void)
{
    struct memory_block *block = NULL;

    if (!list_empty(&memory_pool)) {
       block = list_first_entry(&memory_pool, struct memory_block, list);
       list_del(&block->list);                // 从链表中移除
       list_add_tail(&block->list, &ready_queue); // 添加到就绪链表尾部
        /* 从dma池中取出新的内存块 */
        if (!list_empty(&memory_pool)) {
            block = list_first_entry(&memory_pool, struct memory_block, list);
        }
        else
            block = NULL;
    }
    return block;
}

// 释放内存块
static void free_block(struct memory_block *block)
{
    if (block) {
        //pr_info("Releasing memory block: %x\n", (u32)(block->paddr));
        dma_free_coherent(scanner.device, scanner.cache_size, block->data, block->paddr);
        //pr_info("release kmalloc block : %x\n",(u32)(block));
        kfree(block);       // 释放链表节点
    }
}

// mmap 的关闭回调，处理解除映射
static void vm_close(struct vm_area_struct *vma)
{
    struct memory_block *block = vma->vm_private_data;

    if (block) {
        list_add_tail(&block->list, &memory_pool); // 添加到dma池
    }
}

static const struct vm_operations_struct vm_ops = {
    .close = vm_close,
};

static int scanner_mmap(struct file *file, struct vm_area_struct *vma)
{
    struct memory_block *block;

    if (!list_empty(&ready_queue)) {
        block = list_first_entry(&ready_queue, struct memory_block, list);
        list_del(&block->list);                // 从链表中移除
    }
    else if (finish_flag) {
        finish_flag = 0;
        printk("Transmission completed\r\n");
        return -ENOBUFS;   // 全部数据处理完成
    }
    else
        return -ENOMEM;  // 无可用数据

    // 设置虚拟内存区域操作
    vma->vm_ops = &vm_ops;
    vma->vm_private_data = block; // 记录内存块指针

    // 将内存块映射到用户空间
    if (remap_pfn_range(vma, vma->vm_start,
                        block->paddr >> PAGE_SHIFT,
                        scanner.cache_size, vma->vm_page_prot)) {
        return -EAGAIN;
    }
  
    return 0;
}

/*
* @description		: 打开设备
* @param – inode		: 传递给驱动的inode
* @param – filp		: 设备文件，file结构体有个叫做private_data的成员变量
* 					  一般在open的时候将private_data指向设备结构体。
* @return			: 0 成功;其他 失败
*/
static int scanner_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static irqreturn_t scanner_isr(int irq, void * dev_id) {
    struct scanner_regs __iomem * regs = (struct scanner_regs __iomem *)scanner.membase;
    struct memory_block *block = NULL;
    irqreturn_t ret = IRQ_NONE;
    u32 data = 0;

    data = readl(&regs->control);
    if (data & FINISH_INTERRUPT_MASK) {
        block = finish_itr_func();
        /* 清除中断标志位 */
        writel(data & (~FINISH_INTERRUPT_MASK) , &regs->control);
        if (block != NULL) {
            writel((u32)(block->paddr), &regs->cache_addr);
        } 
        finish_flag = 1;
    }
    else if (data & READY_INTERRUPT_MASK) {
        /* 获取新的dma内存块，更新cache_addr寄存器 */
        block = ready_itr_func();
        if (block != NULL) {
            writel((u32)(block->paddr), &regs->cache_addr);
        } 
        /* 清除中断标志位 */
        writel(data & (~READY_INTERRUPT_MASK) , &regs->control);
    }

    ret = IRQ_HANDLED;
    return ret;
}

static int scanner_startup(void) {
    int ret;

    ret = request_irq(scanner.irq, scanner_isr, IRQF_TRIGGER_HIGH, DRIVER_NAME, (void *)(&scanner));
    if (ret) return ret;

    printk("!! scanner startup !!\r\n");
    return 0;
}

static ssize_t scanner_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    struct scanner_regs __iomem * regs = (struct scanner_regs __iomem *)scanner.membase;
    struct trandata regdes;
    int retvalue = 0;

    retvalue = copy_from_user(&regdes, buf, 8);
    if (0 > retvalue) {
        printk(KERN_ERR "Kernel write failed!\r\n");
        return -EFAULT;
    }

    switch(regdes.number) {
        case 0: regdes.data = readl(&regs->control); break;
        case 1: regdes.data = readl(&regs->cache_addr); break;
        case 2: regdes.data = readl(&regs->cache_size); break;
        case 3: regdes.data = readl(&regs->line_num);  break;
        case 4: regdes.data = readl(&regs->interval_time);  break;
        case 5: regdes.data = readl(&regs->chose);  break;
        case 6: regdes.data = readl(&regs->black_offset);  break;
        default: printk(KERN_ERR "No reg number %u(0~3)!\r\n", regdes.number); return -EFAULT;
    }

    retvalue = copy_to_user(buf+4, &regdes.data, 4);

    if (0 > retvalue) {
        printk(KERN_ERR "Kernel read failed!\r\n");
        return -EFAULT;
    }

    return cnt;
}

/*
* @description		: 向设备写数据 
* @param – filp		: 设备文件，表示打开的文件描述符
* @param – buf		: 要写给设备写入的数据
* @param – cnt		: 要写入的数据长度
* @param – offt		: 相对于文件首地址的偏移
* @return			: 写入的字节数，如果为负值，表示写入失败
*/
static ssize_t scanner_write(struct file *filp, const char __user *buf,
            size_t cnt, loff_t *offt)
{
    struct scanner_regs __iomem * regs = (struct scanner_regs __iomem *)scanner.membase;
    int retvalue = 0;
    struct trandata regdes;
    retvalue = copy_from_user(&regdes, buf, cnt);

    if (0 > retvalue) {
      printk(KERN_ERR "Kernel write failed!\r\n");
      return -EFAULT;
    }
 
    switch(regdes.number) {
        case 0: writel(regdes.data, &regs->control); break;
        case 1: 
            writel(regdes.data, &regs->cache_addr); break;
        case 2: 
        {
            writel(regdes.data, &regs->cache_size); 
            scanner.cache_size = regdes.data;
            break;
        }
        case 3:  writel(regdes.data, &regs->line_num);  break;
        case 4:  writel(regdes.data, &regs->interval_time);  break;
        case 5:  writel(regdes.data, &regs->chose);  break;
        case 6:  writel(regdes.data, &regs->black_offset);  break;
        default: printk(KERN_ERR "No reg number %u!(0~3)\r\n", regdes.number); return -EFAULT;
    }

    return 0;
}

static long scanner_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct memory_block * block = NULL;
    int i, ret = 0;
    u32 data = 0;
    struct scanner_regs __iomem * regs = (struct scanner_regs __iomem *)scanner.membase;

    switch (cmd) {
        case CMD_ALLOC_MEM: {
            for (i = 0; i < arg; i++) {
                ret = add_block_to_pool();
                if (ret < 0) {
                    printk("Memory pool pre allocation failed\r\n");
                    return -1;
                }
            }
            block = list_first_entry(&memory_pool, struct memory_block, list);
            writel((u32)(block->paddr), &regs->cache_addr);
            break;
        }
        case CMD_SET_DPI : {
            if (arg > 2) 
                return -EINVAL;	
            data = readl(&regs->control);
            writel(((data & (~SCAN_MODE_BIT_MASK)) | arg), &regs->control);
            if (arg == 0) 
                scanner.dpi = 300;
            else if (arg == 1) 
                scanner.dpi = 600;
            else if (arg == 2) 
                scanner.dpi = 1200;
            break;
        }
        default: break;
    }
    return 0;
}


static int scanner_release(struct inode *inode, struct file *filp)
{
    struct memory_block *block, *tmp;

    // 清理内存池
    list_for_each_entry_safe(block, tmp, &memory_pool, list) {
        list_del(&block->list);
        free_block(block);
    }

    list_for_each_entry_safe(block, tmp, &ready_queue, list) {
        list_del(&block->list);
        free_block(block);
    }
    finish_flag = 0;

    return 0;
}

static void scanner_init(struct device *dev, u32 mapbase, void __iomem *membase, int irq)
{
    scanner.membase = membase;
    scanner.irq = irq;
    scanner.mapbase = mapbase;
    scanner.dma_mask = DMA_BIT_MASK(32);
    scanner.device->dma_mask = &scanner.dma_mask;
    scanner.device->coherent_dma_mask = DMA_BIT_MASK(32);
    scanner.dpi = 300;
    scanner_startup();
}

/* 设备操作函数 */
static struct file_operations scanner_fops = {
    .owner = THIS_MODULE,
    .open = scanner_open,
    .write = scanner_write,
    .read = scanner_read,
    .mmap    = scanner_mmap,
    .release = scanner_release,
    .unlocked_ioctl = scanner_ioctl,
};

static int scanner_probe(struct platform_device *pdev)
{
    struct resource * iomem;
    void __iomem * ioaddr;
    int irq, ret;

    printk(KERN_INFO "scanner driver and device has matched!\r\n");
    /* 从设备树中获取资源 */
    iomem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    ioaddr = devm_ioremap_resource(&pdev->dev, iomem);

    /* 从设备树中获取中断号 */
    irq = platform_get_irq(pdev, 0);
    if (irq <= 0) return -ENXIO;


    /* 初始化cdev */
    ret = alloc_chrdev_region(&scanner.devid, 0, DEVICE_COUNT, DEVICE_NAME);

    scanner.cdev.owner = THIS_MODULE;
    cdev_init(&scanner.cdev, &scanner_fops);

    /* 添加cdev */
    ret = cdev_add(&scanner.cdev, scanner.devid, DEVICE_COUNT);
    if (ret)
        goto out1;

    /* 创建类class */
    scanner.class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(scanner.class)) {
        ret = PTR_ERR(scanner.class);
        goto out2;
    }

    /* 创建设备 */
    scanner.device = device_create(scanner.class, &pdev->dev,
                scanner.devid, NULL, DEVICE_NAME);
    if (IS_ERR(scanner.device)) {
        ret = PTR_ERR(scanner.device);
        goto out3;
    }

    /* 初始化 */
    scanner_init(&pdev->dev, iomem->start, ioaddr, irq);

    return 0;

out3:
    class_destroy(scanner.class);
        
out2:
    cdev_del(&scanner.cdev);
        
out1:
    unregister_chrdev_region(scanner.devid, DEVICE_COUNT);
        
    return ret;
}

/*
* @description		: platform驱动模块卸载时此函数会执行
* @param – dev		: platform设备指针
* @return			: 0，成功;其他负值,失败
*/
static int scanner_remove(struct platform_device *dev)
{
    printk(KERN_INFO "scanner platform driver remove!\r\n");

    free_irq(scanner.irq, (void *)(&scanner));

    scanner.irq = -1;
    scanner.mapbase = 0;
    scanner.membase = NULL;
    finish_flag = 0;

    /* 注销设备 */
    device_destroy(scanner.class, scanner.devid);

    /* 注销类 */
    class_destroy(scanner.class);

    /* 删除cdev */
    cdev_del(&scanner.cdev);

    /* 注销设备号 */
    unregister_chrdev_region(scanner.devid, DEVICE_COUNT);

    return 0;
}

/* 匹配列表 */
static const struct of_device_id scanner_of_match[] = {
    { .compatible = "ucu-scanner" },
    { /* Sentinel */ }
};

/* platform驱动结构体 */
static struct platform_driver scanner_driver = {
    .driver = {
        .name = DRIVER_NAME,	// 驱动名字，用于和设备匹配
        .of_match_table = scanner_of_match,			// 设备树匹配表，用于和设备树中定义的设备匹配
    },
    .probe          = scanner_probe,		// probe函数
    .remove         = scanner_remove,	// remove函数
};

/*
* @description		: 模块入口函数
* @param			: 无
* @return			: 无
*/
static int __init scanner_driver_init(void)
{
    return platform_driver_register(&scanner_driver);
}

/*
* @description		: 模块出口函数
* @param			: 无
* @return			: 无
*/
static void __exit scanner_driver_exit(void)
{
    platform_driver_unregister(&scanner_driver);
}

module_init(scanner_driver_init);
module_exit(scanner_driver_exit);

MODULE_AUTHOR("ucu");
MODULE_DESCRIPTION("Scanner Platform Driver");
MODULE_LICENSE("GPL");
