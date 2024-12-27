#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>


#define MAX_THREADS 12
#define EOF -1

struct timespec start, end;

typedef struct {
    int thread_id; 
    int num_threads; 
    int num_arrays;
    int array_length;
    int* arrays;
    int* result;
} ThreadData;

int str_to_int(char* str, int* out) {
    int res = 0;
    int sign = 1;
    int i = 0;
    if (str[0] == '-') {
        sign = -1;
        i++;
    }
    for(; str[i] != '\0'; i++) {
        if(str[i] < '0' || str[i] > '9') {
            return -1;
        }
        res = res * 10 + (str[i] - '0');
    }
    *out = res * sign;
    return 0;
}

int int_to_str(int num, char* str) {
    int i = 0;
    int is_negative = 0;
    if(num == 0){
        str[i++] = '0';
        str[i] = '\0';
        return i;
    }
    if(num < 0){
        is_negative = 1;
        num = -num;
    }
    while(num > 0){
        str[i++] = (num % 10) + '0';
        num /= 10;
    }
    if(is_negative){
        str[i++] = '-';
    }
    for(int j = 0; j < i / 2; j++) {
        char tmp = str[j];
        str[j] = str[i - j -1];
        str[i - j -1] = tmp;
    }
    str[i] = '\0';
    return i;
}

int read_int(int fd, int* value) {
    char buffer[20];
    int idx = 0;
    char c;
    while(read(fd, &c, 1) == 1 && (c == ' ' || c == '\n' || c == '\t'));
    if(c == EOF || c == 0){
        return -1;
    }
    if (c == '-') {
        buffer[idx++] = c;
    } else if (c >= '0' && c <= '9') {
        buffer[idx++] = c;
    } else {
        return -1;
    }
    while(read(fd, &c, 1) == 1 && (c >= '0' && c <= '9')) {
        buffer[idx++] = c;
        if(idx >= 19) break;
    }
    buffer[idx] = '\0';
    if(!(c >= '0' && c <= '9')){
        lseek(fd, -1, SEEK_CUR);
    }
    if(str_to_int(buffer, value) != 0){
        return -1;
    }
    return 0;
}

void* sum_arrays(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    int start = (data->thread_id * data->array_length) / data->num_threads;
    int end = ((data->thread_id + 1) * data->array_length) / data->num_threads;

    for(int i = start; i < end; i++) {
        int sum = 0;
        for(int j = 0; j < data->num_arrays; j++) {
            sum += data->arrays[j * data->array_length + i];
        }
        data->result[i] = sum;
    }

    return NULL;
}

int main(int argc, char* argv[]) {
    if(argc != 4) {
        char* msg = "Usage: ./sum_arrays <input_file> <num_threads> <output_file>\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        return EXIT_FAILURE;
    }

    int N;
    if(str_to_int(argv[2], &N) != 0 || N <= 0 || N > MAX_THREADS) {
        char* msg = "Invalid number of threads. Must be between 1 and 8.\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        return EXIT_FAILURE;
    }

    int fd_in = open(argv[1], O_RDONLY);
    if (fd_in == -1) {
        char* msg = "Failed to open input file.\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        return EXIT_FAILURE;
    }

    int num_arrays, array_length;
    if(read_int(fd_in, &num_arrays) == -1 || read_int(fd_in, &array_length) == -1) {
        char* msg = "Failed to read array counts or lengths.\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        close(fd_in);
        return EXIT_FAILURE;
    }

    int* arrays = (int*)malloc(num_arrays * array_length * sizeof(int));
    if(!arrays) {
        char* msg = "Memory allocation failed for arrays.\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        close(fd_in);
        return EXIT_FAILURE;
    }

    for(int i = 0; i < num_arrays; i++) {
        for(int j = 0; j < array_length; j++) {
            if(read_int(fd_in, &arrays[i * array_length + j]) == -1) {
                char* msg = "Failed to read array elements.\n";
                write(STDOUT_FILENO, msg, strlen(msg));
                free(arrays);
                close(fd_in);
                return EXIT_FAILURE;
            }
        }
    }

    close(fd_in);

    int* result = (int*)malloc(array_length * sizeof(int));
    if(!result) {
        char* msg = "Memory allocation failed for result array.\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        free(arrays);
        return EXIT_FAILURE;
    }
    memset(result, 0, array_length * sizeof(int));

    pthread_t* threads = (pthread_t*)malloc(N * sizeof(pthread_t));
    ThreadData* threads_data = (ThreadData*)malloc(N * sizeof(ThreadData));
    if(!threads || !threads_data){
        char* msg = "Memory allocation failed for threads.\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        free(arrays);
        free(result);
        free(threads);
        free(threads_data);
        return EXIT_FAILURE;
    }

    for (int i = 0; i < N; i++) {
        threads_data[i].thread_id = i;
        threads_data[i].num_threads = N;
        threads_data[i].num_arrays = num_arrays;
        threads_data[i].array_length = array_length;
        threads_data[i].arrays = arrays;
        threads_data[i].result = result;
    }


    // Создание потоков
    for (int i = 0; i < N; i++) {
        if(pthread_create(&threads[i], NULL, &sum_arrays, &threads_data[i]) != 0){
            char* msg = "Failed to create thread.\n";
            write(STDOUT_FILENO, msg, strlen(msg));
            free(arrays);
            free(result);
            free(threads);
            free(threads_data);
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < N; i++) {
        pthread_join(threads[i], NULL);
    }


    int fd_out = open(argv[3], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(fd_out == -1){
        char* msg = "Failed to open output file.\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        free(arrays);
        free(result);
        free(threads);
        free(threads_data);
        return EXIT_FAILURE;
    }

    char buffer[20];
    int len = 0;

    // Запись результирующего массива в файл
    for(int i = 0; i < array_length; i++) {
        len = int_to_str(result[i], buffer);
        write(fd_out, buffer, len);
        if(i != array_length -1){
            write(fd_out, " ", 1);
        }
    }
    write(fd_out, "\n", 1);

    close(fd_out);

    free(arrays);
    free(result);
    free(threads);
    free(threads_data);

    return EXIT_SUCCESS;
}
