#pragma once

//Can include any other headers as needed
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <stdbool.h>
#include <fcntl.h>
#include <string.h>

#define MAX_COMMAND_ARGS 100
#define MAX_COMMAND_LENGTH 256
#define MAX_PROCESSES 100
#define MAX_QUEUE_SIZE 100

uint64_t arrival_time = 0;
uint64_t firstProcessstart_time;
uint64_t lastBoostTime;

typedef struct {
    char *command;               // Command to be scheduled
    bool finished;              // If the process is finished safely
    bool error;                 // If an error occurs during execution
    uint64_t start_time;        // Start time of the process in milliseconds
    uint64_t completion_time;   // Completion time of the process in milliseconds
    uint64_t turnaround_time;   // Turnaround time of the process in milliseconds
    uint64_t waiting_time;      // Waiting time of the process in milliseconds
    uint64_t response_time;     // Response time of the process in milliseconds
    bool started;               // If the process has started
    int process_id;             // Process ID of the process
    //uint64_t remaining_time;    // Remaining time for RoundRobin
    uint64_t arrival_time;
    uint64_t burst_time;
    pid_t pid;
    int priority;
    int index;
} Process;

//Queues for MLFQ
Process* queue0_MLFQ[MAX_QUEUE_SIZE];
Process* queue1_MLFQ[MAX_QUEUE_SIZE];
Process* queue2_MLFQ[MAX_QUEUE_SIZE];
int queueSize0 = 0;
int queueSize1 = 0;
int queueSize2 = 0;



// Function prototypes
void FCFS(Process p[], int n);
void RoundRobin(Process p[], int n, int quantum);
void MultiLevelFeedbackQueue(Process p[], int n, int quantum0, int quantum1, int quantum2, int boostTime);

//Generic Function Prototypes
uint64_t time_diff_ms(struct timeval start, struct timeval end);
void write_csv(Process p[], int n, const char *scheduler_type);
uint64_t get_current_time_ms(void);

// Functions for FCFS
void execute_command_FCFS(Process *p);

// Functions for RR
void execute_command_RR(Process *proc, int quantum, uint64_t start_time);

// Functions for MLFQ
void add_to_queue_MLFQ(Process* p);
Process* pop_from_queue_MLFQ(int priority);
int is_empty_MLFQ(int priority);
void boost_queues();
void execute_process_MLFQ(Process *p, uint64_t quantum_end_time);


// Generic Function Definitions
uint64_t time_diff_ms(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
}

uint64_t get_current_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void write_csv(Process p[], int n, const char *scheduler_type) {
    char filename[100];
    snprintf(filename, sizeof(filename), "result_offline_%s.csv", scheduler_type);

    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        perror("Failed to open file for writing");
        return;
    }

    fprintf(file, "Command,Finished,Error,Burst Time (ms),Turnaround Time (ms),Waiting Time (ms),Response Time (ms)\n");

    for (int i = 0; i < n; ++i) {
        uint64_t burst_time = p[i].completion_time - p[i].start_time; // Burst time in milliseconds
        fprintf(file, "%s,%s,%s,%llu,%llu,%llu,%llu\n",
                p[i].command,
                p[i].error ? "No" : "Yes",
                p[i].error ? "Yes" : "No",
                burst_time,
                p[i].turnaround_time,
                p[i].waiting_time,
                p[i].response_time);
    }

    fclose(file);
}

//Functions for FCFS
void execute_command_FCFS(Process *p) {
    pid_t pid = fork();
    if (pid == 0) {
        // Chillu process
        execlp("/bin/sh", "sh", "-c", p->command, (char *)NULL);
        perror("execlp");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Parent process
        p->process_id = pid;
    } else {
        perror("fork");
    }
}

//Functions for RR
void execute_command_RR(Process *proc, int quantum, uint64_t start_time) {
    if (!proc->started) {
        proc->started = 1;
        proc->start_time = get_current_time_ms() - start_time;
        proc->response_time = proc->start_time - proc->arrival_time;

        // Tokenize the command
        char command_copy[MAX_COMMAND_LENGTH];
        strncpy(command_copy, proc->command, MAX_COMMAND_LENGTH - 1);
        command_copy[MAX_COMMAND_LENGTH - 1] = '\0';

        char *args[MAX_COMMAND_ARGS + 1];
        memset(args, 0, sizeof(args));

        char *token = strtok(command_copy, " ");
        int i = 0;
        while (token != NULL && i < MAX_COMMAND_ARGS) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;

        if ((proc->pid = fork()) == 0) {
            execvp(args[0], args);
            perror("execvp failed");
            exit(EXIT_FAILURE);
        } else if (proc->pid < 0) {
            perror("fork failed");
            proc->error = 1;
            exit(EXIT_FAILURE);
        }
    } else {
        if (kill(proc->pid, SIGCONT) == -1) {
            perror("Failed to send SIGCONT");
            proc->error = 1;
            exit(EXIT_FAILURE);
        } else {
            //printf("Resuming process PID %d\n", proc->pid);
        }
    }

    uint64_t context_start = get_current_time_ms();
    usleep(quantum * 1000); // Let the process run for the quantum time
    uint64_t context_end = get_current_time_ms();

    // Stop the process after the quantum
    if (kill(proc->pid, SIGSTOP) == -1) {
        perror("Failed to send SIGSTOP");
        proc->error = 1;
        exit(EXIT_FAILURE);
    } else {
        //printf("Stopping process PID %d\n", proc->pid);
    }

    // Update burst time
    proc->burst_time += (context_end - context_start);

    // Print debug information
    printf("%s | %llu | %llu\n",
           proc->command, context_start - start_time, context_end - start_time);
}

//Functions for MLFQ
//enqueue operation
void add_to_queue_MLFQ(Process* p) {
    if (p->priority == 0 && queueSize0 < MAX_QUEUE_SIZE) {
        queue0_MLFQ[queueSize0++] = p;
    } else if (p->priority == 1 && queueSize1 < MAX_QUEUE_SIZE) {
        queue1_MLFQ[queueSize1++] = p;
    } else if (p->priority == 2 && queueSize2 < MAX_QUEUE_SIZE) {
        queue2_MLFQ[queueSize2++] = p;
    }
}

//dequeue operation
Process* pop_from_queue_MLFQ(int priority) {
    Process* p = NULL;
    if (priority == 0 && queueSize0 > 0) {
        p = queue0_MLFQ[0];
        for (int i = 1; i < queueSize0; i++) {
            queue0_MLFQ[i - 1] = queue0_MLFQ[i];
        }
        queueSize0--;
    } else if (priority == 1 && queueSize1 > 0) {
        p = queue1_MLFQ[0];
        for (int i = 1; i < queueSize1; i++) {
            queue1_MLFQ[i - 1] = queue1_MLFQ[i];
        }
        queueSize1--;
    } else if (priority == 2 && queueSize2 > 0) {
        p = queue2_MLFQ[0];
        for (int i = 1; i < queueSize2; i++) {
            queue2_MLFQ[i - 1] = queue2_MLFQ[i];
        }
        queueSize2--;
    }
    return p;
}

//check operation
int is_empty_MLFQ(int priority) {
    if (priority == 0) return queueSize0 == 0;
    if (priority == 1) return queueSize1 == 0;
    if (priority == 2) return queueSize2 == 0;
    return 1;
}

//bosting all remainig processes to queue0_MLFQ
void boost_queues() {
    while (!is_empty_MLFQ(1)) {
        Process* p = pop_from_queue_MLFQ(1);
        p->priority = 0;
        //p->quantum = quantum0; // Reset to initial quantum for Queue 0
        add_to_queue_MLFQ(p);
    }
    while (!is_empty_MLFQ(2)) {
        Process* p = pop_from_queue_MLFQ(2);
        p->priority = 0;
        //p->quantum = quantum0; // Reset to initial quantum for Queue 0
        add_to_queue_MLFQ(p);
    }
}

//executing the unix-based command using fork and exec
void execute_process_MLFQ(Process *p, uint64_t quantum_end_time) {
    if (!p->started) {
        // Executing a process for the first time by forking
        p->started = 1;
        char *args[MAX_COMMAND_ARGS + 1];
        memset(args, 0, sizeof(args));

        char command_copy[MAX_COMMAND_LENGTH];
        strncpy(command_copy, p->command, MAX_COMMAND_LENGTH - 1);
        command_copy[MAX_COMMAND_LENGTH - 1] = '\0';

        // Tokenizing the command
        char *token = strtok(command_copy, " ");
        int i = 0;
        while (token != NULL && i < MAX_COMMAND_ARGS) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;

        pid_t pid = fork();
        if (pid == 0) {
            execvp(args[0], args);  // Running the command
            // If execvp fails
            perror("execvp failed");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            p->pid = pid;  // Setting pid
            p->start_time = get_current_time_ms() - firstProcessstart_time;
        } else {
            // If fork fails
            perror("fork failed");
            p->error = 1;
            return;
        }
    } else {
        if (kill(p->pid, SIGCONT) == -1) {  // SIGCONT if the process has been previously started
            p->error = 1;
            perror("SIGCONT error");
            return;
        }
    }

    int status;
    while (get_current_time_ms() < quantum_end_time) {
        if (waitpid(p->pid, &status, WNOHANG) != 0) {
            if (WIFEXITED(status)) {
                int exit_status = WEXITSTATUS(status);
                if (exit_status != 0) {
                    p->error = 1;
                }
                p->finished = 1;
            }
            break;
        }
        usleep(10000);
    }

    if (!p->finished) {
        // Stop it if the quantum is over and the process is not finished
        kill(p->pid, SIGSTOP);
    }
}



// FCFS Function
void FCFS(Process p[], int n) {
    struct timeval start_time_tv, end_time_tv;
    gettimeofday(&start_time_tv, NULL);
    uint64_t start_time_ms = start_time_tv.tv_sec * 1000 + start_time_tv.tv_usec / 1000;

    uint64_t current_time = start_time_ms;

    for (int i = 0; i < n; ++i) {
        p[i].start_time = current_time;

        execute_command_FCFS(&p[i]);

        // Wait for the process to complete
        int status;
        if (waitpid(p[i].process_id, &status, 0) == -1) {
            perror("waitpid");
            p[i].error = true;
        } else {
            gettimeofday(&end_time_tv, NULL);
            uint64_t end_time_ms = end_time_tv.tv_sec * 1000 + end_time_tv.tv_usec / 1000;
            p[i].completion_time = end_time_ms;

            uint64_t burst_time = p[i].completion_time - p[i].start_time;
            p[i].turnaround_time = p[i].completion_time - start_time_ms;
            p[i].waiting_time = p[i].start_time - start_time_ms;
            p[i].response_time = p[i].waiting_time; // Response time is same as waiting time for FCFS

            if (WIFEXITED(status)) {
                int exit_status = WEXITSTATUS(status);
                if (exit_status != 0) {
                    p[i].error = true; // Set error if exit status is not zero
                }
            } else {
                p[i].error = true; // Set error if process did not exit normally
            }

            // Print details after each context switch
            // printf("Command: %s\n", p[i].command);
            // printf("Start Time of the context: %llu ms\n", p[i].start_time - start_time_ms);
            // printf("End Time of the context: %llu ms\n", p[i].completion_time - start_time_ms);
            printf("%s | %llu | %llu\n", p[i].command, p[i].start_time - start_time_ms, p[i].completion_time - start_time_ms);
            
            // Update current time
            current_time = p[i].completion_time; // Set the start time of the next process to the end time of the current process
            p[i].finished = true;
        }
    }

    // Write results to CSV file
    write_csv(p, n, "FCFS");
}


//RR Function
void RoundRobin(Process processes[], int num_processes, int quantum) {
    int completed = 0;
    int i = 0;
    //uint64_t start_time = get_current_time_ms();

    // Initialize process data
    for (int j = 0; j < num_processes; j++) {
        processes[j].started = 0;
        processes[j].finished = 0;
        processes[j].arrival_time = 0;
        processes[j].burst_time = 0;
        processes[j].error = 0;
    }
    uint64_t start_time = get_current_time_ms();
    while (completed < num_processes) {
        Process *proc = &processes[i % num_processes];
        if (!proc->finished) {
            execute_command_RR(proc, quantum, start_time);

            // Wait for process to finish
            int status;
            pid_t result = waitpid(proc->pid, &status, WNOHANG);
            if (result == proc->pid) {
                int fixed = 0;
                if (WIFEXITED(status)) {
                    int exit_status = WEXITSTATUS(status);
                    if (exit_status != 0) {
                        fixed = 1;
                        proc->finished = 0;
                        proc->error = 1;
                    }
                } else {
                    fixed = 1;
                    proc->finished = 0;
                    proc->error = 1;
                }

                if (!fixed) proc->finished = 1;
                proc->completion_time = get_current_time_ms() - start_time;
                proc->waiting_time = proc->completion_time - proc->burst_time;
                proc->turnaround_time = proc->completion_time;
                proc->response_time = proc->start_time;
                completed++;
            } else if (result == -1) {
                perror("waitpid failed");
                proc->error = 1;
                proc->finished = 0;
            }
        }
        i++;
    }

    write_csv(processes, num_processes, "RR");
}


//MLFQ Function
void MultiLevelFeedbackQueue(Process processes[], int n, int quantum0, int quantum1, int quantum2, int boostTime) {
    for (int i = 0; i < n; i++) {
        // Allocate memory for each process and initialize its fiellus
        Process *p = (Process *)malloc(sizeof(Process));
        if (p == NULL) {
            perror("Memory allocation failed");
            exit(EXIT_FAILURE);
        }

        p->command = strdup(processes[i].command);  // Copy the command (to avoid pointer issues)
        p->index = i;
        p->arrival_time = arrival_time;              // Set arrival time (replace arrival_time with actual value)
        p->start_time = 0;                           // Start time not yet initialized
        p->completion_time = 0;                      // Completion time will be set after process finishes
        p->priority = 0;                            // Initialize with highest priority (queue0_MLFQ)
        p->started = 0;                             // Process hasn't started yet
        p->error = 0;                               // No error initially
        p->pid = 0;                                 // PID will be assigned after fork
        p->finished = 0;                            // Process is not finished yet
        p->turnaround_time = 0;                      // Turnaround time will be calculated later
        p->burst_time = 0;                           // Burst time is initially 0
        p->response_time = 0;                        // Response time will be calculated after process starts
        p->waiting_time = 0;                         // Waiting time will be updated dynamically

        // Add the initialized process to the MLFQ queue
        add_to_queue_MLFQ(p);
    }

    firstProcessstart_time = get_current_time_ms();       //when the MLFQ is initiated
    lastBoostTime = firstProcessstart_time;          //first boost is assumed at t=0
    //int firstProcessStarted = 0;

    

        while (!is_empty_MLFQ(0) || !is_empty_MLFQ(1) || !is_empty_MLFQ(2)) {
            if (!is_empty_MLFQ(0)) {
                //dealing with the queue0_MLFQ
                Process *p = pop_from_queue_MLFQ(0);
                //p->quantum = quantum0;
                // if (!firstProcessStarted) {
                //     firstProcessstart_time = get_current_time_ms();
                //     firstProcessStarted = 1;
                // }

                uint64_t start_time = get_current_time_ms() - firstProcessstart_time;
                //printf("queue0_MLFQ: Command: %s | Start Time: %llu ms\n", p->command, start_time);

                uint64_t quantumcompletion_time = get_current_time_ms() + quantum0;
                execute_process_MLFQ(p, quantumcompletion_time);

                uint64_t completion_time = get_current_time_ms() - firstProcessstart_time;
                printf("%s | %llu | %llu\n", p->command, start_time, completion_time);

                if (p->finished) {
                    p->completion_time = completion_time;
                    p->turnaround_time = p->completion_time - p->arrival_time;
                    p->burst_time += (completion_time - start_time);
                    p->waiting_time = p->turnaround_time - p->burst_time;
                    p->response_time = p->start_time - p->arrival_time;
                    //write_to_csv(p, 1, p->error, p->burst_time, p->turnaround_time, p->waiting_time, p->response_time);

                    // Update the original processes array
                    processes[p->index].completion_time = p->completion_time;
                    processes[p->index].turnaround_time = p->turnaround_time;
                    processes[p->index].burst_time = p->burst_time;
                    processes[p->index].waiting_time = p->waiting_time;
                    processes[p->index].response_time = p->response_time;
                    processes[p->index].finished = p->finished;
                    processes[p->index].error = p->error;
                } else {
                    p->priority = 1;
                    p->burst_time += quantum0;
                    //p->quantum = quantum1;
                    add_to_queue_MLFQ(p);
                }
            } else if (!is_empty_MLFQ(1)) {
                //Dealing with queue1_MLFQ
                Process *p = pop_from_queue_MLFQ(1);
                //p->quantum = quantum1;

                uint64_t start_time = get_current_time_ms() - firstProcessstart_time;
                //printf("queue1_MLFQ: Command: %s | Start Time: %llu ms\n", p->command, start_time);

                uint64_t quantumcompletion_time = get_current_time_ms() + quantum1;
                execute_process_MLFQ(p, quantumcompletion_time);

                uint64_t completion_time = get_current_time_ms() - firstProcessstart_time;
                printf("%s | %llu | %llu\n", p->command, start_time, completion_time);


                if (p->finished) {
                    p->completion_time = completion_time;
                    p->turnaround_time = p->completion_time - p->arrival_time;
                    p->burst_time += (completion_time - start_time);
                    p->waiting_time = p->turnaround_time - p->burst_time;
                    p->response_time = p->start_time - p->arrival_time;
                    //write_to_csv(p, 1, p->error, p->burst_time, p->turnaround_time, p->waiting_time, p->response_time);

                    // Update the original processes array
                    processes[p->index].completion_time = p->completion_time;
                    processes[p->index].turnaround_time = p->turnaround_time;
                    processes[p->index].burst_time = p->burst_time;
                    processes[p->index].waiting_time = p->waiting_time;
                    processes[p->index].response_time = p->response_time;
                    processes[p->index].finished = p->finished;
                    processes[p->index].error = p->error;
                } else {
                    p->priority = 2;
                    p->burst_time += quantum1;
                    //p->quantum = quantum2;
                    add_to_queue_MLFQ(p);
                }
            } else if (!is_empty_MLFQ(2)) {
                //Dealing with queue2_MLFQ
                Process *p = pop_from_queue_MLFQ(2);
                //p->quantum = quantum2;

                uint64_t start_time = get_current_time_ms() - firstProcessstart_time;
                //printf("queue2_MLFQ: Command: %s | Start Time: %llu ms\n", p->command, start_time);

                uint64_t quantumcompletion_time = get_current_time_ms() + quantum2;
                execute_process_MLFQ(p, quantumcompletion_time);

                uint64_t completion_time = get_current_time_ms() - firstProcessstart_time;
                printf("%s | %llu | %llu\n", p->command, start_time, completion_time);

                if (p->finished) {
                    p->completion_time = completion_time;
                    p->turnaround_time = p->completion_time - p->arrival_time;
                    p->burst_time += (completion_time - start_time);
                    p->waiting_time = p->turnaround_time - p->burst_time;
                    p->response_time = p->start_time - p->arrival_time;
                    //write_to_csv(p, 1, p->error, p->burst_time, p->turnaround_time, p->waiting_time, p->response_time);

                    // Update the original processes array
                    processes[p->index].completion_time = p->completion_time;
                    processes[p->index].turnaround_time = p->turnaround_time;
                    processes[p->index].burst_time = p->burst_time;
                    processes[p->index].waiting_time = p->waiting_time;
                    processes[p->index].response_time = p->response_time;
                    processes[p->index].finished = p->finished;
                    processes[p->index].error = p->error;
                } else {
                    p->burst_time += quantum2;
                    add_to_queue_MLFQ(p);  // Re-add to queue2_MLFQ 
                }
            }

            // Handle boost time logic
            if (get_current_time_ms() - lastBoostTime >= boostTime) {
                //printf("Boosting all processes to Queue 0\n");
                boost_queues();
                lastBoostTime = get_current_time_ms();
            }

            
        }
    

    write_csv(processes, n, "MLFQ");
}
