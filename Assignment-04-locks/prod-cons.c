#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define BUFFER_SIZE 100

unsigned int buffer[BUFFER_SIZE];
int count = 0;
int in = 0;
int out = 0;
int terminate = 0;

pthread_mutex_t mutex;
pthread_cond_t not_full;
pthread_cond_t not_empty;

void *producer(void *arg) {
    FILE *input_file = fopen("input-part1.txt", "r");
    if (input_file == NULL) {
        perror("Failed to open input file");
        exit(1);
    }

    unsigned int item;
    while (fscanf(input_file, "%u", &item) != EOF) {
        pthread_mutex_lock(&mutex);

        // If end signal (0) is read, terminate producer
        if (item == 0) {
            terminate = 1;
            pthread_cond_signal(&not_empty); // Wake up consumer if it's waiting
            pthread_mutex_unlock(&mutex);
            fclose(input_file);
            return NULL;
        }

        while (count == BUFFER_SIZE) {
            pthread_cond_wait(&not_full, &mutex);
        }

        buffer[in] = item;
        in = (in + 1) % BUFFER_SIZE;
        count++;

        pthread_cond_signal(&not_empty);
        pthread_mutex_unlock(&mutex);
    }

    pthread_mutex_lock(&mutex);
    terminate = 1;
    pthread_cond_signal(&not_empty); // Wake up consumer if it's waiting
    pthread_mutex_unlock(&mutex);

    fclose(input_file);
    return NULL;
}

void *consumer(void *arg) {
    FILE *output_file = fopen("output-part1.txt", "w");
    if (output_file == NULL) {
        perror("Failed to open output file");
        exit(1);
    }

    while (1) {
        pthread_mutex_lock(&mutex);

        // Exit if buffer is empty and terminate is set
        if (count == 0 && terminate) {
            pthread_mutex_unlock(&mutex);
            fclose(output_file);
            return NULL;
        }

        while (count == 0) {
            pthread_cond_wait(&not_empty, &mutex);

            // Check for terminate condition after waking up
            if (count == 0 && terminate) {
                pthread_mutex_unlock(&mutex);
                fclose(output_file);
                return NULL;
            }
        }

        unsigned int item = buffer[out];
        out = (out + 1) % BUFFER_SIZE;
        count--;

        // Write to output file
        fprintf(output_file, "Consumed:[%u],Buffer-State:[", item);
        
        // Print all elements in the buffer in their current order
        for (int i = 0; i < count; i++) {
            int index = (out + i) % BUFFER_SIZE;
            fprintf(output_file, "%u", buffer[index]);
            if (i < count - 1) {
                fprintf(output_file, ",");
            }
        }
        
        fprintf(output_file, "]\n");

        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&mutex);
    }

    fclose(output_file);
    return NULL;
}

int main() {
    pthread_t prod_thread, cons_thread;

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&not_full, NULL);
    pthread_cond_init(&not_empty, NULL);

    pthread_create(&prod_thread, NULL, producer, NULL);
    pthread_create(&cons_thread, NULL, consumer, NULL);

    pthread_join(prod_thread, NULL);
    pthread_join(cons_thread, NULL);

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&not_full);
    pthread_cond_destroy(&not_empty);

    return 0;
}