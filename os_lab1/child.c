#include <unistd.h>
#include <stdlib.h>
#include <string.h>

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
    size_t size = 1024;
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
            size += 1024;
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

void reverse_string(char *str) {
    size_t len = 0;
    while (str[len] != '\0') len++;
    for (size_t i = 0; i < len / 2; i++) {
        char tmp = str[i];
        str[i] = str[len - 1 - i];
        str[len - 1 - i] = tmp;
    }
}

int main() {
    while (1) {
        char *line = read_line();
        if (line == NULL) {
            break;
        }

        if (line[0] == '\0') {
            free(line);
            break;
        }

        reverse_string(line);

        write_str(STDOUT_FILENO, line);
        write_str(STDOUT_FILENO, "\n");

        free(line);
    }
    return 0;
}
