/**
 * Compute the average overhead of applying system changes (power caps or DVFS frequencies).
 *
 * @author Connor Imes
 * @date 2016-07-08
 */
// for strdup, strtok_r, pwrite
#define _POSIX_C_SOURCE 200809L
#include <cpufreq-bindings.h>
#include <inttypes.h>
#include <fcntl.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "copper-eval.h"

#define MAX_CORES 1024
#define MAX_SOCKETS 8

static int powercap_fds[MAX_SOCKETS];
static int core_fds[MAX_CORES];

static void close_fds(const int* fds, uint32_t n) {
  uint32_t i;
  for (i = 0; i < n; i++) {
    if (fds[i] > 0 && close(fds[i])) {
      perror("close");
    }
  }
}

static int open_powercap_fds(uint32_t sockets) {
  uint32_t i;
  char file[128];
  for (i = 0; i < sockets; i++) {
    snprintf(file, sizeof(file), "/sys/class/powercap/intel-rapl:%"PRIu32"/constraint_1_power_limit_uw", i);
    powercap_fds[i] = open(file, O_WRONLY);
    if (powercap_fds[i] <= 0) {
      close_fds(powercap_fds, i);
      perror(file);
      return -1;
    }
  }
  return 0;
}

static uint32_t open_dvfs_core_fds(const char* core_list) {
  char* ptr;
  char* cores_str;
  char* core;
  uint32_t c;
  uint32_t ncores = 0;
  uint32_t i;
  // parse the core list and open DVFS files one at a time
  cores_str = strdup(core_list);
  if (cores_str == NULL) {
    perror("strdup");
    return 0;
  }
  core = strtok_r(cores_str, ",", &ptr);
  while (core != NULL) {
    c = atoi(core);
    if (c >= MAX_CORES) {
      fprintf(stderr, "Core value out of range: %"PRIu32"\n", c);
      for (i = 0; i < ncores; i++) {
        if (core_fds[i] > 0) {
          cpufreq_bindings_file_close(core_fds[i]);
        }
      }
      ncores = 0;
      break;
    }
    if (c >= ncores) {
      ncores = c + 1;
    }
    if ((core_fds[c] = cpufreq_bindings_file_open(c, CPUFREQ_BINDINGS_FILE_SCALING_SETSPEED, -1)) < 0) {
      perror("cpufreq_bindings_file_open");
      for (i = 0; i < ncores; i++) {
        if (core_fds[i] > 0) {
          cpufreq_bindings_file_close(core_fds[i]);
        }
      }
      ncores = 0;
      break;
    }
    core = strtok_r(NULL, ",", &ptr);
  }
  free(cores_str);
  return ncores;
}

static int apply_powercap_direct_sysfs(double powercap, uint32_t sockets, const int* fds) {
  // convert to microwatts and split between sockets
  uint64_t newcap = powercap / sockets * 1000000;
  char data[24];
  snprintf(data, sizeof(data), "%"PRIu64, newcap);
  uint32_t i;
  for (i = 0; i < sockets; i++) {
    if (pwrite(fds[i], data, sizeof(data), 0) <= 0) {
      perror("pwrite");
      return -1;
    }
  }
  return 0;
}

int main(int argc, char** argv) {
  uint64_t i;
  uint32_t j;
  double start, end, total;
  uint64_t iterations;
  uint32_t sockets;
  double power_min, power_max;
  const char* core_list;
  uint64_t frequency_min, frequency_max;
  uint32_t max_cores;
  int ret = 0;

  if (argc == 8) {
    iterations = strtoull(argv[1], NULL, 0);
    sockets = atoi(argv[2]);
    power_min = atof(argv[3]);
    power_max = atof(argv[4]);
    core_list = argv[5];
    frequency_min = strtoull(argv[6], NULL, 0);
    frequency_max = strtoull(argv[7], NULL, 0);
  } else {
    fprintf(stderr, "Usage: %s <iterations> <sockets> <min_power> <max_power> <core_list> <min_frequency> <max_frequency>\n", argv[0]);
    printf("Set sockets to 0 and specify core_list to use DVFS instead\n");
    return 1;
  }

  // measure overhead of execution
  if (sockets == 0) {
    // open core DVFS setspeed files
    max_cores = open_dvfs_core_fds(core_list);
    if (max_cores == 0) {
      close_fds(powercap_fds, MAX_SOCKETS);
      return 1;
    }
    // can't test POET directly, it won't call the apply function unless a new config is computed (which it won't be)
    start = omp_get_wtime();
    for (i = 0; i < iterations; i++) {
      // switch between min and max frequencies for maximum overhead
      for (j = 0; j < max_cores; j++) {
        if (cpufreq_bindings_set_scaling_setspeed(core_fds[j], j, (i % 2 == 0 ? frequency_min : frequency_max)) <= 0) {
          perror("cpufreq_bindings_set_scaling_setspeed");
          ret = 1;
          break;
        }
      }
      if (ret) {
        break;
      }
    }
    end = omp_get_wtime();
    for (j = 0; j < MAX_CORES; j++) {
      if (core_fds[j] > 0) {
        cpufreq_bindings_file_close(core_fds[j]);
      }
    }
  } else {
    // open powercap files
    if (open_powercap_fds(sockets)) {
      return 1;
    }
    start = omp_get_wtime();
    for (i = 0; i < iterations; i++) {
      // switch between min and max powercaps for maximum overhead
      if (apply_powercap_direct_sysfs(i % 2 == 0 ? power_min : power_max, sockets, powercap_fds)) {
        perror("apply_powercap_direct_sysfs");
        ret = 1;
        break;
      }
    }
    end = omp_get_wtime();
    close_fds(powercap_fds, MAX_SOCKETS);
  }
  if (!ret) {
    total = end - start;
    fprintf(stdout, "%f us average iteration time\n", (total * 1000000 / iterations));
  }

  return ret;
}
