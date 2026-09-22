/*******************************************************************
 *  CopyRight(C) 2022-2026  Fibocom Wireless Inc
 *******************************************************************
 * FileName : misc_usb.c
 * DESCRIPTION : usb code
 * Author   : Frank.zhou
 * Date     : 2022.08.22
 *******************************************************************/
#include "misc_usb.h"

static int usbfs_bulk_write(fibo_usbdev_t *pdev, void *data, int len, int portnum)
{
    struct usbdevfs_urb bulk;
    struct usbdevfs_urb *urb = NULL;
    int ret = -1;

    // LogInfo("start\n");
    memset(&bulk, 0, sizeof(bulk));
    bulk.type = USBDEVFS_URB_TYPE_BULK;
    bulk.endpoint = pdev->bulk_ep_out[portnum];
    bulk.status = -1;
    bulk.buffer = (void *)data;
    bulk.buffer_length = MIN(len, MAX_USBFS_BULK_OUT_SIZE);
    bulk.usercontext = &bulk;

    if (len <= MAX_USBFS_BULK_OUT_SIZE && pdev->usb_need_zero_package && !(bulk.buffer_length % pdev->wMaxPacketSize))
    {
        //bulk.flags = USBDEVFS_URB_ZERO_PACKET;  //usb_need_zero_package is always 0 in this project, will not enter this condition.
        LogInfo("< USBDEVFS_URB_ZERO_PACKET >\n");
    }

    do {
        ret = ioctl(pdev->usbdev, USBDEVFS_SUBMITURB, &bulk);
    } while ((ret < 0) && (errno == EINTR));

    if (ret != 0) {
        LogInfo("USBDEVFS_SUBMITURB failed, ret: %d, errno:%d(%s)\n", ret, errno, strerror(errno));
        return -1;
    }

    do {
        urb = NULL;
        ret = ioctl(pdev->usbdev, USBDEVFS_REAPURB, &urb);
    } while ((ret < 0) && (errno == EINTR));

    if (ret == 0 && urb && urb->status == 0 && urb->actual_length) {
        return urb->actual_length;
    }

    return -1;
}

static int fibo_usb_write(const void *handle, void *pbuf, int size, int portnum)
{
    fibo_usbdev_t *pdev = (fibo_usbdev_t *)handle;
    int cur_size = 0, ret = 0;

    //LogInfo("start\n");
    while (cur_size < size)
    {
        if (pdev->usbdev >= 0)
        {
            ret = usbfs_bulk_write(pdev, (uint8_t *)pbuf+cur_size, size-cur_size, portnum);
        }
        else if (pdev->ttyfd[portnum] >= 0)
        {
            if (poll_wait(pdev->ttyfd[portnum], POLLOUT, 3000)) {
                LogInfo("poll_wait timeout\n");
                break;
            }
            ret = write(pdev->ttyfd[portnum], (uint8_t *)pbuf+cur_size, size-cur_size);
        }
        else {
            LogInfo("dev port is not opened\n");
            return -1;
        }

        if (ret < 0 && errno != ETIMEDOUT) {
            LogInfo("ret:%d, errno:%d(%s)\n", ret, errno, strerror(errno));
            LogInfo("cur_size:%d\n", cur_size);
            break;
        }
        cur_size += ret;
    }

    return cur_size;
}

static int usbfs_bulk_read(fibo_usbdev_t *pdev, void *pbuf, int len, int timeout_msec, int portnum)
{
    int ret = -1;
    struct usbdevfs_bulktransfer bulk;

    // LogInfo("start\n");
    bulk.ep = pdev->bulk_ep_in[portnum];
    bulk.len = MIN(len, MAX_USBFS_BULK_IN_SIZE);
    bulk.data = pbuf;
    bulk.timeout = timeout_msec;

    do {
        ret = ioctl(pdev->usbdev, USBDEVFS_BULK, &bulk);
    } while ((ret < 0) && (errno == EINTR));

    return ret;
}

static int fibo_usb_read(const void *handle, void *pbuf, int size, int portnum)
{
    fibo_usbdev_t *pdev = (fibo_usbdev_t *)handle;
    int ret = 0;
    int timeout_msec = 3000;

    //LogInfo("start\n");

    if (pdev->usbdev >= 0)
    {
        ret = usbfs_bulk_read(pdev, (uint8_t *)pbuf, size, timeout_msec, portnum);
    }
    else if (pdev->ttyfd[portnum] >= 0)
    {
        if (poll_wait(pdev->ttyfd[portnum] , POLLIN, timeout_msec)) {
            LogInfo("poll_wait timeout\n");
        }
        else
        {
            ret = read(pdev->ttyfd[portnum], (uint8_t *)pbuf, size);
        }
    }
    else {
        LogInfo("dev port is not opened\n");
        return -1;
    }

    if (ret < 0 && errno != ETIMEDOUT) {
        LogInfo("ret:%d, errno:%d(%s)\n", ret, errno, strerror(errno));
    }

    return ret;
}

static int fibo_get_busname_by_uevent(const char *uevent, char *busname)
{
    FILE *fp = NULL;
    char line[MAX_PATH_LEN] = {0};
    int MAJOR = 0, MINOR = 0;
    char DEVTYPE[64] = {0}, PRODUCT[64] = {0};

    fp = fopen(uevent, "r");
    if (fp == NULL) {
        LogInfo("fopen %s failed, errno:%d(%s)\n", uevent, errno, strerror(errno));
        return -1;
    }

    // LogInfo("----------------------------------------\n");
    // LogInfo("%s\n", uevent);
    while (fgets(line, sizeof(line), fp))
    {
        if (line[strlen(line) - 1] == '\n' || line[strlen(line) - 1] == '\r') {
            line[strlen(line) - 1] = 0;
        }

        // LogInfo("%s\n", line);
        if (fibo_strStartsWith(line, "MAJOR="))
        {
            MAJOR = atoi(&line[strlen("MAJOR=")]);
        }
        else if (fibo_strStartsWith(line, "MINOR="))
        {
            MINOR = atoi(&line[strlen("MINOR=")]);
        }
        //start: modify for kernel version 2.6, 2022.01.22
        else if (fibo_strStartsWith(line, "DEVICE="))
        {
            strncpy(busname, &line[strlen("DEVICE=")], MAX_PATH_LEN);
        }
        //end: modify for kernel version 2.6, 2022.01.22
        else if (fibo_strStartsWith(line, "DEVNAME="))
        {
            strncpy(busname, &line[strlen("DEVNAME=")], MAX_PATH_LEN);
        }
        else if (fibo_strStartsWith(line, "DEVTYPE="))
        {
            strncpy(DEVTYPE, &line[strlen("DEVTYPE=")], sizeof(DEVTYPE));
        }
        else if (fibo_strStartsWith(line, "PRODUCT="))
        {
            strncpy(PRODUCT, &line[strlen("PRODUCT=")], sizeof(PRODUCT));
        }
    }
    fclose(fp);
    // LogInfo("----------------------------------------\n");

    if (MAJOR != 189  || MINOR == 0 || busname[0] == 0
        || DEVTYPE[0] == 0 || PRODUCT[0] == 0
        || fibo_strStartsWith(DEVTYPE, "usb_device") == 0) {
        return -1;
    }

    return 0;
}

int strStartsWith(const char *line, const char *src)
{
    for ( ; *line != '\0' && *src != '\0'; line++, src++) {
        if (*line != *src) {
            return 0;
        }
    }

    return *src == '\0';
}


int fibo_get_ttyport_by_syspath(fibo_usbdev_t *udev)
{
    DIR *busdir = NULL;
    struct dirent *dent = NULL;
    char acm_dir_path[FIBO_BUF_SIZE+2*EXTEND];
    int flag_ttyacm=0;

    busdir = opendir(udev->syspath);
    if (busdir == NULL) {
        printf("%s: open [%s] busdir failed\n", __func__, udev->syspath);
        return -1;
    }

    while ((dent = readdir(busdir)) != NULL)
    {
        if (strStartsWith(dent->d_name, "tty")) {
            if(strcmp(dent->d_name, "tty"))
            {
                snprintf(udev->portname, sizeof(udev->portname), "/dev/%s", dent->d_name);
                closedir(busdir);
            }
            else
            {
                closedir(busdir);
                snprintf(acm_dir_path, sizeof(acm_dir_path), "%s/tty", udev->syspath);
                flag_ttyacm = 1;
                break;
            }
            return 0;
        }
    }

    if(flag_ttyacm == 1)
    {
        busdir = opendir(acm_dir_path);
        if (busdir == NULL) {
            printf("%s: open [%s] acm dir failed\n", __func__, acm_dir_path);
            return -1;
        }

        while ((dent = readdir(busdir)) != NULL)
        {
            if (strStartsWith(dent->d_name, "ttyACM")) {
                snprintf(udev->portname, sizeof(udev->portname), "/dev/%s", dent->d_name);
                closedir(busdir);
                return 0;
            }
        }
    }

    closedir(busdir);

    return -1;
}


static int fibo_get_usbsys_val(const char *sys_filename, int base)
{
    char buff[64] = {0};
    int ret_val = -1;

    int fd = open(sys_filename, O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    if (read(fd, buff, sizeof(buff)) <= 0) {
        LogInfo("read:%s failed\n", sys_filename);
    }
    else {
        ret_val = strtoul(buff, NULL, base);
    }
    close(fd);

    return ret_val;
}

int fibo_get_usb_Interface6_SubClass(fibo_usbdev_t *pdev)
{
    int i=0;
    int bInterfaceSubClass=0;
    char interface6_syspath[FIBO_BUF_SIZE] = {0};
    char sys_filename[MAX_PATH_LEN+EXTEND] = {0};

    if(pdev == NULL)
        return NONE;

    strcpy(interface6_syspath, pdev->syspath);
    i = strlen(interface6_syspath);
    //LogInfo("interface6_syspath is %s, len is %d \r\n", interface6_syspath, i);
    /*interface6*/
    interface6_syspath[i-1] = '6';
    LogInfo("interface6_syspath is %s, len is %d \r\n", interface6_syspath, i);

    snprintf(sys_filename, sizeof(sys_filename), "%s/bInterfaceSubClass", interface6_syspath);
    bInterfaceSubClass = fibo_get_usbsys_val(sys_filename, 16);
    LogInfo("bInterfaceSubClass is %x\r\n", bInterfaceSubClass);
    return bInterfaceSubClass;
}

fibo_usbdev_t *fibo_get_fibocom_device(fibo_usbdev_t *(* find_devices_in_table)(int idvendor, int idproduct), char *portname, int portnum)
{
    DIR *usbdir = NULL;
    struct dirent *dent = NULL;
    char sys_filename[MAX_PATH_LEN] = {0};
    int idVendor = 0, idProduct = 0;
    int bNumInterfaces = 0, bConfigurationValue = 0;
    fibo_usbdev_t *pdev = NULL, *ret_udev = NULL;
    uint32_t modules_num = 0;
    char syspath[MAX_PATH_LEN] = {0};

    if (fibo_strStartsWith(portname, USB_DIR_BASE)) {
        strcpy(syspath, portname);
        portname[0] = 0;
    }

    usbdir = opendir(USB_DIR_BASE);
    if (usbdir == NULL) {
        return NULL;
    }

    while ((dent = readdir(usbdir)) != NULL)
    {
        if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, "..") || fibo_strStartsWith(dent->d_name, "usb")) {
            continue;
        }

        snprintf(sys_filename, sizeof(sys_filename), "%s/%s/idVendor", USB_DIR_BASE, dent->d_name);
        if ((idVendor = fibo_get_usbsys_val(sys_filename, 16)) <= 0) {
            continue;
        }

        snprintf(sys_filename, sizeof(sys_filename), "%s/%s/idProduct", USB_DIR_BASE, dent->d_name);
        if ((idProduct = fibo_get_usbsys_val(sys_filename, 16)) <= 0) {
            continue;
        }

        snprintf(sys_filename, sizeof(sys_filename), "%s/%s/bConfigurationValue", USB_DIR_BASE, dent->d_name);
        if ((bConfigurationValue = fibo_get_usbsys_val(sys_filename, 10)) <= 0) {
            continue;
        }

        snprintf(sys_filename, sizeof(sys_filename), "%s/%s/bNumInterfaces", USB_DIR_BASE, dent->d_name);
        if ((bNumInterfaces = fibo_get_usbsys_val(sys_filename, 10)) <= 0) {
            continue;
        }

        pdev = (* find_devices_in_table)(idVendor, idProduct);
        if (pdev != NULL) {
            LogInfo("----------------------------------\n");
            LogInfo("ModuleName: %s\n", pdev->ModuleName);
            LogInfo("idVendor: %04x\n", pdev->idVendor);
            LogInfo("idProduct: %04x\n", pdev->idProduct);
            LogInfo("bNumInterfaces: %d\n", bNumInterfaces);
            LogInfo("bConfigurationValue: %d\n", bConfigurationValue);
            snprintf(sys_filename, sizeof(sys_filename), "%s/%s/uevent", USB_DIR_BASE, dent->d_name);
            fibo_get_busname_by_uevent(sys_filename, pdev->busname);
            LogInfo("busname: %s\n", pdev->busname);
            LogInfo("portnum is %d, ifnum is %d\r\n",portnum, pdev->ifnum[portnum]);



            if(portname[0]&& strcmp(portname, "9008"))
            {
                LogInfo("portname: %s \n", portname);
                strcpy(pdev->portname,portname);
                ret_udev = pdev;
                modules_num = 1;
                goto END;
            }

            if (syspath[0]) {
                for (pdev->ifnum[portnum] = 0; pdev->ifnum[portnum]<bNumInterfaces; pdev->ifnum[portnum]++) {
                    snprintf(pdev->syspath, sizeof(pdev->syspath), "%s/%s/%s:%d.%d", USB_DIR_BASE, dent->d_name, dent->d_name, bConfigurationValue, pdev->ifnum[portnum]);
                    LogInfo("portname: %s\n", pdev->syspath);

                    if ((syspath[0] && !strcmp(syspath, pdev->syspath))) {
                        ret_udev = pdev;
                        modules_num = 1;
                        goto END;
                    }
                }
            }
            else
            {
                snprintf(pdev->syspath, sizeof(pdev->syspath), "%s/%s/%s:%d.%d", USB_DIR_BASE, dent->d_name, dent->d_name, bConfigurationValue, pdev->ifnum[portnum]);
                LogInfo("pdev syspath is %s\r\n", pdev->syspath);
                fibo_get_ttyport_by_syspath(pdev);
                LogInfo("portname: %s -- %s\n", pdev->portname, pdev->syspath);
                ret_udev = pdev;
                modules_num++;
            }
            LogInfo("----------------------------------\n");
        }
        usleep(10000);
    }

END:
    if (usbdir) {
        closedir(usbdir);
        usbdir = NULL;
    }

    if (modules_num == 0) {
        //LogInfo("Can not find Fibocom module.\n");
    }
    else if (modules_num == 1) {
        if (ret_udev->ifnum[portnum] >= 0) {
            LogInfo("%d module match, ifnum[%d]:%d\n", modules_num, portnum, ret_udev->ifnum[portnum]);
            return ret_udev;
        }
    }
    else if (portname[0] == 0 && syspath[0] == 0)
    {
        LogInfo("%d modules found\n", modules_num);
        LogInfo("please set portname <-d /dev/XXX> or set syspath <-s /sys/bus/usb/devices/X-X/X-X:X.X>\n");
    }

    return NULL;
}

static int usbfs_check_kernel_driver(int fd, int ifnum)
{
    struct usbdevfs_getdriver getdrv;

    //LogInfo("\n");
    getdrv.interface = ifnum;
    if (ioctl(fd, USBDEVFS_GETDRIVER, &getdrv) >= 0) {
        struct usbdevfs_ioctl operate;

        operate.data = NULL;
        operate.ifno = ifnum;
        operate.ioctl_code = USBDEVFS_DISCONNECT;

        LogInfo("find interface %d has match the driver %s\n", ifnum, getdrv.driver);
        if (ioctl(fd, USBDEVFS_IOCTL, &operate) < 0) {
            LogInfo("ioctl USBDEVFS_IOCTL failed.\n");
            return -1;
        }

        LogInfo("OK.\n");
    }
    else if (errno != ENODATA) {
        LogInfo("ioctl USBDEVFS_GETDRIVER failed, errno:%d(%s)\n", errno, strerror(errno));
    }

    return 0;
}

static int usbfs_bulk_open(fibo_usbdev_t *pdev)
{
    char devdesc[MAX_PATH_LEN*2] = {0};
    char devname[MAX_PATH_LEN+EXTEND] = {0};
    size_t desc_length = 0, len = 0;
    int bInterfaceNumber = 0;

    LogInfo("start\n");
    if (pdev == NULL) {
        LogInfo("pdev is NULL\n");
        return -1;
    }

    //start: modify for kernel version 2.6, 2022.01.22
    if (strstr(pdev->busname, "/proc/bus/usb")) {
        snprintf(devname, sizeof(devname), "%s", pdev->busname);
    }
    else {
        snprintf(devname, sizeof(devname), "/dev/%s", pdev->busname);
    }
    //end: modify for kernel version 2.6, 2022.01.22

    if (access(devname, F_OK | R_OK| W_OK)) {
        LogInfo("access %s failed, errno:%d(%s)\n", devname, errno, strerror(errno));
        return -1;
    }

    pdev->usbdev = open(devname, O_RDWR | O_NOCTTY);
    if (pdev->usbdev < 0) {
        LogInfo("open %s failed, errno:%d(%s)\n", devname, errno, strerror(errno));
        return -1;
    }
    LogInfo("[%s] OK.\n", devname);

    desc_length = read(pdev->usbdev, devdesc, sizeof(devdesc));
    for (len=0; len<desc_length;)
    {
        struct usb_descriptor_header *h = (struct usb_descriptor_header *)(devdesc + len);

        if (h->bLength == sizeof(struct usb_device_descriptor) && h->bDescriptorType == USB_DT_DEVICE)
        {
            // struct usb_device_descriptor *device = (struct usb_device_descriptor *)h;
            // LogInfo("P: idVendor: %04x idProduct:%04x\n", device->idVendor, device->idProduct);
        }
        else if (h->bLength == sizeof(struct usb_config_descriptor) && h->bDescriptorType == USB_DT_CONFIG)
        {
            // struct usb_config_descriptor *config = (struct usb_config_descriptor *)h;
            // LogInfo("C: bNumInterfaces: %d\n", config->bNumInterfaces);
        }
        else if (h->bLength == sizeof(struct usb_interface_descriptor) && h->bDescriptorType == USB_DT_INTERFACE)
        {
            struct usb_interface_descriptor *interface = (struct usb_interface_descriptor *)h;

            // LogInfo("I: If#= %d Alt= %d #EPs= %d Cls=%02x Sub=%02x Prot=%02x\n",
                // interface->bInterfaceNumber, interface->bAlternateSetting, interface->bNumEndpoints,
                // interface->bInterfaceClass, interface->bInterfaceSubClass, interface->bInterfaceProtocol);
            bInterfaceNumber = interface->bInterfaceNumber;
        }
        else if (h->bLength == USB_DT_ENDPOINT_SIZE && h->bDescriptorType == USB_DT_ENDPOINT)
        {
            struct usb_endpoint_descriptor *endpoint = (struct usb_endpoint_descriptor *)h;
            int i;

            for(i=0; i<2; i++)
            {
                if ((pdev->ifnum[i] == bInterfaceNumber) && (endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK)
                {
                    if (endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) {
                        pdev->bulk_ep_in[i] = endpoint->bEndpointAddress;
                        LogInfo("bulk_ep_in[%d]:0x%02X\n", i, pdev->bulk_ep_in[i]);
                    } else {
                        pdev->bulk_ep_out[i] = endpoint->bEndpointAddress;
                        LogInfo("bulk_ep_out[%d]:0x%02X\n", i, pdev->bulk_ep_out[i]);
                    }
                    pdev->wMaxPacketSize = endpoint->wMaxPacketSize;
                    LogInfo("wMaxPacketSize:%d\n", endpoint->wMaxPacketSize);
                }
            }
        }
        len += h->bLength;
    }

    return 0;
}

void GHT_SetAdvancedOptions(int fd, speed_t baud)
{
    int status = 0;
    struct termios options;
    struct termios options_cpy;

    fcntl(fd, F_SETFL, 0);

    // get the parameters
    tcgetattr(fd, &options);

    // Do like minicom: set 0 in speed options
    cfsetispeed(&options, 0);
    cfsetospeed(&options, 0);

    options.c_iflag = IGNBRK;

    // Enable the receiver and set local mode and 8N1
    options.c_cflag = (CLOCAL | CREAD | CS8 | HUPCL);
    //enable hardware flow control (CNEW_RTCCTS)
    options.c_cflag &= ~CRTSCTS;
    // Set speed
    options.c_cflag |= baud;

    // set raw input
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(INLCR | ICRNL | IGNCR);

    // set raw output
    options.c_oflag &= ~OPOST;
    options.c_oflag &= ~OLCUC;
    options.c_oflag &= ~ONLRET;
    options.c_oflag &= ~ONOCR;
    options.c_oflag &= ~OCRNL;

    // Set the new options for the port...
    options_cpy = options;
    tcsetattr(fd, TCSANOW, &options);
    options = options_cpy;

    // Do like minicom: set speed to 0 and back
    options.c_cflag &= ~baud;
    tcsetattr(fd, TCSANOW, &options);
    options = options_cpy;

    sleep(1);

    options.c_cflag |= baud;
    tcsetattr(fd, TCSANOW, &options);

    ioctl(fd, TIOCMGET, &status);
    status |= TIOCM_DTR;
    status &= ~TIOCM_CTS;
    ioctl(fd, TIOCMSET, &status);
}

int ttyusb_Open(fibo_usbdev_t *pdev, int portnum)
{
    char *portname = pdev->portname;
    LogInfo("start\n");

    if (access(portname, R_OK))
    {
        LogInfo("access %s failed, errno:%d (%s)\n", portname, errno, strerror(errno));
        return -1;
    }

    pdev->ttyfd[portnum] = open(portname, O_RDWR | O_SYNC);
    if (pdev->ttyfd[portnum] < 0)
    {
        LogInfo("open %s failed, errno:%d (%s)\n", portname, errno, strerror(errno));
        goto error_end;
    }

    GHT_SetAdvancedOptions(pdev->ttyfd[portnum], B115200);

    LogInfo("open %s OK\n", portname);
    return 0;
error_end:
    if (pdev->ttyfd[portnum] >= 0) {
        close(pdev->ttyfd[portnum]);
        pdev->ttyfd[portnum] = -1;
    }
    return -1;
}

int fibo_usb_open(fibo_usbdev_t *pdev, int portnum, int is_ttyUSB_first)
{
    LogInfo("start\n");

    if (pdev == NULL) {
        LogInfo("error: pdev is NULL\n");
        return -1;
    }

    if(is_ttyUSB_first == 1)
    {
        if ((pdev->portname[0] != 0) &&!ttyusb_Open(pdev, portnum))
        {
            goto OK_STEP;
        }
    }

    if (pdev->syspath[0] != 0) {
        if (pdev->usbdev < 0 && usbfs_bulk_open(pdev)) {
            if ((pdev->portname[0] != 0) &&!ttyusb_Open(pdev, portnum)) {
                goto OK_STEP;
            }
            return -1;
        } else {
            if (usbfs_check_kernel_driver(pdev->usbdev, pdev->ifnum[portnum])) {
                return -1;
            }

            if (ioctl(pdev->usbdev, USBDEVFS_CLAIMINTERFACE, &pdev->ifnum[portnum])) {
                LogInfo("ioctl USBDEVFS_CLAIMINTERFACE failed, errno:%d(%s)\n", errno, strerror(errno));
                return -1;
            }
            goto OK_STEP;
        }
    } else {
        LogInfo("portname and syspath are all empty.\n");
        return -1;
    }

OK_STEP:
    pdev->write = fibo_usb_write;
    pdev->read = fibo_usb_read;

    LogInfo("OK\n");
    return 0;
}

int fibo_usb_close(fibo_usbdev_t *pdev, int portnum)
{
    if (pdev == NULL) {
        return 0;
    }

    if (pdev->ttyfd[portnum] >= 0) {
        close(pdev->ttyfd[portnum]);
        pdev->ttyfd[portnum] = -1;
    }

    if (pdev->usbdev >= 0) {
        ioctl(pdev->usbdev, USBDEVFS_RELEASEINTERFACE, &pdev->ifnum[portnum]);
        if(pdev->ifnum[portnum+1] >= 0){
        ioctl(pdev->usbdev, USBDEVFS_RELEASEINTERFACE, &pdev->ifnum[portnum+1]);
        }
        close(pdev->usbdev);
        pdev->usbdev = -1;
    }

    return 0;
}


