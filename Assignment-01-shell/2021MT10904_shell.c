#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

// Function prototypes

//Main looping function
void execute(void);

//Read the command
char *read_command(void);
char **split_command(char *command_string);

//Fulfil the ask of the command and account for any pipes
int perform_task(char *command_string, char **command_array);
int perform_pipe_task(char *command_string);

//Debugger
void print_string_array(char **array, int size, char *ch);

//Handling the history
void init_history(int initial_capacity);
void resize_history();
void add_to_history(const char *command);
void free_history();
void print_history(void);

//Built-in functions to implement
int shell_cd(char **command_array);
int shell_history(char **command_array);
int shell_exit(char **command_array);
int shell_pwd(char **command_array);

// Built-in commands
char *builtin_str[] = {
  "cd",
  "history",
  "exit",
  "pwd"
};

int (*builtin_func[]) (char **) = {
  &shell_cd,
  &shell_history,
  &shell_exit,
  &shell_pwd
};

// History variables
char **history = NULL;
int history_count = 0;
int history_capacity = 0;




// Main Function
int main(int argc, char *argv[]) {
    init_history(10);
    execute();
    free_history();
    return EXIT_SUCCESS;
}

void execute(void) {
    char *command_string;
    char **command_array;
    int status;

    do {
        printf("MTL458 > ");
        command_string = read_command();  // Read the input given by the user
        if (command_string == NULL) {
            continue;
        }

        add_to_history(command_string);

        command_array = split_command(command_string);  // Break the commands into its parts
        if (command_array == NULL) {
            free(command_string);
            continue;
        }
        status = perform_task(command_string, command_array);  // Perform the tasks mentioned in the command

        free(command_string);
        for (int i = 0; command_array[i] != NULL; i++) {
            free(command_array[i]);
        }
        free(command_array);

    } while (status);
}

// A function that can read input that the user is providing
char *read_command(void) {
    char *command_string = NULL;  // Pointer to the string
    size_t string_size = 0;       // Size of the string
    ssize_t num_of_char;          // Number of characters read

    num_of_char = getline(&command_string, &string_size, stdin);

    if (num_of_char == -1) {
        free(command_string);
        return NULL;
    }

    return command_string;
}

// Once the user provides an input as a string, we split it into an array of arguments for our convenience
char **split_command(char *command_string) {
    char *temp_cmdstr = strdup(command_string);  // Duplicate the input string

    // Check for a null input
    if (temp_cmdstr == NULL) {
        printf("Invalid Command\n");
        exit(EXIT_FAILURE);
    }

    char **command_array = NULL;  // Array to store tokens
    char *token;
    int tokens_count = 0;

    // First pass: count the tokens
    token = strtok(temp_cmdstr, " \t\n");
    while (token != NULL) {
        tokens_count++;
        token = strtok(NULL, " \t\n");
    }

    free(temp_cmdstr);  // Free the duplicated string

    // Allocate memory for the array of tokens
    command_array = malloc((tokens_count + 1) * sizeof(char *));  // +1 for NULL termination
    if (command_array == NULL) {
        printf("Invalid Command\n");
        return NULL;
    }

    // Second pass: store the tokens
    temp_cmdstr = strdup(command_string);  // Duplicate the input string again
    if (temp_cmdstr == NULL) {
        printf("Invalid Command\n");
        free(command_array);
        exit(EXIT_FAILURE);
    }

    tokens_count = 0;
    token = strtok(temp_cmdstr, " \t\n");
    while (token != NULL) {
        command_array[tokens_count++] = strdup(token);
        if (command_array[tokens_count - 1] == NULL) {
            printf("Invalid Command\n");
            for (int i = 0; i < tokens_count - 1; i++) {
                free(command_array[i]);
            }
            free(command_array);
            free(temp_cmdstr);
            exit(EXIT_FAILURE);
        }
        token = strtok(NULL, " \t\n");
    }

    command_array[tokens_count] = NULL;  // NULL terminate the array

    free(temp_cmdstr);  // Free the duplicated string again

    return command_array;
}

void print_string_array(char **array, int size, char *ch) {
    for (int i = 0; i < size; i++) {
        printf("%s", array[i]); // Print the command
        if (ch) { 
            printf("%s", ch); // Only print the ch string if it's not NULL
        }
    }
}

int perform_task(char *command_string, char **command_array) {
    if (command_array[0] == NULL){
        return 1;  // Empty command was entered, prompt for another one
    }

    for (int i = 0; command_array[i] != NULL; i++){
        if (strcmp(command_array[i], "|") == 0){
            return perform_pipe_task(command_string);
        }
    }

    int num_of_builtins = sizeof(builtin_str)/sizeof(char *);  // number of builtin functions that need to be checked for

    for (int i = 0; i < num_of_builtins; i++){
        if (strcmp(builtin_str[i], command_array[0]) == 0){
            return (*builtin_func[i])(command_array);
        }
    }

    // External commands
    pid_t pid = fork();
    if (pid == 0) {
        // Child process carries out the task
        if (execvp(command_array[0], command_array) == -1) {
            printf("Invalid Command\n");
        }
        exit(EXIT_FAILURE);
    } 
    else if (pid < 0) {
        printf("Invalid Command\n");
    } 
    else {
        wait(NULL);  // parent process waits for its child process to finish
    }

    return 1;
}

// Implement pipe handling
int perform_pipe_task(char *command_string) {
    int pipe_fds[2];  // File descriptors for the pipe
    pid_t p1, p2;

    // Split the command string into two parts based on the pipe
    char *command1 = strtok(command_string, "|");
    char *command2 = strtok(NULL, "|");

    if (command1 == NULL || command2 == NULL) {
        printf("Invalid Command\n");
        return 1;
    }

    // Create the pipe
    if (pipe(pipe_fds) == -1) {
        perror("pipe");
        return 1;
    }

    // Fork the first child process
    if ((p1 = fork()) == -1) {
        perror("fork");
        return 1;
    }

    if (p1 == 0) {
        // In the first child process
        close(pipe_fds[0]); // Close the read end of the pipe
        dup2(pipe_fds[1], STDOUT_FILENO); // Redirect stdout to the write end of the pipe
        close(pipe_fds[1]);

        // Execute the first command
        char **command_array1 = split_command(command1);

        int num_of_builtins = sizeof(builtin_str) / sizeof(char *);
        for (int i = 0; i < num_of_builtins; i++) {
            if (strcmp(builtin_str[i], command_array1[0]) == 0) {
                // Call perform_task directly for built-in commands
                int result = (*builtin_func[i])(command_array1);
                free(command_array1);
                exit(result);
            }
        }

        if (execvp(command_array1[0], command_array1) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    } 
    else {
        // Fork the second child process
        if ((p2 = fork()) == -1) {
            perror("fork");
            return 1;
        }

        if (p2 == 0) {
            // In the second child process
            close(pipe_fds[1]); // Close the write end of the pipe
            dup2(pipe_fds[0], STDIN_FILENO); // Redirect stdin to the read end of the pipe
            close(pipe_fds[0]);

            // Execute the second command
            char **command_array2 = split_command(command2);
            if (execvp(command_array2[0], command_array2) == -1) {
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        } else {
            // In the parent process
            close(pipe_fds[0]); // Close the read end of the pipe
            close(pipe_fds[1]); // Close the write end of the pipe

            // Wait for both child processes to finish
            waitpid(p1, NULL, 0);
            waitpid(p2, NULL, 0);
        }
    }

    return 1;
}

// defining the functions to take care of the history 
void init_history(int initial_capacity) {
    history_capacity = initial_capacity;
    history = malloc(history_capacity * sizeof(char *));
    if (history == NULL) {
        printf("Invalid Command\n");
        exit(EXIT_FAILURE);
    }
}

void resize_history() {
    history_capacity *= 2; // Double the capacity
    history = realloc(history, history_capacity * sizeof(char *));
    if (history == NULL) {
        printf("Invalid Command\n");
        exit(EXIT_FAILURE);
    }
}

void add_to_history(const char *command) {
    if (history_count == history_capacity) {
        resize_history();
    }

    history[history_count++] = strdup(command);
    if (history[history_count - 1] == NULL) {
        printf("Invalid Command\n");
        exit(EXIT_FAILURE);
    }
}

void free_history() {
    for (int i = 0; i < history_count; i++) {
        free(history[i]);
    }
    free(history);
}



// Writing the built-in functions of the shell

// cd function using chdir functionality - change directory and continue accepting more commands
int shell_cd(char **command_array) {
    char *path = command_array[1];
    static char prev_dir[1024] = "";  // Stores the previous directory path
    char current_dir[1024];

    if (path == NULL || strcmp(path, "~") == 0) {
        // Handle "cd" or "cd ~" by navigating to the home directory
        path = getenv("HOME");
    } else if (strcmp(path, "-") == 0) {
        // Handle "cd -" by navigating to the previous directory
        if (strlen(prev_dir) == 0) {
            // If OLDPWD is not set, use the home directory instead
            path = getenv("HOME");
            if (path == NULL) {
                printf("cd: HOME not set\n");
                return 1;
            }
        } else {
            path = prev_dir;
        }
    } else {
        // Remove quotes from the path if present
        int len = strlen(path);
        if ((path[0] == '"' && path[len - 1] == '"') || 
            (path[0] == '\'' && path[len - 1] == '\'')) {
            path[len - 1] = '\0';
            path++;
        }
    }

    // Store the current directory before changing it
    if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
        perror("getcwd");
        return 1;
    }

    // Change the directory
    if (chdir(path) != 0) {
        perror("chdir");
        return 1;
    }

    // Update the previous directory
    strcpy(prev_dir, current_dir);

    return 1;
}

// pwd function using the getcwd functionality - print present directory and continue accepting more commands
int shell_pwd(char **command_array){
    char buffer[1024];  // Buffer to store the pwd

    if (getcwd(buffer, sizeof(buffer)) != NULL){
        printf("%s\n", buffer);
    }
    else{
        printf("Invalid Command\n");
    }
    return 1;
}

// Print the history and continue accepting more commands
int shell_history(char **command_array){
    print_string_array(history, history_count, ""); // Use an empty string instead of "\n"
    return 1;
}

// terminate the program by setting status to 0
int shell_exit(char **command_array){
    return 0;
}