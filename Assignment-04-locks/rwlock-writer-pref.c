#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdlib.h>

typedef struct _rwlock_t {
    sem_t write_lock;         // Binary semaphore for exclusive writer access
    sem_t reader_count_lock;  // Semaphore to protect reader count changes
    int readers;              // Tracks the number of active readers
    int writers_waiting;      // Tracks the number of waiting writers
} rwlock_t;

rwlock_t rwlock;

void rwlock_init(rwlock_t *rw) {
    rw->readers = 0;
    rw->writers_waiting = 0;
    sem_init(&rw->write_lock, 0, 1);
    sem_init(&rw->reader_count_lock, 0, 1);
}

void rwlock_acquire_readlock(rwlock_t *rw) {
    sem_wait(&rw->reader_count_lock);
    
    // Wait until there are no waiting or active writers
    while (rw->writers_waiting > 0) {
        sem_post(&rw->reader_count_lock);
        usleep(100); // Give CPU time to other threads
        sem_wait(&rw->reader_count_lock);
    }

    // If this is the first reader, acquire the write lock
    if (rw->readers == 0) {
        sem_wait(&rw->write_lock);
    }
    rw->readers++;
    
    sem_post(&rw->reader_count_lock);
}

void rwlock_release_readlock(rwlock_t *rw) {
    sem_wait(&rw->reader_count_lock);
    
    rw->readers--;
    if (rw->readers == 0) {
        sem_post(&rw->write_lock); // Last reader releases the writer lock
    }

    sem_post(&rw->reader_count_lock);
}

void rwlock_acquire_writelock(rwlock_t *rw) {
    sem_wait(&rw->reader_count_lock);
    rw->writers_waiting++;
    sem_post(&rw->reader_count_lock);

    // Only one writer can hold this lock, ensuring exclusivity
    sem_wait(&rw->write_lock);

    sem_wait(&rw->reader_count_lock);
    rw->writers_waiting--;
    sem_post(&rw->reader_count_lock);
}

void rwlock_release_writelock(rwlock_t *rw) {
    sem_post(&rw->write_lock);
}

// Reader function
void *reader(void *arg) {
    rwlock_acquire_readlock(&rwlock);

    FILE *output = fopen("output-writer-pref.txt", "a");
    if (output) {
        fprintf(output, "Reading,Number-of-readers-present:[%d]\n", rwlock.readers);
        fclose(output);
    }

    // Simulate reading
    FILE *shared = fopen("shared-file.txt", "r");
    if (shared) {
        char line[256];
        while (fgets(line, sizeof(line), shared)) {
            // Process content if needed
        }
        fclose(shared);
    }

    rwlock_release_readlock(&rwlock);
    return NULL;
}

// Writer function
void *writer(void *arg) {
    rwlock_acquire_writelock(&rwlock);

    FILE *output = fopen("output-writer-pref.txt", "a");
    if (output) {
        fprintf(output, "Writing,Number-of-readers-present:[%d]\n", rwlock.readers);
        fclose(output);
    }

    // Write to shared file
    FILE *shared = fopen("shared-file.txt", "a");
    if (shared) {
        fprintf(shared, "Hello world!\n");
        fclose(shared);
    }

    rwlock_release_writelock(&rwlock);
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <num_readers> <num_writers>\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);
    int m = atoi(argv[2]);
    pthread_t readers[n], writers[m];

    // Initialize the reader-writer lock
    rwlock_init(&rwlock);

    // Create reader and writer threads
    for (int i = 0; i < n; i++) pthread_create(&readers[i], NULL, reader, NULL);
    for (int i = 0; i < m; i++) pthread_create(&writers[i], NULL, writer, NULL);

    // Wait for all threads to complete
    for (int i = 0; i < n; i++) pthread_join(readers[i], NULL);
    for (int i = 0; i < m; i++) pthread_join(writers[i], NULL);

    return 0;
}