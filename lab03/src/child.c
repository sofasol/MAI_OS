#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAXLINE 1024

typedef struct {
    volatile int flag;
    char data[MAXLINE];
} shared_memory_t;

shared_memory_t *shm;
int shm_fd;
int terminate = 0;
char *filename;
int file_fd;
int sig_num;

void error_exit(const char *msg) {
    const char *err_str = strerror(errno);
    write(STDERR_FILENO, msg, strlen(msg));
    write(STDERR_FILENO, ": ", 2);
    write(STDERR_FILENO, err_str, strlen(err_str));
    write(STDERR_FILENO, "\n", 1);
    exit(EXIT_FAILURE);
}

void signal_handler(int signum) {
    if (shm->flag == 1) {
        if (strcmp(shm->data, "TERMINATE") == 0) {
            terminate = 1;
        } else {
            size_t len = strlen(shm->data);
            char inverted[MAXLINE];
            for (size_t i = 0; i < len; i++) {
                inverted[i] = shm->data[len - i - 1];
            }
            inverted[len] = '\n';
            inverted[len + 1] = '\0';
            ssize_t written = write(file_fd, inverted, len + 1);
            if (written == -1) {
                error_exit("write to file");
            }
        }
        shm->flag = 0;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        const char *msg = "Usage: ./child <filename> <shm_name> <signal_name>\n";
        write(STDERR_FILENO, msg, strlen(msg));
        exit(EXIT_FAILURE);
    }

    filename = argv[1];
    const char *shm_name = argv[2];
    const char *signal_name = argv[3];

    shm_fd = shm_open(shm_name, O_RDWR, 0666);
    if (shm_fd == -1)
        error_exit("shm_open child");
    shm = mmap(NULL, sizeof(shared_memory_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED)
        error_exit("mmap child");

    file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (file_fd == -1)
        error_exit("open file");

    if (strcmp(signal_name, "SIGUSR1") == 0) {
        sig_num = SIGUSR1;
    } else if (strcmp(signal_name, "SIGUSR2") == 0) {
        sig_num = SIGUSR2;
    } else {
        error_exit("Invalid signal name");
    }
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(sig_num, &sa, NULL) == -1) {
        error_exit("sigaction");
    }

    while (!terminate) {
        pause();
    }

    close(file_fd);
    munmap(shm, sizeof(shared_memory_t));
    close(shm_fd);

    return 0;
}
