#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <mutex>
#include <fstream>
#include <condition_variable>

#define NUM_READERS 3
#define FILE_NAME "shared.txt"
#define BUFFER_SIZE 1024

std::mutex mtx;
std::condition_variable cv;
int readers = 0;
int writers = 0;
int waitingWriters = 0;

void* readerThread(void* arg) {
    int id = *((int*)arg);

    while (true) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [] { return writers == 0 && waitingWriters == 0; });
        readers++;

        lock.unlock();

        int fd = open(FILE_NAME, O_RDONLY);
        if (fd == -1) {
            perror("Reader: File open failed");
            lock.lock();
            readers--;
            cv.notify_all();
            lock.unlock();
            sleep(1);
            continue;
        }

        struct flock lockStruct;
        lockStruct.l_type = F_RDLCK;
        lockStruct.l_whence = SEEK_SET;
        lockStruct.l_start = 0;
        lockStruct.l_len = 0;
        lockStruct.l_pid = getpid();

        fcntl(fd, F_SETLKW, &lockStruct);

        char buffer[BUFFER_SIZE];
        ssize_t bytesRead = read(fd, buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            std::cout << "[Reader " << id << "] Read:\n" << buffer << "\n";
        }

        lockStruct.l_type = F_UNLCK;
        fcntl(fd, F_SETLK, &lockStruct);
        close(fd);

        lock.lock();
        readers--;
        if (readers == 0) cv.notify_all();
        lock.unlock();

        sleep(3);
    }

    pthread_exit(NULL);
}

int main() {
    pthread_t readerThreads[NUM_READERS];
    int ids[NUM_READERS];

    for (int i = 0; i < NUM_READERS; ++i) {
        ids[i] = i + 1;
        pthread_create(&readerThreads[i], NULL, readerThread, &ids[i]);
    }

    for (int i = 0; i < NUM_READERS; ++i) {
        pthread_join(readerThreads[i], NULL);
    }

    return 0;
}
