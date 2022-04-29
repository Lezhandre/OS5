#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>

long number_of_threads;

struct arg {
    char * filename;
    ssize_t off, end, len_line;
    ushort bit_count;
};

void check_mem(void * ptr){
    if (ptr == NULL) {
        perror("Memory problem");
        exit(EXIT_FAILURE);
    }
}

char* output_buf, * input_buf;

// функция обработки изображения
void *bump(void* arg) {
    struct arg a = *((struct arg *)arg);
    ssize_t end = a.end, len_line = a.len_line;
    ushort bit_count = a.bit_count;
    char* curr_line = input_buf + a.off;
    char* prev_line = curr_line - len_line;
    char* next_line = curr_line + len_line;
    for (ssize_t i = a.off, j, k; i < end; i += len_line) {
        // цикл в котором происходит магия
        for (j = bit_count; j < len_line - bit_count; ++j) {
            double wb = 0;
            for (k = j + bit_count - 1; j < k; ++j) {
                int Gx = -(prev_line[j-bit_count] + 2 * prev_line[j] + prev_line[j+bit_count]) + (next_line[j-bit_count] + 2 * next_line[j] + next_line[j+bit_count]);
                int Gy = -(next_line[j-bit_count] + 2 * curr_line[j-bit_count] + prev_line[j-bit_count]) + (next_line[j+bit_count] + 2 * curr_line[j+bit_count] + prev_line[j+bit_count]);
                wb += sqrt((double)(Gx * Gx + Gy * Gy));
            }
            wb /= 9;
            for (j = k - (bit_count - 1); j < k; ++j)
                output_buf[i + j] = wb;
            output_buf[i + j] = 255;
        }
        prev_line += len_line;
        curr_line += len_line;
        next_line += len_line;
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        printf("Right input: ./main \"picture.bmp\" number_of_threads");
        exit(EXIT_SUCCESS);
    }
    if (argc != 3) {
        printf("Wrong number of arguments!\n");
        exit(EXIT_FAILURE);
    }
    int arg2 = atoi(argv[2]);
    if (arg2 == 0) {
        printf("Number of threads should be equal or greater than 1\n");
        exit(EXIT_FAILURE);
    }
    number_of_threads = arg2;
    struct timeval begin, end;
    // предобработка данных о файле формата bmp
    ushort bit_count;
    ssize_t quan_line, len_line, off;
    uint size;
    int photo_desc = open(argv[1], O_RDWR);
    if (photo_desc < 0) {
        perror("Photo");
        exit(EXIT_FAILURE);
    }
    uint pixels_adress, width, height;
    lseek(photo_desc, 10, SEEK_SET);
    read(photo_desc, &pixels_adress, sizeof(int)); // получение адреса где начинается массив пикселей
    lseek(photo_desc, 18, SEEK_SET);
    read(photo_desc, &width, sizeof(int)); // ширина изображения
    lseek(photo_desc, 28, SEEK_SET);
    read(photo_desc, &bit_count, sizeof(short)); // глубина изображения
    lseek(photo_desc, 34, SEEK_SET);
    read(photo_desc, &size, sizeof(int)); // размер изображения
    len_line = (((width * bit_count + 31) >> 5) << 2); // количество пикселей в горизонтальном ряду
    off = pixels_adress;
    input_buf = (char*)malloc(size);
    check_mem(input_buf);
    lseek(photo_desc, off, SEEK_SET);
    read(photo_desc, input_buf, size);
    output_buf = (char*)malloc(size);
    check_mem(output_buf);
    quan_line = (size - 2 * len_line + arg2 * len_line - 1) / (arg2 * len_line);
    bit_count >>= 5;
    bit_count <<= 2;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_t* threads = (pthread_t *)malloc(number_of_threads*sizeof(pthread_t));
    check_mem(threads);
    struct arg* arr_of_arg = (struct arg *)malloc(number_of_threads*sizeof(struct arg));
    check_mem(arr_of_arg);
    gettimeofday(&begin, 0);
    for (size_t i = 0; i < number_of_threads; ++i) {
        arr_of_arg[i].off = (i * quan_line + 1) * len_line;
        arr_of_arg[i].bit_count = bit_count;
        arr_of_arg[i].end = ((i + 1) * quan_line + 1) * len_line;
        arr_of_arg[i].len_line = len_line;
        threads[i] = 0;
        if (arr_of_arg[i].end > size - len_line) arr_of_arg[i].end = size - len_line;
        pthread_create(threads + i, &attr, &bump, arr_of_arg + i);
    }
    for (size_t i = 0; i < number_of_threads; ++i) pthread_join(threads[i], NULL);
    lseek(photo_desc, off, SEEK_SET);
    write(photo_desc, output_buf, size);
    close(photo_desc);
    gettimeofday(&end, 0);
    long seconds = end.tv_sec - begin.tv_sec;
    long microseconds = end.tv_usec - begin.tv_usec;
    double elapsed = seconds + microseconds * 1e-6;
    free(threads);
    free(arr_of_arg);
    free(output_buf);
    free(input_buf);
    printf("The filter was used successfully in %1.4f seconds!\n", elapsed);
    exit(EXIT_SUCCESS);
}