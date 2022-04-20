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

int main(int argc, char* argv[]){
    if(argc != 3){
        printf("Wrong number of arguments!\n");
        exit(EXIT_FAILURE);
    }
    int arg2 = atoi(argv[2]);
    if(arg2 == 0){
        printf("Number of threads should be equal or greater than 1\n");
        exit(EXIT_FAILURE);
    }
    if((sysconf(_SC_NPROCESSORS_ONLN) << 1) < arg2){
        printf("Number of threads to use is greater than number available. Program will use the maximum possible number (%ld).\n", sysconf(_SC_NPROCESSORS_ONLN) * 2);
    }
    number_of_threads = ((sysconf(_SC_NPROCESSORS_ONLN) << 1) < arg2)? sysconf(_SC_NPROCESSORS_ONLN) << 1 : arg2;
    clock_t start_time, end_time;
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
    quan_line = (size - 2 * len_line) / (arg2 * len_line);
    bit_count >>= 5;
    bit_count <<= 2;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_t* threads = (pthread_t *)malloc(number_of_threads*sizeof(pthread_t));
    check_mem(threads);
    struct arg* arr_of_arg = (struct arg *)malloc(number_of_threads*sizeof(struct arg));
    check_mem(arr_of_arg);
    start_time = clock();
    for(size_t i = 0; i < number_of_threads; ++i){
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
    end_time = clock();
    free(threads);
    free(arr_of_arg);
    free(output_buf);
    printf("The filter was used successfully in %.4f seconds!\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);
    exit(EXIT_SUCCESS);
}