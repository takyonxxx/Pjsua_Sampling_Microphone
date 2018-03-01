#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <sys/types.h>  // open
#include <sys/stat.h>
#include <fcntl.h>

#define GPIO_SYS_PATH "/sys/class/gpio/"

static int is_gpio_port_ready = 0;

int gpio_init()
{
    char str[4];

    int export_fd = open(GPIO_SYS_PATH  "export", O_WRONLY);
    if (export_fd < 0) {
        fprintf(stderr, "Error: open %s!", GPIO_SYS_PATH  "export");
        return 0;
    }
    strcpy(str, "232");
    int bytes = write(export_fd, str, 4);
    close(export_fd);
    if (bytes < 4)
        return 0;	

    int dir_fd = open(GPIO_SYS_PATH  "gpio232/direction", O_WRONLY);
    if (dir_fd < 0) {
        fprintf(stderr, "Error: open %s!", GPIO_SYS_PATH  "gpio232/direction");
        return 0;
    }
    strcpy(str, "out");
    bytes = write(dir_fd, str, 4);
    close(dir_fd);
    if (bytes < 4)
        return 0;	

    is_gpio_port_ready = 1;

    return 1;
}

int gpio_set_ptt()
{
    char str[2];

    if (is_gpio_port_ready) {
        int val_fd = open(GPIO_SYS_PATH "gpio232/value", O_WRONLY);
        if (val_fd < 0) {
            fprintf(stderr, "Error: open %s!", GPIO_SYS_PATH "gpio232/value");
            return 0;
        }
        strcpy(str, "1");
        int bytes = write(val_fd, str, 2);
        close(val_fd);
        if (bytes < 2)
            return 0;	
    }
    return 1;
}

int gpio_release_ptt()
{
    char str[2];

    if (is_gpio_port_ready) {
        int val_fd = open(GPIO_SYS_PATH "gpio232/value", O_WRONLY);
        if (val_fd < 0) {
            fprintf(stderr, "Error: open %s!", GPIO_SYS_PATH "gpio232/value");
            return 0;
        }
        strcpy(str, "0");
        int bytes = write(val_fd, str, 2);
        close(val_fd);
        if (bytes < 2)
            return 0;	
    }
    return 1;
}

int gpio_destroy()
{
    char str[4];

    gpio_release_ptt();

    int unexport_fd = open(GPIO_SYS_PATH "unexport", O_WRONLY);
    if (unexport_fd < 0) {
        fprintf(stderr, "Error: open %s!", GPIO_SYS_PATH "unexport");
        return 0;
    }
    strcpy(str, "232");
    int bytes = write(unexport_fd, str, 4);
    close(unexport_fd);
    if (bytes < 4)
        return 0;	

    is_gpio_port_ready = 0;

    return 1;
}
