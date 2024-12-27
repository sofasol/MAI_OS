#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#define BUFFER_SIZE 1024

void error_exit(const char *msg) {
    ssize_t len = 0;
    while (msg[len] != '\0') len++;
    write(STDERR_FILENO, msg, len);
    exit(EXIT_FAILURE);
}

void write_str(int fd, const char *str) {
    size_t len = 0;
    while (str[len] != '\0') len++;
    ssize_t total_written = 0;
    while (total_written < (ssize_t)len) {
        ssize_t written = write(fd, str + total_written, len - total_written);
        if (written == -1) {
            error_exit("write failed\n");
        }
        total_written += written;
    }
}

char* read_line() {
    size_t size = BUFFER_SIZE;
    size_t len = 0;
    char *buffer = (char*)malloc(size);
    if (!buffer) {
        error_exit("malloc failed\n");
    }
    while (1) {
        char c;
        ssize_t bytes = read(STDIN_FILENO, &c, 1);
        if (bytes == -1) {
            free(buffer);
            error_exit("read failed\n");
        } else if (bytes == 0) { 
            if (len == 0) {
                buffer[0] = '\0';
            }
            break;
        }
        if (c == '\n') {
            break;
        }
        buffer[len++] = c;
        if (len >= size) {
            size += BUFFER_SIZE;
            char *new_buffer = realloc(buffer, size);
            if (!new_buffer) {
                free(buffer);
                error_exit("realloc failed\n");
            }
            buffer = new_buffer;
        }
    }
    buffer[len] = '\0';
    return buffer;
}

int main() {
    if (time(NULL) == ((time_t) -1)) {
        error_exit("time failed\n");
    }
    srand(time(NULL));

    write_str(STDOUT_FILENO, "Введите имя файла для child1: ");
    char *filename1 = read_line();
    if (filename1 == NULL) {
        error_exit("Failed to read filename1\n");
    }
    if (strlen(filename1) == 0) {
        free(filename1);
        error_exit("Filename1 is empty\n");
    }

    write_str(STDOUT_FILENO, "Введите имя файла для child2: ");
    char *filename2 = read_line();
    if (filename2 == NULL) {
        free(filename1);
        error_exit("Failed to read filename2\n");
    }
    if (strlen(filename2) == 0) {
        free(filename1);
        free(filename2);
        error_exit("Filename2 is empty\n");
    }

    int pipe1[2];
    if (pipe(pipe1) == -1) {
        free(filename1);
        free(filename2);
        error_exit("pipe1 creation failed\n");
    }

    int pipe2[2];
    if (pipe(pipe2) == -1) {
        close(pipe1[0]);
        close(pipe1[1]);
        free(filename1);
        free(filename2);
        error_exit("pipe2 creation failed\n");
    }

    pid_t pid1 = fork();
    if (pid1 == -1) {
        close(pipe1[0]);
        close(pipe1[1]);
        close(pipe2[0]);
        close(pipe2[1]);
        free(filename1);
        free(filename2);
        error_exit("fork1 failed\n");
    }
    if (pid1 == 0) {
        if (close(pipe1[1]) == -1) {
            error_exit("child1 close pipe1 write end failed\n");
        }
        if (close(pipe2[0]) == -1) {
            error_exit("child1 close pipe2 read end failed\n");
        }
        if (close(pipe2[1]) == -1) {
            error_exit("child1 close pipe2 write end failed\n");
        }

        if (dup2(pipe1[0], STDIN_FILENO) == -1) {
            error_exit("child1 dup2 pipe1 failed\n");
        }
        if (close(pipe1[0]) == -1) {
            error_exit("child1 close pipe1 read end after dup2 failed\n");
        }

        int file_fd = open(filename1, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (file_fd == -1) {
            error_exit("child1 open file1 failed\n");
        }


        if (dup2(file_fd, STDOUT_FILENO) == -1) {
            close(file_fd);
            error_exit("child1 dup2 file1 failed\n");
        }
        if (close(file_fd) == -1) {
            error_exit("child1 close file1 failed\n");
        }

        execl("./child", "child", NULL);
        error_exit("child1 execl child failed\n");
    }

    pid_t pid2 = fork();
    if (pid2 == -1) {
        close(pipe1[0]);
        close(pipe1[1]);
        close(pipe2[0]);
        close(pipe2[1]);
        free(filename1);
        free(filename2);
        waitpid(pid1, NULL, 0);
        error_exit("fork2 failed\n");
    }
    if (pid2 == 0) {
        if (close(pipe2[1]) == -1) {
            error_exit("child2 close pipe2 write end failed\n");
        }
        if (close(pipe1[0]) == -1) {
            error_exit("child2 close pipe1 read end failed\n");
        }
        if (close(pipe1[1]) == -1) {
            error_exit("child2 close pipe1 write end failed\n");
        }

        if (dup2(pipe2[0], STDIN_FILENO) == -1) {
            error_exit("child2 dup2 pipe2 failed\n");
        }
        if (close(pipe2[0]) == -1) {
            error_exit("child2 close pipe2 read end after dup2 failed\n");
        }

        int file_fd = open(filename2, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (file_fd == -1) {
            error_exit("child2 open file2 failed\n");
        }

        if (dup2(file_fd, STDOUT_FILENO) == -1) {
            close(file_fd);
            error_exit("child2 dup2 file2 failed\n");
        }
        if (close(file_fd) == -1) {
            error_exit("child2 close file2 failed\n");
        }

        execl("./child", "child", NULL);
        error_exit("child2 execl child failed\n");
    }

    if (close(pipe1[0]) == -1) {
        write_str(STDERR_FILENO, "parent close pipe1 read end failed\n");
    }
    if (close(pipe2[0]) == -1) {
        write_str(STDERR_FILENO, "parent close pipe2 read end failed\n");
    }

    write_str(STDOUT_FILENO, "Введите строку (Ctrl+D для завершения):\n");

    while (1) {
        char *line = read_line();
        if (line == NULL) {
            break;
        }

        if (line[0] == '\0') {
            free(line);
            break;
        }

        int r = rand() % 10;
        if (r < 8) {
            write_str(pipe1[1], line);
            write_str(pipe1[1], "\n");
        } else {
            write_str(pipe2[1], line);
            write_str(pipe2[1], "\n");
        }
        free(line);
    }

    if (close(pipe1[1]) == -1) {
        write_str(STDERR_FILENO, "parent close pipe1 write end failed\n");
    }
    if (close(pipe2[1]) == -1) {
        write_str(STDERR_FILENO, "parent close pipe2 write end failed\n");
    }

    int status;
    if (waitpid(pid1, &status, 0) == -1) {
        write_str(STDERR_FILENO, "waitpid for child1 failed\n");
    }
    if (waitpid(pid2, &status, 0) == -1) {
        write_str(STDERR_FILENO, "waitpid for child2 failed\n");
    }

    free(filename1);
    free(filename2);

    return 0;
}
