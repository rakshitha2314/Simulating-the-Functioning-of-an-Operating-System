#pragma once

//Can include any other headers as needed
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>


typedef struct {
    char *command;
    bool finished;
    bool error;    
    uint64_t startTime;
    uint64_t completionTime;
    uint64_t turnaroundTime;
    uint64_t waitingTime;
    uint64_t responseTime;
    uint64_t arrivalTime;
    uint64_t endTime;
    uint64_t burstTimeAvg;
    uint64_t burstTime;
    bool started; 
    pid_t pid;
    int priority;

} Process;

#define MAX_QUEUE_SIZE 100
#define MAX_COMMAND_ARGS 10
#define MAX_COMMAND_LENGTH 256

Process* priorityQueue[MAX_QUEUE_SIZE];
int queueSize = 0;
uint64_t commandBurstTimes[MAX_QUEUE_SIZE]; // Store average burst times
int commandCount[MAX_QUEUE_SIZE]; // Track how many times the same command has run
char *commands[MAX_QUEUE_SIZE]; // Store distinct commands
int commandIndex = 0; // Total number of distinct commands

Process* queue0[MAX_QUEUE_SIZE];
Process* queue1[MAX_QUEUE_SIZE];
Process* queue2[MAX_QUEUE_SIZE];
int queueSize0 = 0;
int queueSize1 = 0;
int queueSize2 = 0;

FILE *csvFile; // File pointer for CSV
uint64_t firstProcessStartTime; // Absolute start time of the first process
//long firstProcessStartTime;
long lastBoostTime;

char buffer[1024];

// Generic Functions
uint64_t get_time_in_ms();
void write_to_csv(Process* p, int finished, int errorStatus, uint64_t burstTime, uint64_t turnaroundTime, uint64_t waitingTime, uint64_t responseTime);

// Helper Functions for SJF
void add_to_queue_SJF(Process* p);
void execute_process_SJF(Process* p);
void update_burst_times(Process* completedProcess);
Process* pop_from_queue_SJF();
int is_empty_SJF();
void handle_non_blocking_input_SJF();

// Helper Functions for MLFQ
void add_to_queue_MLFQ(Process* p);
Process* pop_from_queue_MLFQ(int priority);
int is_empty_MLFQ(int priority);
void boost_queues();
void execute_process_MLFQ(Process *p, uint64_t quantum_end_time);
void handle_non_blocking_input_MLFQ(int quantum0, int quantum1);


// Function prototypes
void ShortestJobFirst();
void MultiLevelFeedbackQueue(int quantum0, int quantum1, int quantum2, int boostTime);



// Generic Functions
void write_to_csv(Process* p, int finished, int errorStatus, uint64_t burstTime, uint64_t turnaroundTime, uint64_t waitingTime, uint64_t responseTime) {
    // Write process details to CSV
    char *yes = "Yes";
    char *no = "No";
    char *finish = yes;
    if (errorStatus) finish = no;
    char *error = no;
    if (errorStatus) error = yes;
    fprintf(csvFile, "%s,%s,%s,%llu,%llu,%llu,%llu\n", p->command, finish, error, burstTime, turnaroundTime, waitingTime, responseTime);
}


uint64_t get_time_in_ms() {
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return (currentTime.tv_sec * 1000) + (currentTime.tv_usec / 1000);
}



// Helper Functions for SJF
void add_to_queue_SJF(Process* p) {
    if (queueSize < MAX_QUEUE_SIZE) {
        priorityQueue[queueSize++] = p;
        p->startTime = get_time_in_ms() - firstProcessStartTime; // Relative start time
    }
}

void execute_process_SJF(Process* p) {
    char* commandCopy = strdup(p->command);
    char* token = strtok(commandCopy, " ");
    char* args[10]; // Assuming max of 10 arguments
    int i = 0;
    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL; // Null-terminate the argument list

    uint64_t startTime = get_time_in_ms() - firstProcessStartTime; // Record start time before fork

    pid_t pid = fork();
    int errorStatus = 0;
    int finished = 1;
    if (pid == 0) {
        // In child process
        p->startTime = startTime;  // Start of context
        execvp(args[0], args);
        //perror("execvp failed");
        exit(EXIT_FAILURE); // Exit if execvp fails
    } else {
        // In parent process
        int status;
        waitpid(pid, &status, 0);
        uint64_t endTime = get_time_in_ms() - firstProcessStartTime; // Record end time after process completion
        p->endTime = endTime;  // End of context

        // Calculate burst time
        p->burstTime = endTime - p->startTime;

        // Calculate turnaround time, waiting time, and response time
        p->turnaroundTime = p->endTime - p->arrivalTime;
        p->waitingTime = p->startTime - p->arrivalTime;
        p->responseTime = p->waitingTime;  // In SJF, waiting time is the same as response time

        // Print end time of context
        printf("%s | %llu | %llu\n", p->command, p->startTime, p->endTime);

        // Check process status
        //finished = 1;
        if (WIFEXITED(status)) {
            errorStatus = WEXITSTATUS(status);
        } else {
            errorStatus = 1;  // Indicates abnormal termination
            finished = 0;
        }
        //printf("AT: %s, %llu", p->command, p->arrivalTime);
        // Write process details to the CSV
        write_to_csv(p, finished, errorStatus, p->burstTime, p->turnaroundTime, p->waitingTime, p->responseTime);
    }

    free(commandCopy); // Free the duplicated command string
}


void update_burst_times(Process* completedProcess) {
    int found = 0;
    uint64_t updatedBurstTime = 0;

    // Update the burst time for the completed process' command
    for (int j = 0; j < commandIndex; j++) {
        if (strcmp(commands[j], completedProcess->command) == 0) {
            commandCount[j]++;
            commandBurstTimes[j] = ((commandBurstTimes[j] * (commandCount[j] - 1)) + completedProcess->burstTime) / commandCount[j];
            updatedBurstTime = commandBurstTimes[j];
            found = 1;
            break;
        }
    }

    // Update the burst times in the queue for all processes with the same command
    if (found) {
        for (int i = 0; i < queueSize; i++) {
            Process* p = priorityQueue[i];
            if (strcmp(p->command, completedProcess->command) == 0) {
                p->burstTime = updatedBurstTime;
            }
        }
    }
}

Process* pop_from_queue_SJF() {
    if (queueSize == 0) return NULL;

    // Find process with the shortest burst time (SJF)
    int shortestIndex = 0;
    for (int i = 1; i < queueSize; i++) {
        if (priorityQueue[i]->burstTime < priorityQueue[shortestIndex]->burstTime) {
            shortestIndex = i;
        }
    }

    Process* selectedProcess = priorityQueue[shortestIndex];

    // Shift remaining processes in the queue
    for (int i = shortestIndex; i < queueSize - 1; i++) {
        priorityQueue[i] = priorityQueue[i + 1];
    }
    queueSize--;

    return selectedProcess;
}

int is_empty_SJF() {
    return queueSize == 0;
}


void handle_non_blocking_input_SJF() {
    while (fgets(buffer, sizeof(buffer), stdin)) {
        buffer[strcspn(buffer, "\n")] = 0;  // Remove newline character
        if (strcmp(buffer, "exit") == 0) {
            exit(0);
        }

        // Create a new process for the command
        Process* newProcess = (Process*)malloc(sizeof(Process));
        newProcess->command = strdup(buffer);
        newProcess->arrivalTime = get_time_in_ms() - firstProcessStartTime; // Relative arrival time
        newProcess->startTime = 0;
        newProcess->turnaroundTime = 0;
        newProcess->endTime = 0;
        newProcess->waitingTime = 0;
        newProcess->responseTime = 0;
        newProcess->completionTime = 0;
        newProcess->burstTime = 0;
        newProcess->started=0;
        newProcess->finished = 0;
        newProcess->pid =0;
        
        // Check if the command has been executed before
        int found = 0;
        for (int i = 0; i < commandIndex; i++) {
            if (strcmp(commands[i], newProcess->command) == 0) {
                newProcess->burstTime = commandBurstTimes[i];
                found = 1;
                break;
            }
        }

        if (!found) {
            // If it's a new command, add it to the list
            commands[commandIndex] = strdup(newProcess->command);
            newProcess->burstTime = 1000; // Default burst time for new commands
            commandBurstTimes[commandIndex] = newProcess->burstTime;
            commandCount[commandIndex] = 1;
            commandIndex++;
        }

        // Add process to queue
        add_to_queue_SJF(newProcess);
    }
}



// Helper Functions for MLFQ Online
//enqueue operation
void add_to_queue_MLFQ(Process* p) {
    if (p->priority == 0 && queueSize0 < MAX_QUEUE_SIZE) {
        queue0[queueSize0++] = p;
    } else if (p->priority == 1 && queueSize1 < MAX_QUEUE_SIZE) {
        queue1[queueSize1++] = p;
    } else if (p->priority == 2 && queueSize2 < MAX_QUEUE_SIZE) {
        queue2[queueSize2++] = p;
    }
}

//dequeue operation
Process* pop_from_queue_MLFQ(int priority) {
    Process* p = NULL;
    if (priority == 0 && queueSize0 > 0) {
        p = queue0[0];
        for (int i = 1; i < queueSize0; i++) {
            queue0[i - 1] = queue0[i];
        }
        queueSize0--;
    } else if (priority == 1 && queueSize1 > 0) {
        p = queue1[0];
        for (int i = 1; i < queueSize1; i++) {
            queue1[i - 1] = queue1[i];
        }
        queueSize1--;
    } else if (priority == 2 && queueSize2 > 0) {
        p = queue2[0];
        for (int i = 1; i < queueSize2; i++) {
            queue2[i - 1] = queue2[i];
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

//bosting all remainig processes to queue0
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
    if (!p->started){
        //executing a process for the first time by forking
        p->started = 1;
        char *args[MAX_COMMAND_ARGS + 1];
        memset(args, 0, sizeof(args));

        char command_copy[MAX_COMMAND_LENGTH];
        strncpy(command_copy, p->command, MAX_COMMAND_LENGTH - 1);
        command_copy[MAX_COMMAND_LENGTH - 1] = '\0';
        //tokenizing the command
        char *token = strtok(command_copy, " ");
        int i = 0;
        while (token != NULL && i < MAX_COMMAND_ARGS) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;

        pid_t pid = fork();
        if (pid == 0) {
            execvp(args[0], args);      //running the command
            //p->error = 1;
            //perror("execvp failed");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            p->pid = pid;   //setting pid
            p->startTime = get_time_in_ms() - firstProcessStartTime;
            //printf("Queue%d: Command: %s | Start Time: %llu ms\n", p->priority, p->command, p->startTime);
        } else {
            p->error = 1;
            //perror("fork");
            return;
        }
    } else {
        if (kill(p->pid, SIGCONT) == -1) {      //SIGCONT if the process has been previously started
            p->error = 1;
            //perror("SIGCONT error");
            return;
        }
        //printf("Queue%d: Command: %s | Continued.\n", p->priority, p->command);
    }

    
    while (get_time_in_ms() < quantum_end_time) {
        if (waitpid(p->pid, NULL, WNOHANG) != 0) {
            //If the process finishes within the quantum
            p->finished = 1;
            //p->completionTime = get_time_in_ms() - firstProcessStartTime;
            break;
        }
        usleep(10000);
    }

    if (waitpid(p->pid, NULL, WNOHANG) != 0) {
        p->finished = 1;
    }

    if (!p->finished) {
        //STOP it if the quantum is over, the process is not
        kill(p->pid, SIGSTOP);
    }
    
}

void handle_non_blocking_input_MLFQ(int quantum0, int quantum1) {
    //Non-blocking input, taking inputs continuously through the console
    while (fgets(buffer, sizeof(buffer), stdin)) {
        buffer[strcspn(buffer, "\n")] = 0;
        if (strcmp(buffer, "exit") == 0) {
            exit(0);
        }

        Process* newProcess = (Process*)malloc(sizeof(Process));
        newProcess->command = strdup(buffer);
        newProcess->arrivalTime = get_time_in_ms() - firstProcessStartTime;

        // Check if the command has been executed before
        int found = 0;
        for (int i = 0; i < commandIndex; i++) {
            if (strcmp(commands[i], newProcess->command) == 0) {
                newProcess->burstTimeAvg = commandBurstTimes[i];
                if (newProcess->burstTimeAvg <= quantum0){
                    newProcess->priority = 0;
                }
                else if (newProcess->burstTimeAvg <= quantum1) {
                    newProcess->priority = 1;
                }
                else{
                    newProcess->priority = 2;
                }
                found = 1;
                break;
            }
        }

        if (!found) {
            // If it's a new command, add it to the list
            commands[commandIndex] = strdup(newProcess->command);
            newProcess->burstTimeAvg = 1000; // Default burst time for new commands
            newProcess->priority = 1;
            commandBurstTimes[commandIndex] = newProcess->burstTimeAvg;
            commandCount[commandIndex] = 1;
            commandIndex++;
        }

        //newProcess->priority = 0;
        newProcess->finished = 0;
        //newProcess->quantum = 0;
        newProcess->startTime = 0;
        newProcess->completionTime = 0;
        newProcess->turnaroundTime = 0;
        newProcess->waitingTime = 0;
        newProcess->responseTime = 0;
        newProcess->burstTime = 0;
        //newProcess->remainingTime = 1000;

        add_to_queue_MLFQ(newProcess);
    }
}

void update_burst_times_MLFQ(Process* completedProcess) {
    int found = 0;
    uint64_t updatedBurstTime = 0;

    // Update the burst time for the completed process' command
    for (int j = 0; j < commandIndex; j++) {
        if (strcmp(commands[j], completedProcess->command) == 0) {
            commandCount[j]++;
            commandBurstTimes[j] = ((commandBurstTimes[j] * (commandCount[j] - 1)) + completedProcess->burstTime) / commandCount[j];
            updatedBurstTime = commandBurstTimes[j];
            found = 1;
            break;
        }
    }
}



// SJF Function
void ShortestJobFirst() {
    // Open CSV file for writing
    csvFile = fopen("result_online_SJF.csv", "w");
    if (csvFile == NULL) {
        //perror("Error opening CSV file");
        exit(1);
    }
    // Write header to the CSV
    fprintf(csvFile, "Command,Finished,Error,Burst Time,Turnaround Time,Waiting Time,Response Time\n");

    // Initialize non-blocking input using fcntl
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    firstProcessStartTime = get_time_in_ms(); // Record start time of the first process

    while (1) {
        // Handle input and update the queue when input is available
        handle_non_blocking_input_SJF();

        // If there's a process in the queue, execute it
        if (!is_empty_SJF()) {
            Process* p = pop_from_queue_SJF();
            p->startTime = get_time_in_ms() - firstProcessStartTime;
            execute_process_SJF(p);
            p->endTime = get_time_in_ms() - firstProcessStartTime;
            p->burstTime = p->endTime - p->startTime;
            p->turnaroundTime = p->endTime - p->arrivalTime;
            p->waitingTime = p->startTime - p->arrivalTime;
            p->responseTime = p->waitingTime;
            update_burst_times(p);
        }
    }

    fclose(csvFile); // Close the CSV file when done
}

// Online MLFQ Function
void MultiLevelFeedbackQueue(int quantum0, int quantum1, int quantum2, int boostTime) {
    //CSV as required for logging the processes
    csvFile = fopen("result_online_MLFQ.csv", "w");
    if (csvFile == NULL) {
        //perror("Error opening CSV file");
        exit(1);
    }
    fprintf(csvFile, "Command,Finished,Error,Burst Time,Turnaround Time,Waiting Time,Response Time\n");

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    firstProcessStartTime = get_time_in_ms();       //when the MLFQ is initiated
    lastBoostTime = firstProcessStartTime;          //first boost is assumed at t=0
    //int firstProcessStarted = 0;

    while (1) {
        handle_non_blocking_input_MLFQ(quantum0, quantum1);

        while (!is_empty_MLFQ(0) || !is_empty_MLFQ(1) || !is_empty_MLFQ(2)) {
            if (!is_empty_MLFQ(0)) {
                //dealing with the queue0
                //printf("its %d", 0);
                Process *p = pop_from_queue_MLFQ(0);
                //p->quantum = quantum0;
                // if (!firstProcessStarted) {
                //     firstProcessStartTime = get_time_in_ms();
                //     firstProcessStarted = 1;
                // }

                uint64_t startTime = get_time_in_ms() - firstProcessStartTime;
                //printf("Queue0: Command: %s | Start Time: %llu ms\n", p->command, startTime);

                uint64_t quantumcompletionTime = get_time_in_ms() + quantum0;
                execute_process_MLFQ(p, quantumcompletionTime);

                uint64_t completionTime = get_time_in_ms() - firstProcessStartTime;
                printf("%s | %llu | %llu\n", p->command, startTime, completionTime);

                if (p->finished) {
                    p->completionTime = completionTime;
                    p->turnaroundTime = p->completionTime - p->arrivalTime;
                    p->burstTime += (completionTime - startTime);
                    p->waitingTime = p->turnaroundTime - p->burstTime;
                    p->responseTime = p->startTime - p->arrivalTime;
                    update_burst_times_MLFQ(p);
                    write_to_csv(p, 1, p->error, p->burstTime, p->turnaroundTime, p->waitingTime, p->responseTime);
                } else {
                    p->priority = 1;
                    p->burstTime += quantum0;
                    //p->quantum = quantum1;
                    add_to_queue_MLFQ(p);
                }
            } else if (!is_empty_MLFQ(1)) {
                //Dealing with queue1
                //printf("its %d", 1);
                Process *p = pop_from_queue_MLFQ(1);
                //p->quantum = quantum1;

                uint64_t startTime = get_time_in_ms() - firstProcessStartTime;
                //printf("Queue1: Command: %s | Start Time: %llu ms\n", p->command, startTime);

                uint64_t quantumcompletionTime = get_time_in_ms() + quantum1;
                execute_process_MLFQ(p, quantumcompletionTime);

                uint64_t completionTime = get_time_in_ms() - firstProcessStartTime;
                printf("%s | %llu | %llu\n", p->command, startTime, completionTime);


                if (p->finished) {
                    p->completionTime = completionTime;
                    p->turnaroundTime = p->completionTime - p->arrivalTime;
                    p->burstTime += (completionTime - startTime);
                    p->waitingTime = p->turnaroundTime - p->burstTime;
                    p->responseTime = p->startTime - p->arrivalTime;
                    update_burst_times_MLFQ(p);
                    write_to_csv(p, 1, p->error, p->burstTime, p->turnaroundTime, p->waitingTime, p->responseTime);
                } else {
                    p->priority = 2;
                    p->burstTime += quantum1;
                    //p->quantum = quantum2;
                    add_to_queue_MLFQ(p);
                }
            } else if (!is_empty_MLFQ(2)) {
                //Dealing with queue2
                //printf("its %d", 2);
                Process *p = pop_from_queue_MLFQ(2);
                //p->quantum = quantum2;

                uint64_t startTime = get_time_in_ms() - firstProcessStartTime;
                //printf("Queue2: Command: %s | Start Time: %llu ms\n", p->command, startTime);

                uint64_t quantumcompletionTime = get_time_in_ms() + quantum2;
                execute_process_MLFQ(p, quantumcompletionTime);

                uint64_t completionTime = get_time_in_ms() - firstProcessStartTime;
                printf("%s | %llu | %llu\n", p->command, startTime, completionTime);

                if (p->finished) {
                    p->completionTime = completionTime;
                    p->turnaroundTime = p->completionTime - p->arrivalTime;
                    p->burstTime += (completionTime - startTime);
                    p->waitingTime = p->turnaroundTime - p->burstTime;
                    p->responseTime = p->startTime - p->arrivalTime;
                    update_burst_times_MLFQ(p);
                    write_to_csv(p, 1, p->error, p->burstTime, p->turnaroundTime, p->waitingTime, p->responseTime);
                } else {
                    p->burstTime += quantum2;
                    add_to_queue_MLFQ(p);  // Re-add to queue2 
                }
            }

            // Handle boost time logic
            if (get_time_in_ms() - lastBoostTime >= boostTime) {
                //printf("Boosting all processes to Queue 0\n");
                boost_queues();
                lastBoostTime = get_time_in_ms();
            }

            //Check for any new inputs
            handle_non_blocking_input_MLFQ(quantum0, quantum1);
        }
    }

    fclose(csvFile);
}
