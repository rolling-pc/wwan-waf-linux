/*
 * Copyright (c) 2016-2017, Linaro Ltd.
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * All rights reserved.
 *
 * ... (原版权声明保持不动) ...
 */
#include <linux/usbdevice_fs.h>
#include <linux/usb/ch9.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libudev.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "qdl.h"
#include "patch.h"
#include "ufs.h"

/* 需在 qdl.h 中添加以下枚举与成员 (此处仅作说明) */
#if 0
// ----- 添加到 qdl.h -----
enum qdl_interface {
    QDL_IF_USB,
    QDL_IF_PCIE
};

struct qdl_device {
    // ... 原有成员 ...
    enum qdl_interface if_type;   // 新增接口类型
};
// ------------------------
#endif

#define MAX_USBFS_BULK_SIZE     (16*1024)

/* PCIe 设备路径 */
#define PCIE_DEVICE_PATH        "/dev/wwan0firehose0"

enum {
    QDL_FILE_UNKNOWN,
    QDL_FILE_PATCH,
    QDL_FILE_PROGRAM,
    QDL_FILE_UFS,
    QDL_FILE_CONTENTS,
};

bool qdl_debug;
static struct qdl_device qdl;

static int detect_type(const char *xml_file)
{
    xmlNode *root;
    xmlDoc *doc;
    xmlNode *node;
    int type = QDL_FILE_UNKNOWN;

    doc = xmlReadFile(xml_file, NULL, 0);
    if (!doc) {
        fprintf(stderr, "[PATCH] failed to parse %s\n", xml_file);
        return -EINVAL;
    }

    root = xmlDocGetRootElement(doc);
    if (!xmlStrcmp(root->name, (xmlChar*)"patches")) {
        type = QDL_FILE_PATCH;
    } else if (!xmlStrcmp(root->name, (xmlChar*)"data")) {
        for (node = root->children; node ; node = node->next) {
            if (node->type != XML_ELEMENT_NODE)
                continue;
            if (!xmlStrcmp(node->name, (xmlChar*)"program")) {
                type = QDL_FILE_PROGRAM;
                break;
            }
            if (!xmlStrcmp(node->name, (xmlChar*)"ufs")) {
                type = QDL_FILE_UFS;
                break;
            }
        }
    } else if (!xmlStrcmp(root->name, (xmlChar*)"contents")) {
        type = QDL_FILE_CONTENTS;
    }

    xmlFreeDoc(doc);

    return type;
}

static int parse_usb_desc(int fd, struct qdl_device *qdl, int *intf)
{
    const struct usb_interface_descriptor *ifc;
    const struct usb_endpoint_descriptor *ept;
    const struct usb_device_descriptor *dev;
    const struct usb_config_descriptor *cfg;
    const struct usb_descriptor_header *hdr;
    unsigned type;
    unsigned out;
    unsigned in;
    unsigned k;
    unsigned l;
    ssize_t n;
    size_t out_size;
    size_t in_size;
    void *ptr;
    void *end;
    char desc[1024];

    n = read(fd, desc, sizeof(desc));
    if (n < 0)
        return n;

    ptr = (void*)desc;
    end = ptr + n;

    dev = ptr;

    if (dev->idVendor != 0x05c6 || dev->idProduct != 0x9008)
        return -EINVAL;

    ptr += dev->bLength;
    if (ptr >= end || dev->bDescriptorType != USB_DT_DEVICE)
        return -EINVAL;

    cfg = ptr;
    ptr += cfg->bLength;
    if (ptr >= end || cfg->bDescriptorType != USB_DT_CONFIG)
        return -EINVAL;

    for (k = 0; k < cfg->bNumInterfaces; k++) {
        if (ptr >= end)
            return -EINVAL;

        do {
            ifc = ptr;
            if (ifc->bLength < USB_DT_INTERFACE_SIZE)
                return -EINVAL;

            ptr += ifc->bLength;
        } while (ptr < end && ifc->bDescriptorType != USB_DT_INTERFACE);

        in = -1;
        out = -1;
        in_size = 0;
        out_size = 0;

        for (l = 0; l < ifc->bNumEndpoints; l++) {
            if (ptr >= end)
                return -EINVAL;

            do {
                ept = ptr;
                if (ept->bLength < USB_DT_ENDPOINT_SIZE)
                    return -EINVAL;

                ptr += ept->bLength;
            } while (ptr < end && ept->bDescriptorType != USB_DT_ENDPOINT);

            type = ept->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK;
            if (type != USB_ENDPOINT_XFER_BULK)
                continue;

            if (ept->bEndpointAddress & USB_DIR_IN) {
                in = ept->bEndpointAddress;
                in_size = ept->wMaxPacketSize;
            } else {
                out = ept->bEndpointAddress;
                out_size = ept->wMaxPacketSize;
            }

            if (ptr >= end)
                break;

            hdr = ptr;
            if (hdr->bDescriptorType == USB_DT_SS_ENDPOINT_COMP)
                ptr += USB_DT_SS_EP_COMP_SIZE;
        }

        if (ifc->bInterfaceClass != 0xff)
            continue;
        if (ifc->bInterfaceSubClass != 0xff)
            continue;
        if (ifc->bInterfaceProtocol != 0xff &&
            ifc->bInterfaceProtocol != 16 &&
            ifc->bInterfaceProtocol != 17)
            continue;

        qdl->fd = fd;
        qdl->in_ep = in;
        qdl->out_ep = out;
        qdl->in_maxpktsize = in_size;
        qdl->out_maxpktsize = out_size;
        qdl->if_type = QDL_IF_USB;   /* 标记为 USB */

        *intf = ifc->bInterfaceNumber;

        return 0;
    }

    return -ENOENT;
}

static int usb_open(struct qdl_device *qdl)
{
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices;
    struct udev_list_entry *dev_list_entry;
    struct udev_monitor *mon;
    struct udev_device *dev;
    const char *dev_node;
    struct udev *udev;
    const char *path;
    struct usbdevfs_ioctl cmd;
    int mon_fd;
    int intf = -1;
    int ret;
    int fd;

    udev = udev_new();
    if (!udev)
        err(1, "failed to initialize udev");

    mon = udev_monitor_new_from_netlink(udev, "udev");
    udev_monitor_filter_add_match_subsystem_devtype(mon, "usb", NULL);
    udev_monitor_enable_receiving(mon);
    mon_fd = udev_monitor_get_fd(mon);

    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "usb");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);

    udev_list_entry_foreach(dev_list_entry, devices) {
        path = udev_list_entry_get_name(dev_list_entry);
        dev = udev_device_new_from_syspath(udev, path);
        dev_node = udev_device_get_devnode(dev);

        if (!dev_node)
            continue;

        fd = open(dev_node, O_RDWR);
        if (fd < 0)
            continue;

        ret = parse_usb_desc(fd, qdl, &intf);
        if (!ret)
            goto found;

        close(fd);
    }

    fprintf(stderr, "Waiting for EDL device\n");

    for (;;) {
        fd_set rfds;

        FD_ZERO(&rfds);
        FD_SET(mon_fd, &rfds);

        ret = select(mon_fd + 1, &rfds, NULL, NULL, NULL);
        if (ret < 0)
            return -1;

        if (!FD_ISSET(mon_fd, &rfds))
            continue;

        dev = udev_monitor_receive_device(mon);
        dev_node = udev_device_get_devnode(dev);

        if (!dev_node)
            continue;

        fd = open(dev_node, O_RDWR);
        if (fd < 0)
            continue;

        ret = parse_usb_desc(fd, qdl, &intf);
        if (!ret)
            goto found;

        close(fd);
    }

    udev_enumerate_unref(enumerate);
    udev_monitor_unref(mon);
    udev_unref(udev);

    return -ENOENT;

found:
    udev_enumerate_unref(enumerate);
    udev_monitor_unref(mon);
    udev_unref(udev);

    cmd.ifno = intf;
    cmd.ioctl_code = USBDEVFS_DISCONNECT;
    cmd.data = NULL;

    ret = ioctl(qdl->fd, USBDEVFS_IOCTL, &cmd);
    if (ret && errno != ENODATA)
        err(1, "failed to disconnect kernel driver");

    ret = ioctl(qdl->fd, USBDEVFS_CLAIMINTERFACE, &intf);
    if (ret < 0)
        err(1, "failed to claim USB interface");

    return 0;
}

/* 新增：打开 PCIe 设备 */
static int pcie_open(struct qdl_device *qdl)
{
    int fd;

    fd = open(PCIE_DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "failed to open %s: %s\n", PCIE_DEVICE_PATH, strerror(errno));
        return -1;
    }

    qdl->fd = fd;
    /* PCIe 无端点概念，设置最大包大小为 0 表示无限制，用于 qdl_write 判断 */
    qdl->in_ep = 0;
    qdl->out_ep = 0;
    qdl->in_maxpktsize = 0;
    qdl->out_maxpktsize = 0;
    qdl->if_type = QDL_IF_PCIE;

    return 0;
}

int qdl_read(struct qdl_device *qdl, void *buf, size_t len, unsigned int timeout)
{
    /* PCIe 路径：带超时的读取 */
    if (qdl->if_type == QDL_IF_PCIE) {
        struct pollfd pfd;
        int ret;
        ssize_t n;

        pfd.fd = qdl->fd;
        pfd.events = POLLIN;

        while (1) {
            ret = poll(&pfd, 1, (int)timeout);
            if (ret < 0) {
                if (errno == EINTR)     /* 被信号中断，重试 */
                    continue;
                return -errno;
            }
            if (ret == 0) {             /* 超时 */
                errno = ETIMEDOUT;
                return -1;
            }

            /* 检查可读事件 */
            if (pfd.revents & POLLIN) {
                n = read(qdl->fd, buf, len);
                if (n < 0) {
                    if (errno == EINTR)
                        continue;
                    return -errno;
                }
                return (int)n;          /* 成功读取，返回实际字节数 */
            }

            /* 设备异常或断开 */
            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
                return -EIO;
        }
    }

    /* USB 路径，原逻辑不变 */
    struct usbdevfs_bulktransfer bulk = {};

    bulk.ep = qdl->in_ep;
    bulk.len = len;
    bulk.data = buf;
    bulk.timeout = timeout;

    return ioctl(qdl->fd, USBDEVFS_BULK, &bulk);
}

int qdl_write(struct qdl_device *qdl, const void *buf, size_t len)
{
    /* PCIe 路径：直接写入，不分包，不补零长度包 */
    if (qdl->if_type == QDL_IF_PCIE) {
        unsigned char *data = (unsigned char *)buf;
        size_t remaining = len;
        while (remaining > 0) {
            ssize_t n = write(qdl->fd, data, remaining);
            if (n < 0) {
                fprintf(stderr, "ERROR: pcie write failed, errno = %d (%s)\n",
                        errno, strerror(errno));
                return -1;
            }
            remaining -= n;
            data += n;
        }
        return len;
    }

    /* USB 路径，原逻辑不变 */
    unsigned char *data = (unsigned char*) buf;
    struct usbdevfs_bulktransfer bulk = {};
    unsigned count = 0;
    size_t len_orig = len;
    int n;

    while(len > 0) {
        int xfer;
        xfer = (len > qdl->out_maxpktsize) ? qdl->out_maxpktsize : len;

        bulk.ep = qdl->out_ep;
        bulk.len = xfer;
        bulk.data = data;
        bulk.timeout = 1000;

        n = ioctl(qdl->fd, USBDEVFS_BULK, &bulk);
        if(n != xfer) {
            fprintf(stderr, "ERROR: n = %d, errno = %d (%s)\n",
                    n, errno, strerror(errno));
            return -1;
        }
        count += xfer;
        len -= xfer;
        data += xfer;
    }

    if (len_orig % qdl->out_maxpktsize == 0) {
        bulk.ep = qdl->out_ep;
        bulk.len = 0;
        bulk.data = NULL;
        bulk.timeout = 1000;

        n = ioctl(qdl->fd, USBDEVFS_BULK, &bulk);
        if (n < 0)
            return n;
    }

    return count;
}

static void print_usage(void)
{
    extern const char *__progname;
    fprintf(stderr,
            "%s [--debug] [--storage <emmc|nand|ufs>] [--finalize-provisioning] [--include <PATH>] [--interface <usb|pcie>] <prog.mbn> [<program> <patch> ...]\n",
            __progname);
}

int main(int argc, char **argv)
{
    char *prog_mbn, *storage="ufs";
    char *incdir = NULL;
    char *interface = "usb";   /* 默认 USB */
    int type;
    int ret;
    int opt;
    bool qdl_finalize_provisioning = false;

    static struct option options[] = {
        {"debug", no_argument, 0, 'd'},
        {"include", required_argument, 0, 'i'},
        {"finalize-provisioning", no_argument, 0, 'l'},
        {"storage", required_argument, 0, 's'},
        {"interface", required_argument, 0, 't'},   /* 新增 -t / --interface */
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "di:s:t:", options, NULL )) != -1) {
        switch (opt) {
        case 'd':
            qdl_debug = true;
            break;
        case 'i':
            incdir = optarg;
            break;
        case 'l':
            qdl_finalize_provisioning = true;
            break;
        case 's':
            storage = optarg;
            break;
        case 't':                                     /* 解析接口类型 */
            interface = optarg;
            if (strcmp(interface, "usb") && strcmp(interface, "pcie")) {
                fprintf(stderr, "Invalid interface: %s (must be usb or pcie)\n", interface);
                print_usage();
                return 1;
            }
            break;
        default:
            print_usage();
            return 1;
        }
    }

    if ((optind + 2) > argc) {
        print_usage();
        return 1;
    }

    if (!strcmp(interface, "usb")) {
    	prog_mbn = argv[optind++];
    }

    do {
        type = detect_type(argv[optind]);
        if (type < 0 || type == QDL_FILE_UNKNOWN)
            errx(1, "failed to detect file type of %s\n", argv[optind]);

        switch (type) {
        case QDL_FILE_PATCH:
            ret = patch_load(argv[optind]);
            if (ret < 0)
                errx(1, "patch_load %s failed", argv[optind]);
            break;
        case QDL_FILE_PROGRAM:
            ret = program_load(argv[optind], !strcmp(storage, "nand"));
            if (ret < 0)
                errx(1, "program_load %s failed", argv[optind]);
            break;
        case QDL_FILE_UFS:
            ret = ufs_load(argv[optind],qdl_finalize_provisioning);
            if (ret < 0)
                errx(1, "ufs_load %s failed", argv[optind]);
            break;
        default:
            errx(1, "%s type not yet supported", argv[optind]);
            break;
        }
    } while (++optind < argc);

    /* 根据接口类型打开设备 */
    if (!strcmp(interface, "pcie")) {
        ret = pcie_open(&qdl);
    } else {
        ret = usb_open(&qdl);
    }
    if (ret)
        return 1;

    if (!strcmp(interface, "usb")) {
    	qdl.mappings[0] = prog_mbn;
    	ret = sahara_run(&qdl, qdl.mappings, true);
    	if (ret < 0) 
      	    return 1;
    }

    ret = firehose_run(&qdl, incdir, storage);
    if (ret < 0)
        return 1;

    return 0;
}
