#include "myMonitoringTool.h"

void ignore(int code){
    ///_|> descry: handler for sigtstp
    ///_|> arg_i: int code, the signal number
    ///_|> returning: nothing   

    printf("\nStop ignored (code=%d)\n", code);
}

void quit(int code){
    ///_|> descry: handler for sigint
    ///_|> arg_i: int code, the signal number
    ///_|> returning: nothing

    printf("\nInterrupt (code=%d) Do you want to quit? Input 'y' or 'n'\n", code);
    char response = getchar();
    if (response == 'y') {
        printf("\nQuitting\n");
        exit(0);
    } else {
        printf("\nContinuing\n");
    }
}

int isNumber(char* input) {
    ///_|> descry: checks if a string represents a valid number
    ///_|> arg_i: char* input, the text to be checked
    ///_|> returning: 1 for true, 0 for false

    int length = strlen(input);
    for (int i = 0; i < length; i++) {
        if (!isdigit(input[i])) {
            return 0;
        }
    }
    return 1;
}

int getCoreCount() {
    ///_|> descry: retrieves the number of cpu cores by reading /proc/cpuinfo
    ///_|> returning: number of cores

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

double getCpuUsage(long* last_total_user, long* last_total_user_low, long* last_total_sys, long* last_total_idle, long* last_total_io, long* last_total_irq, long* last_total_soft) {
    ///_|> descry: calculates the CPU usage by reading /proc/stat
    ///_|> arg_i: long* last_total_user, previous time spent processing normally in user mode
    ///_|> arg_ii: long* last_total_user_low, previous time spent with niced processes in user mode
    ///_|> arg_iii: long* last_total_sys, previous time spent running in kernel mode
    ///_|> arg_iv: long* last_total_idle, previous time spent doing nothing
    ///_|> arg_v: long* last_total_io, previous time spent waiting for I/O
    ///_|> arg_vi: long* last_total_irq, previous time spent serving hardware interrupts
    ///_|> arg_vii: long* last_total_soft, previous time spent serving software interrupts
    ///_|> returning: the cpu usage as a percentage

    FILE *file = fopen("/proc/stat", "r");
    if (file == NULL) {
        fprintf(stderr, "Error: Failed to open /proc/stat\n");
        exit(1);
    }

    char line[128];
    fgets(line, sizeof(line), file);
    fclose(file);

    long total_user, total_user_low, total_sys, total_idle, total_io, total_irq, total_soft;
    sscanf(line, "cpu  %ld %ld %ld %ld %ld %ld %ld", &total_user, &total_user_low, &total_sys, &total_idle, &total_io, &total_irq, &total_soft);

    long total = (total_user - *last_total_user) + (total_user_low - *last_total_user_low) +
                 (total_sys - *last_total_sys) + (total_io - *last_total_io) + (total_irq - *last_total_irq) +
                 (total_soft - *last_total_soft);
    long total_time = total + (total_idle - *last_total_idle);

    double cpu_usage = (double)total / total_time * 100;
    *last_total_user = total_user;
    *last_total_user_low = total_user_low;
    *last_total_sys = total_sys;
    *last_total_idle = total_idle;
    *last_total_io = total_io;
    *last_total_irq = total_irq;
    *last_total_soft = total_soft;

    return cpu_usage;
}

void parseArguments(int argc, char **argv, int *samples, int *tdelay, int *memory, int *cpu, int *cores) {
    ///_|> descry: parses command-line arguments and updates the corresponding parameters
    ///_|> arg_i: int argc, the number of command-line arguments
    ///_|> arg_ii: char **argv, the array of command-line arguments
    ///_|> arg_iii: int *samples, pointer to the number of samples
    ///_|> arg_iv: int *tdelay, pointer to the time delay between samples
    ///_|> arg_v: lint *memory, pointer to the memory flag
    ///_|> arg_vi: int *cpu, pointer to the cpu flag
    ///_|> arg_vii: int *cores, pointer to the cores flag
    ///_|> returning: nothing

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

void printCoreDiagram(int num_cores) {
    ///_|> descry: prints a diagram representing the cpu cores in a grid (4 cores per row)
    ///_|> arg_i: int num_cores, the number of cpu cores
    ///_|> returning: nothing

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

void printMemoryUsage(int samples, int col_index, int **mem_history, int pos, double total_memory, double used_memory) {
    ///_|> descry: prints memory usage information and updates the memory history
    ///_|> arg_i: int samples, the number of samples
    ///_|> arg_ii: int col_index, the current column index in the history array
    ///_|> arg_iii: int **mem_history, the memory history array
    ///_|> arg_iv: int pos, the row index to place the 1 in the memory history array
    ///_|> arg_v: double total_memory, the total system memory
    ///_|> arg_vi: double used_memory, the total used system memory
    ///_|> returning: nothing

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

void printCpuUsage(int samples, int col_index, int **cpu_history, double cpu_usage) {
    ///_|> descry: prints cpu usage information and updates the cpu history
    ///_|> arg_i: int samples, the number of samples
    ///_|> arg_ii: int col_index, the current column index in the history array
    ///_|> arg_iii: int **cpu_history, the cpu history array
    ///_|> arg_iv: double cpu_usage, the cpu usage as a percentage
    ///_|> returning: nothing

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

void cleanup(int **cpu_history, int **mem_history) {
    ///_|> descry: frees dynamically allocated memory
    ///_|> arg_i: int **cpu_history, the cpu history array
    ///_|> arg_ii: int **mem_history, the memory history array
    ///_|> returning: nothing

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

    long last_total_user = 0;
    long last_total_user_low = 0;
    long last_total_sys = 0;
    long last_total_idle = 0;
    long last_total_io = 0;
    long last_total_irq = 0;
    long last_total_soft = 0;

    for (int i = 0; i < samples; i++) {
        printf("\033c"); // Clear screen
        printf("Nbr of samples: %d -- every %d microSecs (%.2f secs)\n", samples, tdelay, tdelay / 1e6); // Print initial configuration
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
                write(pipes[0][1], results, sizeof(results));
                close(pipes[0][1]);
                exit(0);
            }
            else{
                close(pipes[0][1]);
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
                double cpu_usage = getCpuUsage(&last_total_user, &last_total_user_low, &last_total_sys, &last_total_idle, &last_total_io, &last_total_irq, &last_total_soft);
                write(pipes[1][1], &cpu_usage, sizeof(cpu_usage));
                close(pipes[1][1]);
                exit(0);
            }
            else{
                close(pipes[1][1]);
            }
        }

        if (cores) {
            int result = fork();
            if (result < 0){
                perror("fork failed");
                cleanup(cpu_history, mem_history);
                exit(1);
            }
            else if (result == 0){
                close(pipes[2][0]);
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
                write(pipes[2][1], &ghz, sizeof(ghz));
                close(pipes[2][1]);
                exit(0);
            }
            else{
                close(pipes[2][1]);
            }
        }

        for (int i = 0; i < 3; i++) {
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

        double ghz;
        read(pipes[2][0], &ghz, sizeof(ghz));
        printf("v Number of Cores: %d @ %.2f GHz\n", num_cores, ghz);
        printCoreDiagram(num_cores);
        close(pipes[2][0]);

        usleep(tdelay);
    }

    // Clean up dynamically allocated memory
    cleanup(cpu_history, mem_history);
    return 0;
}