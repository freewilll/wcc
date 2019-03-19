#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

int sort_array(long *a, int count) {
    int i, j;
    long t;

    for (i = 0; i < count; i++)
        for (j = i; j < count; j++)
            if (a[i] > a[j]) t = a[i], a[i] = a[j], a[j] = t;
}

int main(int argc, char **argv) {
    int i, j, command_count, run_count, result;
    char **commands, *command;
    struct timeval start, end;
    long secs_used,microseconds;
    long *times;
    long first_time;

    command_count = 2;
    commands = malloc(sizeof(char *) * command_count);
    commands[0] = "./wc4  wc4.c -c -S -o /dev/null";
    commands[1] = "./wc42 wc4.c -c -S -o /dev/null";

    run_count = 31; // Must be odd for the median to work correctly
    times = malloc(sizeof(long) * run_count);

    for (i = 0; i < command_count; i++) {
        command = commands[i];
        printf("Executing %-38s ", command);

        for (j = 0; j < run_count; j++) {
            gettimeofday(&start, NULL);
            result = system(command);
            gettimeofday(&end, NULL);
            if (result) {
                printf("\nBad exit code %d\n", result >> 8);
                exit(1);
            }

            secs_used=(end.tv_sec - start.tv_sec); //avoid overflow by subtracting first
            microseconds= ((secs_used*1000000) + end.tv_usec) - (start.tv_usec);

            times[j] = microseconds;
        }

        sort_array(times, run_count);
        if (i == 0) first_time = times[run_count / 2];
        printf("%ld us %0.2f%%\n", times[run_count / 2], 100.0 * (double) times[run_count / 2] / (double) first_time);
    }
}
