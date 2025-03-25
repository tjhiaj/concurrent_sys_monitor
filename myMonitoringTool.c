#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <sys/resource.h>
#include <unistd.h>
#include <signal.h>
// #include <sys/types.h>
#include <sys/wait.h>

// Function prototypes
int isNumber(char* input);
int getCoreCount();
double getCpuUsage(long* last_total_user, long* last_total_user_low, long* last_total_sys, long* last_total_idle);
void parseArguments(int argc, char **argv, int *samples, int *tdelay, int *memory, int *cpu, int *cores);
void printCoreDiagram(int num_cores);
void printMemoryUsage(int samples, int col_index, int **mem_history, int pos, double total_memory, double used_memory);
// void printCpuUsage(int samples, int col_index, int **cpu_history, long *last_total_user, long *last_total_user_low, long *last_total_sys, long *last_total_idle);
void printCpuUsage(int samples, int col_index, int **cpu_history, double cpu_usage);
void cleanup(int **cpu_history, int **mem_history);

void ignore(int code){
    printf("\nStop ignored (code=%d)\n", code);
}

void quit(int code){
    printf("\nInterrupt (code=%d) Do you want to quit? Input 'y' or 'n'\n", code);
    char response = getchar();
    if (response == 'y') {
        printf("\nQuitting\n");
        exit(0);
    } else {
        printf("\nContinuing\n");
    }
}

/**
 * Checks if a string represents a valid number.
 * 
 * @param input The string to check.
 * @return 1 if the string is a valid number, 0 otherwise.
 */
int isNumber(char* input) {
    int length = strlen(input);
    for (int i = 0; i < length; i++) {
        if (!isdigit(input[i])) {
            return 0;
        }
    }
    return 1;
}

/**
 * Retrieves the number of CPU cores by reading /proc/cpuinfo.
 * 
 * @return The number of CPU cores, or -1 on error.
 */
int getCoreCount() {
    FILE *file = fopen("/proc/cpuinfo", "r");
    if (file == NULL) {
        fprintf(stderr, "Error: Failed to open /proc/cpuinfo\n");
        exit(1);
    }

    int core_count = 0;
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "processor", 9) == 0) {
            core_count++;
        }
    }
    fclose(file);
    return core_count;
}

/**
 * Calculates the CPU usage by reading /proc/stat.
 * 
 * @param last_total_user Pointer to the previous total user time.
 * @param last_total_user_low Pointer to the previous total low user time.
 * @param last_total_sys Pointer to the previous total system time.
 * @param last_total_idle Pointer to the previous total idle time.
 * @return The CPU usage as a percentage.
 */
double getCpuUsage(long* last_total_user, long* last_total_user_low, long* last_total_sys, long* last_total_idle) {
    FILE *file = fopen("/proc/stat", "r");
    if (file == NULL) {
        fprintf(stderr, "Error: Failed to open /proc/stat\n");
        exit(1);
    }

    char line[128];
    fgets(line, sizeof(line), file);
    fclose(file);

    long total_user, total_user_low, total_sys, total_idle;
    sscanf(line, "cpu  %ld %ld %ld %ld", &total_user, &total_user_low, &total_sys, &total_idle);

    long total = (total_user - *last_total_user) + (total_user_low - *last_total_user_low) +
                 (total_sys - *last_total_sys);
    long total_time = total + (total_idle - *last_total_idle);

    double cpu_usage = (double)total / total_time * 100;
    *last_total_user = total_user;
    *last_total_user_low = total_user_low;
    *last_total_sys = total_sys;
    *last_total_idle = total_idle;

    return cpu_usage;
}

/**
 * Parses command-line arguments and updates the corresponding parameters.
 * 
 * @param argc The number of command-line arguments.
 * @param argv The array of command-line arguments.
 * @param samples Pointer to the number of samples.
 * @param tdelay Pointer to the time delay between samples.
 * @param memory Pointer to the memory flag.
 * @param cpu Pointer to the CPU flag.
 * @param cores Pointer to the cores flag.
 */
void parseArguments(int argc, char **argv, int *samples, int *tdelay, int *memory, int *cpu, int *cores) {
    int samples_pos_detected = 0;
    int flag_detected = 0;

    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--memory", 8) == 0) {
            *memory = 1;
            flag_detected = 1;
        } else if (strncmp(argv[i], "--cpu", 5) == 0) {
            *cpu = 1;
            flag_detected = 1;
        } else if (strncmp(argv[i], "--cores", 7) == 0) {
            *cores = 1;
            flag_detected = 1;
        } else if (strncmp(argv[i], "--samples=", 10) == 0) {
            *samples = (int)(strtod(argv[i] + 10, NULL));
        } else if (strncmp(argv[i], "--tdelay=", 9) == 0) {
            *tdelay = (int)(strtod(argv[i] + 9, NULL));
        } else if (isNumber(argv[i])) {
            if (i == 1) {
                *samples = (int)(strtod(argv[i], NULL));
                samples_pos_detected = 1;
            }
            if (i == 2 && samples_pos_detected) {
                *tdelay = (int)(strtod(argv[i], NULL));
            }
        } else {
            fprintf(stderr, "Error: Invalid command line argument '%s'.\n", argv[i]);
            fprintf(stderr, "Usage: ./myMonitoringTool [samples [tdelay]] [--memory] [--cpu] [--cores] [--samples=N] [--tdelay=T]\n");
            exit(1);
        }
    }

    if (flag_detected == 0) {
        *memory = 1;
        *cpu = 1;
        *cores = 1;
    }
}

/**
 * Prints a diagram representing the CPU cores in a grid (4 cores per row).
 * 
 * @param num_cores The number of CPU cores.
 */
void printCoreDiagram(int num_cores) {
    int cores_per_row = 4; // Number of cores per row
    int rows = (num_cores + cores_per_row - 1) / cores_per_row; // Calculate the number of rows

    for (int row = 0; row < rows; row++) {
        // Print the top line of squares
        for (int core = 0; core < cores_per_row; core++) {
            int core_index = row * cores_per_row + core;
            if (core_index < num_cores) {
                printf("+---+ ");
            } else {
                printf("      "); // Fill with spaces if no core exists in this position
            }
        }
        printf("\n");

        // Print the middle line of squares
        for (int core = 0; core < cores_per_row; core++) {
            int core_index = row * cores_per_row + core;
            if (core_index < num_cores) {
                printf("|   | ");
            } else {
                printf("      "); // Fill with spaces if no core exists in this position
            }
        }
        printf("\n");

        // Print the bottom line of squares
        for (int core = 0; core < cores_per_row; core++) {
            int core_index = row * cores_per_row + core;
            if (core_index < num_cores) {
                printf("+---+ ");
            } else {
                printf("      "); // Fill with spaces if no core exists in this position
            }
        }
        printf("\n");
    }
}

/**
 * Prints memory usage information and updates the memory history.
 * 
 * @param samples The number of samples.
 * @param col_index The current column index in the history array.
 * @param mem_history The memory history array.
 * @param info Pointer to the sysinfo structure.
 */
void printMemoryUsage(int samples, int col_index, int **mem_history, int pos, double total_memory, double used_memory) {
    // Clear previous values in this column before setting the new #
    for (int row = 0; row < 10; row++) {
        mem_history[row][col_index] = 0;
    }
    mem_history[pos][col_index] = 1; // Place new #

    printf("v Memory %.2f GB\n", used_memory);

    for (int row = 0; row < 10; row++) {
        if (row == 0){
            printf("%.2f GB |", total_memory);
        }
        else{
            printf("\n         |");
        }
        for (int col = 0; col < samples; col++) {
            if (mem_history[row][col]){
                printf("#");
            }
            else{
                printf(" ");
            }
        }
    }

    // Print bottom axis
    printf("\n    0 GB ");
    for (int col = 0; col < samples; col++) {
        printf("-");
    }
    printf("\n");
}

/**
 * Prints CPU usage information and updates the CPU history.
 * 
 * @param samples The number of samples.
 * @param col_index The current column index in the history array.
 * @param cpu_history The CPU history array.
 * @param last_total_user Pointer to the previous total user time.
 * @param last_total_user_low Pointer to the previous total low user time.
 * @param last_total_sys Pointer to the previous total system time.
 * @param last_total_idle Pointer to the previous total idle time.
 */
void printCpuUsage(int samples, int col_index, int **cpu_history, double cpu_usage) {
    // Convert CPU usage to a vertical position
    int used_blocks_cpu = (int)((cpu_usage / 100) * 9);

    printf("v CPU %.2f %%\n", cpu_usage);

    // Clear previous values in this column before setting the new :
    for (int row = 0; row < 10; row++) {
        cpu_history[row][col_index] = 0;
    }
    cpu_history[9 - used_blocks_cpu][col_index] = 1; // Place new :

    for (int row = 0; row < 10; row++) {
        if (row == 0){
            printf("    100%% |");
        }
        else{
            printf("\n         |");
        }
        for (int col = 0; col < samples; col++) {
            if (cpu_history[row][col]){
                printf(":");
            }
            else{
                printf(" ");
            }
        }
    }

    // Print bottom axis
    printf("\n    0 GB ");
    for (int col = 0; col < samples; col++) {
        printf("-");
    }
    printf("\n");
}

/**
 * Frees dynamically allocated memory.
 * 
 * @param cpu_history The CPU history array.
 * @param mem_history The memory history array.
 */
void cleanup(int **cpu_history, int **mem_history) {
    for (int row = 0; row < 10; row++) {
        free(cpu_history[row]);
        free(mem_history[row]);
    }
    free(cpu_history);
    free(mem_history);
}

int main(int argc, char **argv) {
    int samples = 20;
    int tdelay = 500000;
    int memory = 0;
    int cpu = 0;
    int cores = 0;

    struct sigaction stpact;
    struct sigaction intact;

    stpact.sa_handler = ignore;
    stpact.sa_flags = 0;
    intact.sa_handler = quit;
    intact.sa_flags = 0;

    if (sigaction(SIGINT, &intact,NULL) == -1) exit(1);
    if (sigaction(SIGTSTP, &stpact,NULL) == -1) exit(1);

    // Parse command-line arguments
    parseArguments(argc, argv, &samples, &tdelay, &memory, &cpu, &cores);

    // Get the number of CPU cores
    int num_cores = getCoreCount();
    if (num_cores <= 0) {
        fprintf(stderr, "Error: Failed to determine CPU core count\n");
        exit(1);
    }

    // Dynamically allocate memory for history arrays
    int **cpu_history = malloc(10 * sizeof(int *));
    int **mem_history = malloc(10 * sizeof(int *));
    if (cpu_history == NULL || mem_history == NULL) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }
    for (int row = 0; row < 10; row++) {
        cpu_history[row] = malloc(samples * sizeof(int));
        mem_history[row] = malloc(samples * sizeof(int));
        if (cpu_history[row] == NULL || mem_history[row] == NULL) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            cleanup(cpu_history, mem_history);
            exit(1);
        }
        memset(cpu_history[row], 0, samples * sizeof(int));
        memset(mem_history[row], 0, samples * sizeof(int));
    }

    // Print initial configuration
    printf("Nbr of samples: %d -- every %d microSecs (%.2f secs)\n", samples, tdelay, tdelay / 1e6);

    long last_total_user = 0;
    long last_total_user_low = 0;
    long last_total_sys = 0;
    long last_total_idle = 0;

    for (int i = 0; i < samples; i++) {
        printf("\033c"); // Clear screen
        int col_index = i % samples;
        int pipes[3][2];
        for (int i = 0; i < 3; i++){
            if (pipe(pipes[i])==-1){
                perror("pipe failed");
                cleanup(cpu_history, mem_history);
                exit(1);
            }
        }

        if (memory) {            
            int result = fork();
            if (result < 0){
                perror("fork failed");
                cleanup(cpu_history, mem_history);
                exit(1);
            }
            else if (result == 0){
                close(pipes[0][0]);
                struct sysinfo info;
                sysinfo(&info);
                double total_memory = info.totalram;
                double total_memory_gb = total_memory / 1e9;
                double used_memory = (total_memory - info.freeram) / 1e9;
                double results[3];
                results[0] = 9 - (int)((used_memory / total_memory_gb) * 9);
                results[1] = total_memory_gb;
                results[2] = used_memory;
                int used_blocks_mem = (int)((used_memory / total_memory_gb) * 9);
                write(pipes[0][1], results, sizeof(results));
                close(pipes[0][1]);
                exit(0);
            }
            else{
                // double results[3];
                close(pipes[0][1]);
                // read(pipe1[0], results, sizeof(results));
                // printMemoryUsage(samples, col_index, mem_history, (int)results[0], results[1], results[2]);
                // close(pipe1[0]);
            }
        }

        if (cpu) {
            int result = fork();
            if (result < 0){
                perror("fork failed");
                cleanup(cpu_history, mem_history);
                exit(1);
            }
            else if (result == 0){
                close(pipes[1][0]);
                double cpu_usage = getCpuUsage(&last_total_user, &last_total_user_low, &last_total_sys, &last_total_idle);
                write(pipes[1][1], &cpu_usage, sizeof(cpu_usage));
                close(pipes[1][1]);
                exit(0);
            
            }
            else{
                // double cpu_usage;
                close(pipes[1][1]);
                // read(pipe2[0], cpu_usage, sizeof(cpu_usage));
                // printCpuUsage(samples, col_index, cpu_history, cpu_usage);
                // close(pipe1[0]);
            }
        }

        for (int i = 0; i < 2; i++) {
            wait(NULL);
        }

        double results[3];
        read(pipes[0][0], results, sizeof(results));
        printMemoryUsage(samples, col_index, mem_history, (int)results[0], results[1], results[2]);
        close(pipes[0][0]);

        double cpu_usage;
        read(pipes[1][0], &cpu_usage, sizeof(cpu_usage));
        printCpuUsage(samples, col_index, cpu_history, cpu_usage);
        close(pipes[1][0]);

        if (cores) {
            FILE *file = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r");
            if (file == NULL) {
                fprintf(stderr, "Error: Failed to open CPU frequency file\n");
                cleanup(cpu_history, mem_history);
                exit(1);
            }

            unsigned long khz = 0;
            if (fscanf(file, "%lu", &khz) != 1) {
                fprintf(stderr, "Error: Failed to read CPU frequency\n");
                fclose(file);
                cleanup(cpu_history, mem_history);
                exit(1);
            }
            fclose(file);

            double ghz = khz / 1e6;
            printf("v Number of Cores: %d @ %.2f GHz\n", num_cores, ghz);
            printCoreDiagram(num_cores);
        }

        usleep(tdelay);
    }

    // Clean up dynamically allocated memory
    cleanup(cpu_history, mem_history);
    return 0;
}