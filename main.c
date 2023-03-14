#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define LED_ON  1
#define LED_OFF 0

int main(int argc, char* argv[]) {
    char* file_name = NULL;
    unsigned short status = 0;
    int fd = 0;

    if (argc != 3) {
        printf("num of params should be 3!\r\n");
        return -1;
    }
    file_name = argv[1];
    status = atoi(argv[2]);

    fd = open(file_name, O_RDWR);
    if (fd < 0) {
        printf("open fail\r\n");
        return -1;
    }

    if (status == LED_ON || status == LED_OFF) {
        printf("status = %d.\r\n", status);
        write(fd, &status, sizeof(status));
    }

    close(fd);

    return 0;
}