#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdlib.h>

typedef struct _rwlock_t {
    sem_t write_lock;         // Binary semaphore for writers
    sem_t reader_lock;        // Semaphore to control reader count access
    int readers;              // Tracks the number of active readers
} rwlock_t;

rwlock_t rwlock;

void rwlock_init(rwlock_t *rw) {
    rw->readers = 0;
    sem_init(&rw->write_lock, 0, 1);
    sem_init(&rw->reader_lock, 0, 1); // Controls access to reader count
}

void rwlock_acquire_readlock(rwlock_t *rw) {
    sem_wait(&rw->reader_lock); // Lock access to reader count

    if (rw->readers == 0) {
        // First reader locks the writer
        sem_wait(&rw->write_lock);
    }
    rw->readers++; // Increment the number of active readers

    sem_post(&rw->reader_lock); // Unlock access to reader count
}

void rwlock_release_readlock(rwlock_t *rw) {
    sem_wait(&rw->reader_lock); // Lock access to reader count
    rw->readers--; // Decrement the number of active readers

    if (rw->readers == 0) {
        // Last reader releases the writer lock
        sem_post(&rw->write_lock);
    }
    sem_post(&rw->reader_lock); // Unlock access to reader count
}

void rwlock_acquire_writelock(rwlock_t *rw) {
    sem_wait(&rw->write_lock); // Writer acquires the lock when no readers are active
}

void rwlock_release_writelock(rwlock_t *rw) {
    sem_post(&rw->write_lock); // Release the writer lock
}

// Reader function
void *reader(void *arg) {
    rwlock_acquire_readlock(&rwlock); 

    // Log reading action
    FILE *output = fopen("output-reader-pref.txt", "a");
    if (output) {
        fprintf(output, "Reading,Number-of-readers-present:[%d]\n", rwlock.readers);
        fclose(output);
    }

    // Simulate reading from shared file
    FILE *shared = fopen("shared-file.txt", "r");
    if (shared) {
        char line[256];
        while (fgets(line, sizeof(line), shared)) {
            // Here you can process the content if needed
        }
        fclose(shared);
    }

    rwlock_release_readlock(&rwlock);
    return NULL;
}

// Writer function
void *writer(void *arg) {
    rwlock_acquire_writelock(&rwlock);

    // Log writing action
    FILE *output = fopen("output-reader-pref.txt", "a");
    if (output) {
        fprintf(output, "Writing,Number-of-readers-present:[%d]\n", rwlock.readers);
        fclose(output);
    }

    // Write to shared file
    FILE *shared = fopen("shared-file.txt", "a");
    if (shared) {
        fprintf(shared, "Hello world!\n"); // Writing content to shared file
        fclose(shared);
    }

    rwlock_release_writelock(&rwlock);
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 3) return 1;
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