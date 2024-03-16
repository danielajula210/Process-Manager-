#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <dirent.h>
#include <sys/resource.h>

#define BUFFER_SIZE 1024

void tag_process(int pid, double cpu_usage, double memory_usage, double io_usage) {
    printf("Process ID: %d\n", pid);
    printf("CPU Usage: %.2f%%\n", cpu_usage);
    printf("Memory Usage: %.2f%%\n", memory_usage);
    printf("I/O Utilization: %.2f\n", io_usage);

    if (cpu_usage > memory_usage) {
        printf("Tag: CPU Bound\n");
    } else {
        printf("Tag: I/O Bound\n");
    }

    printf("------------------------\n");
}

int main() {
    FILE *file = fopen("/proc/stat", "r");
    if (!file) {
        perror("Error opening /proc/stat");
        return 1;
    }

    char buffer[BUFFER_SIZE];
    double total_cpu_idle_prev = 0;
    double total_cpu_prev = 0;

    while (1) {
        fseek(file, 0, SEEK_SET);
        if (fgets(buffer, sizeof(buffer), file) == NULL) {
            perror("Error reading /proc/stat");
            break;
        }

        sscanf(buffer, "cpu %*u %*u %*u %*u %lf %*u %*u %*u %*u", &total_cpu_idle_prev);
        total_cpu_prev = total_cpu_idle_prev + 100;

        DIR *proc_dir = opendir("/proc");
        if (!proc_dir) {
            perror("Error opening /proc directory");
            fclose(file);
            return 1;
        }

        struct dirent *entry;
        while ((entry = readdir(proc_dir)) != NULL) {
            if (entry->d_type == DT_DIR) {
                int pid = atoi(entry->d_name);

                if (pid > 0 && pid != getpid() && pid != 1) {
                    char proc_path[50];
                    sprintf(proc_path, "/proc/%d", pid);
                    struct stat statbuf;
                    if (stat(proc_path, &statbuf) == 0) {
                        char stat_path[50];
                        sprintf(stat_path, "/proc/%d/stat", pid);

                        FILE *stat_file = fopen(stat_path, "r");
                        if (stat_file) {
                            double utime, stime;  
                            fscanf(stat_file, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %lf %lf", &utime, &stime);
                            fclose(stat_file);

                            double total_cpu_now = utime + stime;
                            double cpu_denominator = total_cpu_now + total_cpu_idle_prev - total_cpu_prev - total_cpu_idle_prev;
                            double cpu_usage = (cpu_denominator != 0) ? 100 * (total_cpu_now - total_cpu_prev) / cpu_denominator : 0;
                            total_cpu_prev = total_cpu_now;

                            char statm_path[50];
                            sprintf(statm_path, "/proc/%d/statm", pid);

                            FILE *statm_file = fopen(statm_path, "r");
                            if (statm_file) {
                                unsigned long size, resident, share;
                                fscanf(statm_file, "%lu %lu %lu", &size, &resident, &share);
                                fclose(statm_file);

				double memory_usage = 100 *((double)resident / (size * sysconf(_SC_PAGE_SIZE)));

                                char io_path[50];
                                sprintf(io_path, "/proc/%d/io", pid);

                                if (access(io_path, F_OK) != -1) {
                                    FILE *io_file = fopen(io_path, "r");
                                    if (io_file) {
                                        unsigned long io_read, io_write;
                                        int result = fscanf(io_file, "rchar: %lu\nwchar: %lu", &io_read, &io_write);
                                        fclose(io_file);

                                        if (result == 2) {
                                            double io_usage = (io_read + io_write) / 1024.0; 
                                            tag_process(pid, cpu_usage, memory_usage, io_usage);
                                        } else {
                                            perror("Error reading /proc/{pid}/io");
                                        }
                                    } else {
                                        perror("Error opening /proc/{pid}/io");
                                    }
                                } else {
                                    tag_process(pid, cpu_usage, memory_usage, 0.0);
                                }
                            } else {
                                perror("Error opening /proc/{pid}/statm");
                            }
                        } else {
                            perror("Error opening /proc/{pid}/stat");
                        }
                    }
                }
            }
        }

        closedir(proc_dir);

        sleep(1);
    }

    fclose(file);

    return 0;
}

