#define _POSIX_C_SOURCE 200809L


#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAXLINE 1024
#define SHM_SIZE MAXLINE

typedef struct {
    volatile int flag;
    char data[MAXLINE];
} shared_memory_t;

void error_exit(const char *msg) {
    const char *err_str = strerror(errno);
    write(STDERR_FILENO, msg, strlen(msg));
    write(STDERR_FILENO, ": ", 2);
    write(STDERR_FILENO, err_str, strlen(err_str));
    write(STDERR_FILENO, "\n", 1);
    exit(EXIT_FAILURE);
}

void send_string(shared_memory_t *shm, const char *str) {
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = 1000000L;

    while (shm->flag == 1) {
        nanosleep(&req, NULL);
    }
    size_t len = strlen(str);
    if (len >= MAXLINE) len = MAXLINE - 1;
    memcpy(shm->data, str, len);
    shm->data[len] = '\0';
    shm->flag = 1;
}


void read_input(char *buffer, size_t size) {
    ssize_t n = 0;
    size_t total_read = 0;
    while (total_read < size - 1) {
        n = read(STDIN_FILENO, buffer + total_read, 1);
        if (n <= 0) {
            if (n == 0 && total_read == 0) {
                buffer[0] = '\0';
                return;
            }
            break;
        }
        if (buffer[total_read] == '\n') {
            buffer[total_read] = '\0';
            return;
        }
        total_read += n;
    }
    buffer[total_read] = '\0';
}

int main() {
    char filename1[MAXLINE], filename2[MAXLINE];
    pid_t pid1, pid2;
    const char *child1_shm_name = "/child1_shm";
    const char *child2_shm_name = "/child2_shm";
    shared_memory_t *child1_shm, *child2_shm;
    int shm_fd1, shm_fd2;
    char line[MAXLINE];
    size_t len = 0;
    srand(time(NULL));

    const char *prompt1 = "Enter filename for child1: ";
    write(STDOUT_FILENO, prompt1, strlen(prompt1));
    read_input(filename1, MAXLINE);

    const char *prompt2 = "Enter filename for child2: ";
    write(STDOUT_FILENO, prompt2, strlen(prompt2));
    read_input(filename2, MAXLINE);

    shm_fd1 = shm_open(child1_shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd1 == -1)
        error_exit("shm_open child1");
    if (ftruncate(shm_fd1, sizeof(shared_memory_t)) == -1)
        error_exit("ftruncate child1");
    child1_shm = mmap(NULL, sizeof(shared_memory_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd1, 0);
    if (child1_shm == MAP_FAILED)
        error_exit("mmap child1");
    child1_shm->flag = 0;

    shm_fd2 = shm_open(child2_shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd2 == -1)
        error_exit("shm_open child2");
    if (ftruncate(shm_fd2, sizeof(shared_memory_t)) == -1)
        error_exit("ftruncate child2");
    child2_shm = mmap(NULL, sizeof(shared_memory_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd2, 0);
    if (child2_shm == MAP_FAILED)
        error_exit("mmap child2");
    child2_shm->flag = 0;

    pid1 = fork();
    if (pid1 < 0) {
        error_exit("fork child1");
    }
    if (pid1 == 0) {
        execl("./child", "./child", filename1, child1_shm_name, "SIGUSR1", (char *)NULL);
        error_exit("execl child1");
    }

    pid2 = fork();
    if (pid2 < 0) {
        error_exit("fork child2");
    }
    if (pid2 == 0) {
        execl("./child", "./child", filename2, child2_shm_name, "SIGUSR2", (char *)NULL);
        error_exit("execl child2");
    }

    const char *input_prompt = "Enter strings (Ctrl+D to end):\n";
    write(STDOUT_FILENO, input_prompt, strlen(input_prompt));

    while (1) {
        read_input(line, MAXLINE);
        if (strlen(line) == 0) {
            break;
        }

        int r = rand() % 10;
        if (r < 8) {
            send_string(child1_shm, line);
            if (kill(pid1, SIGUSR1) == -1) {
                error_exit("kill SIGUSR1 to child1");
            }
        } else {
            send_string(child2_shm, line);
            if (kill(pid2, SIGUSR2) == -1) {
                error_exit("kill SIGUSR2 to child2");
            }
        }
    }

    send_string(child1_shm, "TERMINATE");
    send_string(child2_shm, "TERMINATE");
    kill(pid1, SIGUSR1);
    kill(pid2, SIGUSR2);

    if (waitpid(pid1, NULL, 0) == -1)
        error_exit("waitpid pid1");
    if (waitpid(pid2, NULL, 0) == -1)
        error_exit("waitpid pid2");

    munmap(child1_shm, sizeof(shared_memory_t));
    munmap(child2_shm, sizeof(shared_memory_t));
    shm_unlink(child1_shm_name);
    shm_unlink(child2_shm_name);

    return 0;
}
