#include "unisoc_log_main.h"
#include <sys/statfs.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <stdio.h>
#include "log_control.h"
#include "misc_usb.h"

extern char modem_name_para[32];
extern uint32_t query_panic_addr;

struct ql_usb_device_info s_usb_device_info[8];

struct usbfs_getdriver
{
    unsigned int interface;
    char driver[256];
};
struct usbfs_ioctl
{
    int ifno;       /* interface 0..N ; negative numbers reserved */
    int ioctl_code; /* MUST encode size + direction of data so the
                     * macros in <asm/ioctl.h> give correct values */
    void *data;     /* param buffer (in, or out) */
};


static int get_value_from_file(const char *fname, int base)
{
    char buff[64] = {'\0'};

    int fd = open(fname, O_RDONLY);
    if (fd <= 0)
    {
        if (errno != ENOENT)
            LogInfo("Fail to open %s,  errno: %d (%s)\n", fname, errno, strerror(errno));
        return -1;
    }
    if (read(fd, buff, sizeof(buff)) == -1) {
    }
    close(fd);
    return strtoul(buff, NULL, base);
}

static const char *get_value_from_uevent(const char *uevent,  const char *key) {
    FILE *fp;
    static char line[256];

    fp = fopen(uevent, "r");
    if (fp == NULL)
    {
        LogInfo("fail to fopen %s, errno: %d (%s)\n", uevent, errno, strerror(errno));
        return "-1";
    }

    //dbg_time("%s\n", uevent);
    while(fgets(line, sizeof(line), fp))
    {
        if(line[strlen(line) - 1] == '\n' || line[strlen(line) - 1] == '\r')
        {
            line[strlen(line) - 1] = '\0';
        }

        //dbg_time("%s\n", line);
        if(strStartsWith(line, key) && line[strlen(key)] == '=')
        {
            fclose(fp);
            return &line[strlen(key)+1];
        }
    }

    fclose(fp);

    return "-1";
}

static void ql_find_usb_interface_info(const char *intfpath, struct ql_usb_interface_info *pIntf)
{
    DIR *dev_dir = NULL;
    struct dirent *dev = NULL;
    char devpath[256+32];

    dev_dir = opendir(intfpath);
    if(!dev_dir)
    {
        LogInfo("fail to opendir('%s'), errno: %d (%s)\n", intfpath, errno, strerror(errno));
        return;
    }

    while (NULL != (dev = readdir(dev_dir)))
    {
        if(!strncasecmp(dev->d_name, "ep_", 3))
        {
            int ep = strtoul(&dev->d_name[3], NULL, 16);
            if (ep&0x80)
                pIntf->ep_in = ep;
            else
                 pIntf->ep_out = ep;
       }
    }

    closedir(dev_dir);

    snprintf(devpath, sizeof(devpath), "%.256s/bInterfaceClass", intfpath);
    pIntf->bInterfaceClass = get_value_from_file(devpath, 16);

    snprintf(devpath, sizeof(devpath), "%.256s/bInterfaceSubClass", intfpath);
    pIntf->bInterfaceSubClass = get_value_from_file(devpath, 16);

    snprintf(devpath, sizeof(devpath), "%.256s/bInterfaceProtocol", intfpath);
    pIntf->bInterfaceProtocol = get_value_from_file(devpath, 16);

    snprintf(devpath, sizeof(devpath), "%.256s/bNumEndpoints", intfpath);
    pIntf->bNumEndpoints = get_value_from_file(devpath, 16);

#if 1
    LogInfo("Interface Class: %02x, SubClass: %02x, Protocol: %02x\n",
        pIntf->bInterfaceClass, pIntf->bInterfaceSubClass, pIntf->bInterfaceProtocol);
    LogInfo("NumEndpoints: %d, ep_in: %02x, ep_out: %02x\n", pIntf->bNumEndpoints, pIntf->ep_in, pIntf->ep_out);
#endif
}

static void ql_find_tty_info(const char *intfpath, char *ttyport, size_t len)
{
    DIR *dev_dir = NULL;
    struct dirent *dev = NULL;
    char devpath[256];
    int wait_tty_register = 10;

    ttyport[0] = '\0';

_scan_tty:
    // find tty device
    dev_dir = opendir(intfpath);
    if(!dev_dir)
    {
        LogInfo("fail to opendir('%s'), errno: %d (%s)\n", intfpath, errno, strerror(errno));
        return;
    }

    while(NULL != (dev = readdir(dev_dir)))
    {
        if(!strncasecmp(dev->d_name, "tty", 3))
        {
            snprintf(ttyport, len, "/dev/%.16s", dev->d_name);
            break;
        }
    }

    closedir(dev_dir);

    if(!strcmp(ttyport, "/dev/tty")) { //find tty not ttyUSBx or ttyACMx
        snprintf(devpath, sizeof(devpath), "%.128s/tty", intfpath);

        dev_dir = opendir(devpath);
        if(dev_dir)
        {
            while(NULL != (dev = readdir(dev_dir)))
            {
                if(!strncasecmp(dev->d_name, "tty", 3))
                {
                        snprintf(ttyport, len, "/dev/%.16s", dev->d_name);
                        break;
                }
            }
            closedir(dev_dir);
        }
    }

    if(ttyport[0] == '\0' && wait_tty_register)
    {
        usleep(100*1000); //maybe other files not ready
        wait_tty_register--;
        goto _scan_tty;
    }
}

static void ql_get_dm_major_and_minor(const char *devname, struct ql_usb_device_info *ql_dev)
{
    char devpath[256];

    if(devname[0] == '\0')
        return;

    snprintf(devpath, sizeof(devpath), "/sys/class/tty/%.27s/uevent", devname + strlen("/dev/"));
    if(access(devpath, F_OK) && errno == ENOENT)
        return;

    ql_dev->dm_major = atoi(get_value_from_uevent(devpath, "MAJOR"));
    ql_dev->dm_minor = atoi(get_value_from_uevent(devpath, "MINOR"));
}
#define IOCTL_USBFS_DISCONNECT _IO('U', 22)
#define IOCTL_USBFS_CONNECT _IO('U', 23)

static int usbfs_is_kernel_driver_alive(int fd, int ifnum)
{
    struct usbfs_getdriver getdrv;
    getdrv.interface = ifnum;
    if(ioctl(fd, USBDEVFS_GETDRIVER, &getdrv) < 0)
    {
        if (errno != ENODATA)
            LogInfo("%s ioctl USBDEVFS_GETDRIVER on interface %d failed, kernel driver may be inactive\n", __func__, ifnum);
        return 0;
    }
    LogInfo("%s find interface %d has match the driver %s\n", __func__, ifnum, getdrv.driver);
    return 1;
}

static void usbfs_detach_kernel_driver(int fd, int ifnum)
{
    struct usbfs_ioctl operate;
    operate.data = NULL;
    operate.ifno = ifnum;
    operate.ioctl_code = IOCTL_USBFS_DISCONNECT;
    if (ioctl(fd, USBDEVFS_IOCTL, &operate) < 0)
        LogInfo("%s detach kernel driver failed\n", __func__);
    else
        LogInfo("%s detach kernel driver success\n", __func__);
}

int ql_usbfs_open_interface(const struct ql_usb_device_info *usb_dev, int intf)
{
    char devname[64];
    int dev_mknod_and_delete_after_use = 0;
    int usbfd = -1;
    int ret;

    snprintf(devname, sizeof(devname), "/dev/%s", usb_dev->devname);
    if(access(devname, F_OK) && errno == ENOENT)
    {
        char *p = strstr(devname+strlen("/dev/"), "/");

        while (p)
        {
            p[0] = '_';
            p = strstr(p, "/");
        }

#define MKDEV(__ma, __mi) (((__ma & 0xfff) << 8) | (__mi & 0xff) | ((__mi & 0xfff00) << 12))
        if(mknod(devname, S_IFCHR|0666, MKDEV(usb_dev->major, usb_dev->minor)))
        {
            devname[1] = 't';
            devname[2] = 'm';
            devname[3] = 'p';

            if(mknod(devname, S_IFCHR|0666, MKDEV(usb_dev->major, usb_dev->minor)))
            {
                LogInfo("Fail to mknod %s, errno : %d (%s)\n", devname, errno, strerror(errno));
            }
        }

        dev_mknod_and_delete_after_use = 1;
    }

    usbfd = open(devname, O_RDWR | O_NDELAY);
    if(dev_mknod_and_delete_after_use)
    {
        remove(devname);
    }

    if(usbfd == -1)
    {
        LogInfo("usbfs open %s failed, errno: %d (%s)\n", devname, errno, strerror(errno));
        return -1;
    }

    if (usbfs_is_kernel_driver_alive(usbfd, intf))
        usbfs_detach_kernel_driver(usbfd, intf);

    ret = ioctl(usbfd, USBDEVFS_CLAIMINTERFACE, &intf); // attach usbfs driver
    if(ret != 0)
    {
        LogInfo("ioctl USBDEVFS_CLAIMINTERFACE failed, errno = %d(%s)\n", errno, strerror(errno));
        close(usbfd);
        return -1;
    }

    return usbfd;
}

int find_unisoc_modules(void)
{
    DIR *usb_dir = NULL;
    struct dirent *usb = NULL;
    const char *usbpath = "/sys/bus/usb/devices";
    struct ql_usb_device_info ql_dev;
    int modules_num = 0;
    char idProduct[128] = {0};

    usb_dir = opendir(usbpath);
    if (NULL == usb_dir)
        return modules_num;

    while (NULL != (usb = readdir(usb_dir)))
    {
        char devpath[256] = {'\0'};
        if (usb->d_name[0] == '.' || usb->d_name[0] == 'u')
            continue;

        memset(&ql_dev, 0, sizeof(struct ql_usb_device_info));

        snprintf(devpath, sizeof(devpath), "%.24s/%.16s/idVendor", usbpath, usb->d_name);
        ql_dev.idVendor = get_value_from_file(devpath, 16);

        snprintf(devpath, sizeof(devpath), "%.24s/%.16s/idProduct", usbpath, usb->d_name);
        ql_dev.idProduct = get_value_from_file(devpath, 16);

        if ((ql_dev.idVendor == 0x2cb7 && (ql_dev.idProduct&0x0a04))
            || (ql_dev.idVendor == 0x2cb7 && (ql_dev.idProduct&0x0a05))
            || (ql_dev.idVendor == 0x2cb7 && (ql_dev.idProduct&0x0a06))
            || (ql_dev.idVendor == 0x2cb7 && (ql_dev.idProduct&0x0a09))
            || (ql_dev.idVendor == 0x1782 && (ql_dev.idProduct&0x4d00))
            )
        {
        }
        else
        {
            continue;
        }

        usleep(100*1000); //maybe other files not ready

        snprintf(devpath, sizeof(devpath), "%.24s/%.16s/bNumInterfaces", usbpath, usb->d_name);
        ql_dev.bNumInterfaces = get_value_from_file(devpath, 10);

        snprintf(devpath, sizeof(devpath), "%.24s/%.16s/uevent", usbpath, usb->d_name);
        ql_dev.major = atoi(get_value_from_uevent(devpath, "MAJOR"));
        ql_dev.minor = atoi(get_value_from_uevent(devpath, "MINOR"));
        ql_dev.busnum = atoi(get_value_from_uevent(devpath, "BUSNUM"));
        ql_dev.devnum = atoi(get_value_from_uevent(devpath, "DEVNUM"));
        strncpy(ql_dev.devname, get_value_from_uevent(devpath, "DEVNAME"), sizeof(ql_dev.devname));

        snprintf(devpath, sizeof(devpath), "%s/%.16s/bcdDevice", usbpath, usb->d_name);
        ql_dev.bcdDevice = get_value_from_file(devpath, 10);

        memset(&ql_dev.dm_intf, 0xff, sizeof(struct ql_usb_interface_info));
        memset(&ql_dev.general_intf, 0xff, sizeof(struct ql_usb_interface_info));
        ql_dev.general_type = -1;

        ql_dev.dm_intf.bInterfaceNumber = 0;    //include EC20 GW
        if (ql_dev.bNumInterfaces > 1)
        {
            /*Modify for MBB0063-677 20230625 zhangboxing begin*/
            sprintf(idProduct,"%04x",ql_dev.idProduct);
            LogInfo("idProduct============%s\n",idProduct);
            if(strcmp(idProduct,"0a04") == 0)
            {
                ql_dev.dm_intf.bInterfaceNumber = 3;
                ql_dev.general_intf.bInterfaceNumber = 4;  //log_intf
            }
            else
            {
                ql_dev.dm_intf.bInterfaceNumber = 4;
                ql_dev.general_intf.bInterfaceNumber = 5;  //log_intf
                //ql_dev.general_type = RG500U_LOG;
            }
            /*Modify for MBB0063-677 20230625 zhangboxing end*/

        }

_rescan_dm:
        snprintf(devpath, sizeof(devpath), "%.24s/%.16s/%.16s:1.%d", usbpath, usb->d_name, usb->d_name, ql_dev.dm_intf.bInterfaceNumber);
        if (access(devpath, F_OK))
            continue;

        ql_find_usb_interface_info(devpath, &ql_dev.dm_intf);
        if (ql_dev.dm_intf.bInterfaceNumber == 0
            && ql_dev.dm_intf.bInterfaceClass == 0x02
            && ql_dev.dm_intf.bInterfaceSubClass == 0x0e) { //EM05-G 's interface 0 is MBIM
            ql_dev.dm_intf.bInterfaceNumber = 3;
            goto _rescan_dm;
        }

        ql_find_tty_info(devpath, ql_dev.ttyDM, sizeof(ql_dev.ttyDM));
        if(ql_dev.ttyDM[0])
        {
            ql_get_dm_major_and_minor(ql_dev.ttyDM, &ql_dev);
        }

        if(ql_dev.general_intf.bInterfaceNumber != 0xFF)
        {
            snprintf(devpath, sizeof(devpath), "%.24s/%.16s/%.16s:1.%d", usbpath, usb->d_name, usb->d_name, ql_dev.general_intf.bInterfaceNumber);
            if (access(devpath, F_OK))
                continue;

            ql_find_usb_interface_info(devpath, &ql_dev.general_intf);
            ql_find_tty_info(devpath, ql_dev.ttyGENERAL, sizeof(ql_dev.ttyGENERAL));
        }

        snprintf(ql_dev.usbdevice_pah, sizeof(ql_dev.usbdevice_pah), "%.24s/%.16s", usbpath, usb->d_name);
        LogInfo("Find [%d] idVendor=%04x, idProduct=%04x, bNumInterfaces=%d, ttyDM=%s, ttyGENERAL=%s, busnum=%03d, dev=%03d, usbdevice_pah=%s\n",
            modules_num, ql_dev.idVendor, ql_dev.idProduct, ql_dev.bNumInterfaces, ql_dev.ttyDM,
            ql_dev.ttyGENERAL, ql_dev.busnum, ql_dev.devnum, ql_dev.usbdevice_pah);

        if (modules_num < 8)
            s_usb_device_info[modules_num++] = ql_dev;
    }

    closedir(usb_dir);
    return modules_num;
}

int ql_usbfs_read(int usbfd, int ep_in, void *pbuf, unsigned len)
{
    struct usbdevfs_bulktransfer bulk;
    int n = 0;

    bulk.ep = ep_in;
    bulk.len = len;
    bulk.data = (void *)pbuf;
    bulk.timeout = 5000; // keep waiting

    n = ioctl(usbfd, USBDEVFS_BULK, &bulk);
    if(n < 0)
    {
        //LogInfo("%s n = %d, errno: %d (%s)\n", __func__, n, errno, strerror(errno));
    }
    else if(n == 0)
    {
        //zero length packet
    }

    return n;
}

int ql_usbfs_write(int usbfd, int ep_out, const void *pbuf, unsigned len)
{
    struct usbdevfs_urb bulk;
    struct usbdevfs_urb *urb = &bulk;
    int n = 0;

    memset(urb, 0, sizeof(struct usbdevfs_urb));
    urb->type = USBDEVFS_URB_TYPE_BULK;
    urb->endpoint = ep_out;
    urb->status = -1;
    urb->buffer = (void *)pbuf;
    urb->buffer_length = len;
    urb->usercontext = urb;
    urb->flags = 0;

    n = ioctl(usbfd, USBDEVFS_SUBMITURB, urb);
    if(n < 0)
    {
        LogInfo("%s submit n = %d, errno: %d (%s)\n", __func__, n, errno, strerror(errno));
        return 0;
    }

    urb = NULL;
    n = ioctl(usbfd, USBDEVFS_REAPURB, &urb);
    if(n < 0)
    {
        LogInfo("%s reap n = %d, errno: %d (%s)\n", __func__, n, errno, strerror(errno));
        return 0;
    }

    if(urb && urb->status == 0 && urb->actual_length)
    {
        LogInfo("urb->actual_length = %u\n", urb->actual_length);
        return urb->actual_length;
    }

    return 0;
}
