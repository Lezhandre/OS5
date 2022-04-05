#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

long number_of_processors;

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

// функция обработки изображения
void *bump(void* arg) {
    struct arg a = *((struct arg *)arg);
    int photo_desc = open(a.filename, O_RDWR);
    ssize_t end = a.end, len_line = a.len_line;
    ushort bit_count = a.bit_count;
    char* buf = (char *)malloc(len_line);
    check_mem(buf);
    for (ssize_t i = a.off, j; i < end; i += len_line * number_of_processors) {
        lseek(photo_desc, i, SEEK_SET);
        int check;
        check = read(photo_desc, buf, len_line);
        if (check < 0) {
            perror("read");
            close(photo_desc);
            return NULL;
        }
        // цикл в котором происходит магия
        for (j = 0; j < check; ++j) {
            if (j % bit_count == bit_count - 1) continue;
            buf[j] ^= 255;
        }
        lseek(photo_desc, i, SEEK_SET);
        check = write(photo_desc, buf, check);
        if (check < 0) {
            perror("write");
            close(photo_desc);
            return NULL;
        }
    }
    free(buf);
    close(photo_desc);
    return NULL;
}

int main(int argc, char* argv[]){
    if(argc != 2){
        printf("Wrong number of arguments!\n");
        exit(EXIT_FAILURE);
    }
    number_of_processors = (sysconf(_SC_NPROCESSORS_ONLN) << 1);
    // предобработка данных о файле формата bmp
    ushort bit_count;
    ssize_t end, len_line, off;
    {
        int photo_desc = open(argv[1], O_RDWR);
        if (photo_desc < 0) {
            perror("Photo");
            exit(EXIT_FAILURE);
        }
        uint pixels_adress, width, height, size;
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
        end = off + size;
        bit_count >>= 5;
        bit_count <<= 2;
        close(photo_desc);
    }
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_t* threads = (pthread_t *)malloc(number_of_processors*sizeof(pthread_t));
    check_mem(threads);
    struct arg* arr_of_arg = (struct arg *)malloc(number_of_processors*sizeof(struct arg));
    check_mem(arr_of_arg);
    for(size_t i = 0; i < number_of_processors; ++i, off += len_line){
        arr_of_arg[i].filename = argv[1];
        arr_of_arg[i].off = off;
        arr_of_arg[i].bit_count = bit_count;
        arr_of_arg[i].end = end;
        arr_of_arg[i].len_line = len_line;
        threads[i] = 0;
        pthread_create(threads + i, &attr, &bump, arr_of_arg + i);
    }
    for(size_t i = 0; i < number_of_processors; ++i) pthread_join(threads[i], NULL);
    free(threads);
    free(arr_of_arg);
    exit(EXIT_SUCCESS);
}