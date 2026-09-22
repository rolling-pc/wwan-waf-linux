/*******************************************************************
 *  CopyRight(C) 2022-2026  Fibocom Wireless Inc
 *******************************************************************
 * FileName : main.c
 * Author   : Frank.zhou
 * Date     : 2022.07.26
 * Used     : Fibocom_MultiPlatform_log_tool
 *******************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "misc_usb.h"

int socket_port = -1;
char tty_port[FIBO_BUF_SIZE] = {0};
int logtype = -1;
platform_enum g_platform=NONE;

#define TOOL_VERSION "Fibocom_MultiPlatform_logtool_V1.4.0.3"

fibo_usbdev_t *zte_find_devices_in_table(int idvendor, int idproduct);
fibo_usbdev_t *qcom_find_devices_in_table(int idvendor, int idproduct);
fibo_usbdev_t *uinisoc_find_devices_in_table(int idvendor, int idproduct);
fibo_usbdev_t *unisoc_x_find_devices_in_table(int idvendor, int idproduct);
fibo_usbdev_t *qcom_find_pcie_devices();
fibo_usbdev_t *udx710_find_pcie_devices();
fibo_usbdev_t *qcom_find_pciedump_devices();
fibo_usbdev_t *sl8563_x_find_devices_in_table(int idvendor, int idproduct);
fibo_usbdev_t *eigencomm_find_devices_in_table(int idvendor, int idproduct);
int hostproxy();

static fibo_usbdev_t *find_devices_in_table(int idvendor, int idproduct)
{
    fibo_usbdev_t *udev = NULL;

#ifdef CONFIG_ZTE
    if((g_platform == NONE) || (g_platform == V3E) || (g_platform == V3T))
    {
        udev = zte_find_devices_in_table(idvendor, idproduct);
        if (udev != NULL) {
            return udev;
        }
    }
#endif

#ifdef CONFIG_QCOM
    if((g_platform == NONE) || (g_platform == QCOM))
    {
        udev = qcom_find_devices_in_table(idvendor, idproduct);
        if (udev != NULL) {
            return udev;
        }
    }
#endif

#ifdef CONFIG_UNISOC_X
    if((g_platform == NONE) || (g_platform == UNISOC_X))
    {
        udev = unisoc_x_find_devices_in_table(idvendor, idproduct);
        if (udev != NULL) {
            return udev;
        }
    }
#endif

#ifdef CONFIG_UNISOC
    if((g_platform == NONE) || (g_platform == UNISOC))
    {
        udev = uinisoc_find_devices_in_table(idvendor, idproduct);
        if (udev != NULL) {
            return udev;
        }
    }
#endif

#ifdef CONFIG_EIGENCOMM
    if((g_platform == NONE) || (g_platform == EIGENCOMM))
    {
        udev = eigencomm_find_devices_in_table(idvendor, idproduct);
        if (udev != NULL) {
            return udev;
        }
    }
#endif

#ifdef CONFIG_SL8563_X
    if((g_platform == NONE) || (g_platform == SL8563))
    {
        udev = sl8563_x_find_devices_in_table(idvendor, idproduct);
        if (udev != NULL) {
            return udev;
        }
    }
#endif
    return NULL;
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
        printf("[%s] read:%s failed\n", __func__, sys_filename);
    }
    else {
        ret_val = strtoul(buff, NULL, base);
    }
    close(fd);

    return ret_val;
}


static fibo_usbdev_t *fibo_dectect_fibocom_modules(char *portname)
{
    DIR *usbdir = NULL;
    struct dirent *dent = NULL;
    char sys_filename[FIBO_BUF_SIZE] = {0};
    int idVendor = 0, idProduct = 0;
    int bNumInterfaces = 0, bConfigurationValue = 0;
    fibo_usbdev_t *udev = NULL, *ret_udev = NULL;
    unsigned modules_num = 0;
    char syspath[FIBO_BUF_SIZE] = {0};

    if (strStartsWith(portname, USB_DIR_BASE)) {
        strcpy(syspath, portname);
        portname[0] = 0;
    }

#ifdef CONFIG_QCOM
    /*zhangboxing IRP-20230200297 2023/03/13  begin*/
    if(strcmp(portname,"/dev/mhi_DIAG")== 0)
    {
        udev = qcom_find_pcie_devices();
        return udev;
    }
    /*zhangboxing IRP-20230200297 2023/03/13  end*/
    if(strcmp(portname,"/dev/mhi_SAHARA")== 0)
    {
        udev = qcom_find_pciedump_devices();
        return udev;
    }
#endif

#ifdef CONFIG_UNISOC
    if(strcmp(portname,"/dev/sdiag_nr")== 0)
    {
        udev = udx710_find_pcie_devices();
        return udev;
    }

    if(strcmp(portname,"/dev/slog_nr")== 0)
    {
        udev = udx710_find_pcie_devices();
        return udev;
    }
#endif

    usbdir = opendir(USB_DIR_BASE);
    if (usbdir == NULL) {
        return NULL;
    }

    while ((dent = readdir(usbdir)) != NULL)
    {
        if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0) {
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

        udev = find_devices_in_table(idVendor, idProduct);
        if (udev != NULL) {
            int i = 0;
            ret_udev = udev;
            printf("----------------------------------\n");
            printf("ModuleName: %s\n", udev->ModuleName);
            printf("idVendor: %04x\n", udev->idVendor);
            printf("idProduct: %04x\n", udev->idProduct);
            printf("bNumInterfaces: %d\n", bNumInterfaces);
            printf("bConfigurationValue: %d\n", bConfigurationValue);
            snprintf(sys_filename, sizeof(sys_filename), "%s/%s/uevent", USB_DIR_BASE, dent->d_name);
            // fibo_get_busname_by_uevent(sys_filename, udev->busname);
            // printf("busname: %s\n", udev->busname);
            for (i = 0; i<bNumInterfaces; i++) {
                snprintf(udev->syspath, sizeof(udev->syspath), "%s/%s/%s:%d.%d", USB_DIR_BASE, dent->d_name, dent->d_name, bConfigurationValue, i);
                fibo_get_ttyport_by_syspath(udev);
                printf("portname: %s -- %s\n", udev->portname, udev->syspath);
                printf("%s , %s; %s , %s\r\n", syspath, udev->syspath, portname, udev->portname);

                if ((syspath[0] && !strcmp(syspath, udev->syspath)) || (portname[0] &&!strcmp(portname, udev->portname))) {
                    udev->ifnum[0] = i;
                    ret_udev = udev;
                    modules_num = 1;
                    printf("find the ifnum is %d\r\n", udev->ifnum[0]);
                    goto END;
                }
                memset(udev->portname, 0, sizeof(udev->portname));
            }
            modules_num++;
            printf("----------------------------------\n");
        }
        usleep(10000);
    }

END:
    if (usbdir) {
        closedir(usbdir);
        usbdir = NULL;
    }

    if (modules_num == 0) {
        printf("Can not find Fibocom module.\n");
    }
    else if (modules_num == 1) {
        printf("%d module match.\n", modules_num);
        return ret_udev;
    } else if (portname[0] == 0 && syspath[0] == 0) {
        printf("%d modules found\n", modules_num);
        printf("please set portname <-d /dev/XXX> or set syspath <-d /sys/bus/usb/devices/X-X/X-X:X.X>\n");
    }

    return NULL;
}

int main(int argc, char **argv)
{
    int opt = -1;
    char portname[FIBO_BUF_SIZE] = {0};
    fibo_usbdev_t *udev = NULL;

    printf("TOOL_VERSION:%s\n", TOOL_VERSION);

    umask(0);
    optind = 1;//must set to 1
    while ((opt = getopt(argc, argv, "d:p:P:s:a:m:n:i:u:w:f:t:h")) != -1)/* Modify for MBB0201-84 20240525 willa.liu */
    {
        switch (opt)
        {
            case 'd':
                snprintf(portname, sizeof(portname), "%s", optarg);
                snprintf(tty_port, sizeof(tty_port), "%s", portname);
                break;
            case 'p':
                snprintf(portname, sizeof(portname), "%s", optarg);
                snprintf(tty_port, sizeof(tty_port), "%s", portname);
                break;
            case 'P':
                socket_port = atoi(optarg);
                break;
            case 'a':
                logtype = 2;
                break;
            case 't':
                g_platform = atoi(optarg);
                break;
            default:;
        }
    }

    if(socket_port != -1)
    {
        //hostproxy
        if((socket_port < 9000 || socket_port > 9999) && strlen(tty_port)!= 0)
        {
            //hostproxy
            hostproxy();
        }
        else if(socket_port >= 9000 && socket_port <= 9999)
        {
            printf("UDX710 Port proxy mode\n");
        }
        else
        {
            printf("The port is not specified or the proxy port is incorrectly specified\n");
            printf("Example:./logtool -P 8888 -d /dev/ttyUSB0\n");
            return 0;
        }
    }

    udev = fibo_dectect_fibocom_modules(portname);
    if (udev == NULL) {
        return -1;
    }

    return udev->log_main_function(argc, argv);
}
