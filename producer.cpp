#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <vector>
#include <mutex>
#include <fstream>
#include <condition_variable>

#define NUM_WRITERS 2
#define FILE_NAME "shared.txt"

std::mutex mtx;
std::condition_variable cv;
int readers = 0;
int writers = 0;
int waitingWriters = 0;

void* writerThread(void* arg) {
    int id = *((int*)arg);

    for (int i = 0; i < 5; ++i) {
        std::unique_lock<std::mutex> lock(mtx);
        waitingWriters++;
        cv.wait(lock, [] { return readers == 0 && writers == 0; });
        waitingWriters--;
        writers = 1; // Only one writer at a time

        lock.unlock();

        // Begin write with file lock
        int fd = open(FILE_NAME, O_WRONLY | O_APPEND | O_CREAT, 0666);
        if (fd == -1) {
            perror("Writer: File open failed");
            continue;
        }

        struct flock lockStruct;
        lockStruct.l_type = F_WRLCK;
        lockStruct.l_whence = SEEK_SET;
        lockStruct.l_start = 0;
        lockStruct.l_len = 0;
        lockStruct.l_pid = getpid();

        fcntl(fd, F_SETLKW, &lockStruct); // Wait if locked

        std::string msg = "Writer " + std::to_string(id) + " wrote line " + std::to_string(i) + "\n";
        write(fd, msg.c_str(), msg.size());
        std::cout << "[Producer] " << msg;

        lockStruct.l_type = F_UNLCK;
        fcntl(fd, F_SETLK, &lockStruct);
        close(fd);

        lock.lock();
        writers = 0;
        cv.notify_all(); // Wake up readers and writers
        lock.unlock();

        sleep(2);
    }

    pthread_exit(NULL);
}

int main() {
    pthread_t writerThreads[NUM_WRITERS];
    std::vector<int> ids(NUM_WRITERS);

    for (int i = 0; i < NUM_WRITERS; ++i) {
        ids[i] = i + 1;
        pthread_create(&writerThreads[i], NULL, writerThread, &ids[i]);
    }

    for (int i = 0; i < NUM_WRITERS; ++i) {
        pthread_join(writerThreads[i], NULL);
    }

    std::cout << "All writers done.\n";
    return 0;
}
