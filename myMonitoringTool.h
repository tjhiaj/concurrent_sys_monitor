#define _DEFAULT_SOURCE

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <sys/resource.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

// Function prototypes
int isNumber(char* input);
int getCoreCount();
double getCpuUsage(long* last_total_user, long* last_total_user_low, long* last_total_sys, long* last_total_idle, long* last_total_io, long* last_total_irq, long* last_total_soft);
void parseArguments(int argc, char **argv, int *samples, int *tdelay, int *memory, int *cpu, int *cores);
void printCoreDiagram(int num_cores);
void printMemoryUsage(int samples, int col_index, int **mem_history, int pos, double total_memory, double used_memory);
void printCpuUsage(int samples, int col_index, int **cpu_history, double cpu_usage);
void cleanup(int **cpu_history, int **mem_history);
