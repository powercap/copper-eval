/**
 * Run a controller using simulated feedback from a characterized application.
 *
 * @author Connor Imes
 * @date 2017-03-19
 */
// for setenv, strtok_r
#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <heartbeat-acc-pow.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "copper-eval.h"
#include "copper-eval-copper.h"
#include "copper-eval-dsc.h"
#include "copper-eval-poet.h"
#include "copper-eval-psc.h"

#define MAX_RESULTS 24
#define NCOLS_HEARTBEATS_CLASSIC 12
#define NCOLS_HEARTBEATS_SIMPLE 25

#define R_THOUSAND 1000.0
#define R_MILLION  1000000.0
#define R_BILLION  1000000000.0

typedef enum controller_type {
  COPPER,
  POWERCAP_SIMPLE,
  DVFS_POET,
  DVFS_SIMPLE
} controller_type;

typedef enum model_type {
  MODEL_TYPE_DVFS,
  MODEL_TYPE_POWER
} model_type;

typedef struct results_entry {
  model_type type;
  // TODO: We don't actually account for cores - maybe accept a fixed core allocation as cmd line a parameter?
  uint32_t cores;
  uint32_t freq;
  double powercap;
  double perf;
  double pwr;
  heartbeat_acc_pow_record* hbs;
  uint64_t hb_lines;
} results_entry;

static controller_type controller;

static results_entry app_model[MAX_RESULTS];
static uint32_t app_model_size;
// determined by the controller type actually
static model_type app_model_type;

static heartbeat_acc_pow_context hb;
static heartbeat_acc_pow_record* window_buffer;
static int hb_fd;


static void print_model(model_type mtype, results_entry* model, uint32_t n, const char* name, FILE* f) {
  assert(model != NULL);
  assert(name != NULL);
  assert(f != NULL);
  uint32_t i;
  fprintf(f, "%s:\n", name);
  for (i = 0; i < n; i++) {
    switch (mtype) {
      case MODEL_TYPE_DVFS:
        fprintf(f, "%"PRIu32" %"PRIu32" %f %f\n", model[i].cores, model[i].freq, model[i].perf, model[i].pwr);
        break;
      case MODEL_TYPE_POWER:
        fprintf(f, "%"PRIu32" %f %f %f\n", model[i].cores, model[i].powercap, model[i].perf, model[i].pwr);
        break;
      default:
        assert(0);
        errno = EINVAL;
        return;
    }
  }
}

static uint32_t get_model_from_results_file(model_type mtype, results_entry* model, uint32_t max_len, const char* filename) {
  assert(model != NULL);
  assert(filename != NULL);
  uint32_t i;
  int escape = 0;
  char buf[1024];
  int fields;
  FILE* f = fopen(filename, "r");
  if (f == NULL) {
    perror(filename);
    return 0;
  }
  // ignore header line
  fgets(buf, sizeof(buf), f);
  for (i = 0; fgets(buf, sizeof(buf), f) != NULL && i < max_len && !escape; i++) {
    model[i].hbs = NULL;
    model[i].hb_lines = 0;
    switch(mtype) {
      case MODEL_TYPE_DVFS:
        fields = sscanf(buf, "%"PRIu32" %"PRIu32" %lf %lf", &model[i].cores, &model[i].freq, &model[i].perf, &model[i].pwr);
        model[i].powercap = 0;
        break;
      case MODEL_TYPE_POWER:
        fields = sscanf(buf, "%"PRIu32" %lf %lf %lf", &model[i].cores, &model[i].powercap, &model[i].perf, &model[i].pwr);
        model[i].freq = 0;
        break;
      default:
        assert(0);
        escape = 1;
        errno = EINVAL;
        continue;
    }
    if (fields < 4) {
      fprintf(stderr, "%s: syntax error on line %"PRIu32"\n", filename, i + 1);
      escape = 1;
    }
  }
  fclose(f);
  return escape ? 0 : i;
}

static int get_heartbeats_simple_log(FILE* f, const char* filename, results_entry* model, uint64_t iterations) {
  assert(f != NULL);
  assert(filename != NULL);
  assert(model != NULL);
  char line[512];
  heartbeat_acc_pow_record* r;
  for (model->hb_lines = 0;
       model->hb_lines < iterations && fgets(line, sizeof(line), f) != NULL;
       model->hb_lines++) {
    r = &model->hbs[model->hb_lines];
    if (sscanf(line,
               "%"PRIu64" %"PRIu64
               " %"PRIu64" %"PRIu64" %"PRIu64
               " %"PRIu64" %"PRIu64" %"PRIu64" %"PRIu64
               " %lf %lf %lf"
               " %"PRIu64" %"PRIu64" %"PRIu64
               " %lf %lf %lf"
               " %"PRIu64" %"PRIu64" %"PRIu64" %"PRIu64
               " %lf %lf %lf",
               &r->id, &r->user_tag,
               &r->wd.global, &r->wd.window, &r->work,
               &r->td.global, &r->td.window, &r->start_time, &r->end_time,
               &r->perf.global, &r->perf.window, &r->perf.instant,
               &r->ad.global, &r->ad.window, &r->accuracy,
               &r->acc.global, &r->acc.window, &r->acc.instant,
               &r->ed.global, &r->ed.window, &r->start_energy, &r->end_energy,
               &r->pwr.global, &r->pwr.window, &r->pwr.instant) < NCOLS_HEARTBEATS_SIMPLE) {
      fprintf(stderr, "%s: syntax error on line %"PRIu64"\n", filename, model->hb_lines + 1);
      return -1;
    }
  }
  return 0;
}

static int get_heartbeats_classic_log(FILE* f, const char* filename, results_entry* model, uint64_t iterations) {
  assert(f != NULL);
  assert(filename != NULL);
  assert(model != NULL);
  char line[512];
  // the first data line only tells us the start time of the next heartbeat, so we don't record it in the model
  uint64_t start_time, start_energy;
  uint32_t i;
  heartbeat_acc_pow_record* r;
  for (model->hb_lines = 0, i = 0;
       model->hb_lines < iterations && fgets(line, sizeof(line), f) != NULL;
       i++) {
    r = &model->hbs[model->hb_lines];
    if (sscanf(line,
               "%"PRIu64" %"PRIu64" %"PRIu64
               " %lf %lf %lf"
               " %lf %lf %lf"
               " %lf %lf %lf",
               &r->id, &r->user_tag, &r->end_time,
               &r->perf.global, &r->perf.window, &r->perf.instant,
               &r->acc.global, &r->acc.window, &r->acc.instant,
               &r->pwr.global, &r->pwr.window, &r->pwr.instant) < NCOLS_HEARTBEATS_CLASSIC) {
      fprintf(stderr, "%s: syntax error on line %"PRIu64"\n", filename, model->hb_lines + 1);
      return -1;
    }
    // fill in some of the missing fields
    if (i == 0) {
      start_time = r->end_time;
      start_energy = 0;
    } else {
      r->work = 1;
      if (model->hb_lines == 0) {
        r->start_time = start_time;
        r->start_energy = start_energy;
      } else {
        r->start_time = model->hbs[model->hb_lines - 1].end_time;
        r->start_energy = model->hbs[model->hb_lines - 1].end_energy;
      }
      r->end_energy = r->start_energy + (((r->end_time - r->start_time) / R_THOUSAND) * r->pwr.instant);
      // r->wd.global = 0;
      // r->wd.window = 0;
      // r->td.global = 0;
      // r->td.window = 0;
      // r->ad.global = 0;
      // r->ad.window = 0;
      // r->accuracy = 0;
      // r->ed.global = 0;
      // r->ed.window = 0;
      model->hb_lines++;
    }
  }
  return 0;
}

// consumes the str parameter
static uint32_t count_heartbeat_log_cols(char* str) {
  assert(str != NULL);
  uint32_t cols = 0;
  char* ptr;
  char* tok = strtok_r(str, " \t\n", &ptr);
  while (tok != NULL) {
    cols++;
    tok = strtok_r(NULL, " \t\n", &ptr);
  }
  return cols;
}

static void heartbeat_logs_cleanup(results_entry* model, uint32_t n) {
  uint32_t i;
  for (i = 0; i < n; i++) {
    free(model[i].hbs);
    model[i].hbs = NULL;
    model[i].hb_lines = 0;
  }
}

// Expects the directory to contain heartbeat log files in the format "heartbeat_<cores>-<FREQ|POWERCAP>.log"
static int heartbeat_logs_load(model_type mtype, results_entry* model, uint32_t n, uint64_t iterations, const char* dir) {
  assert(model != NULL);
  assert(iterations > 0);
  assert(dir != NULL);
  char filename[4096];
  char line[512];
  FILE* f;
  int ret = 0;
  uint32_t cols;
  uint32_t i;
  for (i = 0; i < n && !ret; i++) {
    // TODO: What if iterations is huge, like way bigger than the actual heartbeat log?
    model[i].hbs = calloc(iterations, sizeof(heartbeat_acc_pow_record));
    if (model[i].hbs == NULL) {
      perror("calloc");
      ret = -1;
      continue;
    }
    switch (mtype) {
      case MODEL_TYPE_DVFS:
        snprintf(filename, sizeof(filename), "%s/heartbeat_%"PRIu32"-%"PRIu32".log", dir, model[i].cores, model[i].freq);
        break;
      case MODEL_TYPE_POWER:
        snprintf(filename, sizeof(filename), "%s/heartbeat_%"PRIu32"-%"PRIu32".log", dir, model[i].cores, (uint32_t) model[i].powercap);
        break;
      default:
        assert(0);
        errno = EINVAL;
        ret = -1;
        continue;
    }
    if ((f = fopen(filename, "r")) == NULL) {
      perror(filename);
      ret = -1;
      continue;
    }
    // decide which heartbeat log type to use by counting the number of columns in the header (first line)
    fgets(line, sizeof(line), f);
    cols = count_heartbeat_log_cols(line);
    if (cols == 0) {
      fprintf(stderr, "Failed to get number of columns in heartbeat log: %s\n", filename);
      ret = -1;
    } else if (cols == NCOLS_HEARTBEATS_CLASSIC) {
      ret = get_heartbeats_classic_log(f, filename, &model[i], iterations);
    } else if (cols == NCOLS_HEARTBEATS_SIMPLE) {
      ret = get_heartbeats_simple_log(f, filename, &model[i], iterations);
    } else {
      fprintf(stderr, "Unknown heartbeat log format with %"PRIu32" columns in heartbeat log: %s\n", cols, filename);
      ret = -1;
    }
    fclose(f);
  }
  if (ret) {
    // cleanup
    heartbeat_logs_cleanup(model, i);
  }
  return ret;
}

static int cmp_u32(const void* a, const void* b) {
  const uint32_t* ua = (const uint32_t*) a;
  const uint32_t* ub = (const uint32_t*) b;
  return (*ua > *ub) - (*ua < *ub);
}

static int cmp_results_entry_powercap(const void* a, const void* b) {
  const results_entry* ua = (const results_entry*) a;
  const results_entry* ub = (const results_entry*) b;
  return (ua->powercap > ub->powercap) - (ua->powercap < ub->powercap);
}

static void dvfs_model_to_sorted_freq_arr(const results_entry* model, uint32_t model_size, uint32_t* freq_arr) {
  assert(model != NULL);
  assert(freq_arr != NULL);
  uint32_t i;
  for (i = 0; i < model_size; i++) {
    freq_arr[i] = model[i].freq;
  }
  qsort(freq_arr, i, sizeof(uint32_t), cmp_u32);
}

static int setup(uint64_t window_size, double perf_target, uint32_t* controller_freqs, uint32_t nfreqs,
                 uint32_t freq_start, double pmin, double pmax, double pstart) {
  assert(window_size > 0);
  assert(controller_freqs != NULL);
  int ret;
  hb_fd = open("heartbeat.log", O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (hb_fd < 0) {
    perror("open:heartbeat.log");
    return -1;
  }
  window_buffer = malloc(window_size * sizeof(heartbeat_acc_pow_record));
  if (window_buffer == NULL) {
    perror("malloc");
    close(hb_fd);
    return -1;
  }
  if (heartbeat_acc_pow_init(&hb, window_size, window_buffer, hb_fd, NULL)) {
    perror("heartbeat_acc_pow_init");
    close(hb_fd);
    free(window_buffer);
    return -1;
  }
  hb_acc_pow_log_header(hb_fd);
  switch (controller) {
    case COPPER:
      if ((ret = copper_eval_copper_init(perf_target, pmin, pmax, pstart))) {
        perror("copper_eval_copper_init");
      }
      break;
    case POWERCAP_SIMPLE:
      if ((ret = copper_eval_psc_init(perf_target, pmin, pmax, pstart))) {
        perror("copper_eval_psc_init");
      }
      break;
    case DVFS_SIMPLE:
      if ((ret = copper_eval_dsc_init(perf_target, controller_freqs, nfreqs, freq_start))) {
        perror("copper_eval_dsc_init");
      }
      break;
    case DVFS_POET:
      // TODO: Configure poet with the system model specified
      fprintf(stderr, "Warning: Controller model ignored for POET - ensure that POET config files are set\n");
      // disable actuation (POET makes direct calls to actuator)
      setenv(ENV_POWERCAP_DISABLE, "1", 0);
      if ((ret = copper_eval_poet_init(perf_target, window_size, freq_start))) {
        perror("copper_eval_poet_init");
      }
      break;
    default:
      assert(0);
      errno = EINVAL;
      ret = -1;
      break;
  }
  if (ret) {
    close(hb_fd);
    free(window_buffer);
  }
  return ret;
}

static int teardown(void) {
  int ret;
  switch (controller) {
    case COPPER:
      if ((ret = copper_eval_copper_finish())) {
        perror("copper_eval_copper_finish");
      }
      break;
    case POWERCAP_SIMPLE:
      if ((ret = copper_eval_psc_finish())) {
        perror("copper_eval_psc_finish");
      }
      break;
    case DVFS_SIMPLE:
      if ((ret = copper_eval_dsc_finish())) {
        perror("copper_eval_dsc_finish");
      }
      break;
    case DVFS_POET:
      copper_eval_poet_finish();
      ret = 0;
      break;
    default:
      assert(0);
      errno = EINVAL;
      ret = -1;
      break;
  }
  close(hb_fd);
  free(window_buffer);
  return ret;
}

static uint32_t get_curr_freq(void) {
  switch (controller) {
    case DVFS_SIMPLE:
      return copper_eval_dsc_get_curr_freq();
    case DVFS_POET:
      return copper_eval_poet_get_curr_freq();
    default:
      assert(0);
      errno = EINVAL;
      break;
  }
  return 0;
}

static double get_curr_powercap(void) {
  switch (controller) {
    case POWERCAP_SIMPLE:
      return copper_eval_psc_get_curr_powercap();
    case COPPER:
      return copper_eval_copper_get_curr_powercap();
    default:
      assert(0);
      errno = EINVAL;
      break;
  }
  return 0;
}

static uint32_t freq_to_idx(const results_entry* model, uint32_t model_size, uint32_t freq) {
  assert(model != NULL);
  assert(model_size > 0);
  assert(freq > 0);
  uint32_t i;
  // Don't binary search:
  // 1) the model is not be sorted by frequency
  // 2) the array is so small, we don't need a sorted copy anyway
  for (i = 0; i < model_size; i++) {
    if (model[i].freq == freq) {
      break;
    }
  }
  assert(i < model_size);
  return i;
}

static void powercap_to_idxs(double powercap, uint32_t* idx_l, uint32_t* idx_u) {
  assert(powercap > 0);
  assert(idx_l != NULL);
  assert(idx_u != NULL);
  assert(app_model_size > 0);
  uint32_t l, u, i;
  // model is sorted by powercap - do binary search
  if (powercap <= app_model[0].powercap) {
    l = 0;
    u = 0;
  } else if (powercap >= app_model[app_model_size - 1].powercap) {
    l = app_model_size - 1;
    u = app_model_size - 1;
  } else {
    // binary search for lower/upper indices
    for (l = 0, u = app_model_size - 1; u - l > 1; ) {
      i = (u + l) / 2;
      if (app_model[i].powercap <= powercap) {
        l = i;
      } else {
        u = i;
      }
    }
  }
  assert(l < app_model_size);
  assert(u < app_model_size);
  assert(l <= u);
  *idx_l = l;
  *idx_u = u;
}

// assertions in the following functions ensure that heartbeat data isn't corrupt and that we loaded it properly

static uint64_t get_work_completed(uint32_t idx, uint64_t iteration) {
  assert(idx < app_model_size);
  uint64_t work;
  if (iteration < app_model[idx].hb_lines) {
    // read direct from heartbeat log
    work = app_model[idx].hbs[iteration].work;
  } else {
    // work is already accounted for in performance data
    work = 1;
  }
  assert(work > 0);
  return work;
}

static uint64_t get_elapsed_ns_dvfs(uint32_t idx, uint64_t iteration) {
  assert(idx < app_model_size);
  uint64_t ns, start_time, end_time;
  if (iteration < app_model[idx].hb_lines) {
    // read direct from heartbeat log
    start_time = app_model[idx].hbs[iteration].start_time;
    end_time = app_model[idx].hbs[iteration].end_time;
    assert(end_time > start_time);
    ns = end_time - start_time;
  } else {
    // use average behavior
    assert(app_model[idx].perf > 0);
    ns = (uint64_t) (R_BILLION / app_model[idx].perf);
  }
  assert(ns > 0);
  return ns;
}

static uint64_t get_elapsed_uj_dvfs(uint32_t idx, uint64_t iteration, uint64_t ns) {
  assert(idx < app_model_size);
  uint64_t uj, start_energy, end_energy;
  if (iteration < app_model[idx].hb_lines) {
    // read direct from heartbeat log
    end_energy = app_model[idx].hbs[iteration].end_energy;
    start_energy = app_model[idx].hbs[iteration].start_energy;
    assert(end_energy >= start_energy);
    uj = end_energy - start_energy;
    // check for 0 watts - happens when a heartbeat comes too fast for a reliable energy reading
    // this is a hack - fall back on window power
    if (uj == 0) {
      uj = (uint64_t) (app_model[idx].hbs[iteration].pwr.window * ((double) ns / R_THOUSAND));
    }
  } else {
    // use average behavior
    assert(app_model[idx].pwr >= 0);
    uj = (uint64_t) (app_model[idx].pwr * ((double) ns / R_THOUSAND));
  }
  // uj might be 0
  if (uj == 0) {
    fprintf(stderr, "Warning: Got 0 uj for iteration %"PRIu64"\n", iteration);
  }
  return uj;
}

static uint64_t get_elapsed_ns_powercap(uint32_t l, uint32_t u, uint64_t iteration, double powercap) {
  assert(l < app_model_size);
  assert(u < app_model_size);
  double perf, slope, perf_l, perf_u, powercap_l, powercap_u;
  // performance accounts for work, so we need to un-account for it
  const uint64_t work = get_work_completed(u, iteration);
  if (iteration < app_model[l].hb_lines && iteration < app_model[u].hb_lines) {
    // read direct from heartbeat log
    perf_l = app_model[l].hbs[iteration].perf.instant / work;
    perf_u = app_model[u].hbs[iteration].perf.instant / work;
  } else {
    // use average behavior
    perf_l = app_model[l].perf / work;
    perf_u = app_model[u].perf / work;
  }
  assert(perf_l > 0);
  assert(perf_u > 0);
  // note: it doesn't necessary hold that perf_u > perf_l
  if (l == u) {
    perf = perf_l;
  } else {
    // interpolate
    powercap_l = app_model[l].powercap;
    powercap_u = app_model[u].powercap;
    assert(powercap_l > 0);
    assert(powercap_u > 0);
    assert(powercap_u > powercap_l);
    assert(powercap_l <= powercap);
    assert(powercap_u >= powercap);
    slope = (perf_u - perf_l) / (powercap_u - powercap_l);
    perf = perf_l + (slope * (powercap - powercap_l));
  }
  assert(perf > 0);
  return (uint64_t) (R_BILLION / perf);
}

static uint64_t get_elapsed_uj_powercap(uint32_t l, uint32_t u, uint64_t iteration, double powercap, uint64_t ns) {
  assert(l < app_model_size);
  assert(u < app_model_size);
  double watts, slope, watts_l, watts_u, powercap_l, powercap_u;
  uint64_t uj;
  if (iteration < app_model[l].hb_lines && iteration < app_model[u].hb_lines) {
    // read direct from heartbeat log
    watts_l = app_model[l].hbs[iteration].pwr.instant;
    watts_u = app_model[u].hbs[iteration].pwr.instant;
    // check for 0 watts - happens when a heartbeat comes too fast for a reliable energy reading
    // this is a hack - fall back on window power
    if (watts_l <= 0) {
      watts_l = app_model[l].hbs[iteration].pwr.window;
    }
    if (watts_u <= 0) {
      watts_u = app_model[u].hbs[iteration].pwr.window;
    }
  } else {
    // use average behavior
    watts_l = app_model[l].pwr;
    watts_u = app_model[u].pwr;
  }
  assert(watts_l >= 0);
  assert(watts_u >= 0);
  if (l == u) {
    watts = watts_l;
  } else {
    // interpolate
    powercap_l = app_model[l].powercap;
    powercap_u = app_model[u].powercap;
    assert(powercap_l > 0);
    assert(powercap_u > 0);
    assert(powercap_u > powercap_l);
    assert(powercap_l <= powercap);
    assert(powercap_u >= powercap);
    slope = (watts_u - watts_l) / (powercap_u - powercap_l);
    watts = watts_l + (slope * (powercap - powercap_l));
  }
  assert(watts >= 0);
  uj = (uint64_t) (watts * ((double) ns / R_THOUSAND));
  if (uj == 0) {
    fprintf(stderr, "Warning: Got 0 uj for iteration %"PRIu64"\n", iteration);
  }
  return uj;
}

static void work(uint64_t iterations, uint64_t window_size) {
  assert(window_size > 0);
  static const uint64_t I_ACCURACY = 0;
  uint64_t ns_start;
  uint64_t ns_end = 0;
  uint64_t ns_elapsed;
  uint64_t uj_start;
  uint64_t uj_end = 0;
  uint32_t idx_l, idx_u;
  uint32_t curr_freq;
  double curr_powercap;
  double i_perf, i_pwr;
  uint64_t i_work;
  uint64_t i;
  for (i = 0; i < iterations; i++) {
    ns_start = ns_end;
    uj_start = uj_end;
    switch (controller) {
      case COPPER:
      case POWERCAP_SIMPLE:
        curr_powercap = get_curr_powercap();
        powercap_to_idxs(curr_powercap, &idx_l, &idx_u);
        ns_elapsed = get_elapsed_ns_powercap(idx_l, idx_u, i, curr_powercap);
        ns_end = ns_start + ns_elapsed;
        uj_end = uj_start + get_elapsed_uj_powercap(idx_l, idx_u, i, curr_powercap, ns_elapsed);
        break;
      case DVFS_SIMPLE:
      case DVFS_POET:
        curr_freq = get_curr_freq();
        idx_u = freq_to_idx(app_model, app_model_size, curr_freq);
        ns_elapsed = get_elapsed_ns_dvfs(idx_u, i);
        ns_end = ns_start + ns_elapsed;
        uj_end = uj_start + get_elapsed_uj_dvfs(idx_u, i, ns_elapsed);
        break;
      default:
        assert(0);
        errno = EINVAL;
        return;
    }
    i_work = get_work_completed(idx_u, i);
    heartbeat_acc_pow(&hb, i, i_work, ns_start, ns_end, I_ACCURACY, uj_start, uj_end);
    i_perf = hb_acc_pow_get_window_perf(&hb);
    switch (controller) {
      case COPPER:
        if (i > 0 && i % window_size == 0) {
          copper_eval_copper_iteration(i, i_perf);
        }
        break;
      case POWERCAP_SIMPLE:
        if (i > 0 && i % window_size == 0) {
          copper_eval_psc_iteration(i, i_perf);
        }
        break;
      case DVFS_SIMPLE:
        if (i > 0 && i % window_size == 0) {
          copper_eval_dsc_iteration(i, i_perf);
        }
        break;
      case DVFS_POET:
        i_pwr = hb_acc_pow_get_window_power(&hb);
        // call on every iteration
        copper_eval_poet_iteration(i, i_perf, i_pwr);
        break;
      default:
        assert(0);
        errno = EINVAL;
        return;
    }
    printf(".");
    if (i > 0 && i % window_size == 0) {
      printf("\nFinished window period at iteration %"PRIu64"\n", i);
    }
  }
  printf("\n");
}

static int set_controller(const char* name) {
  assert(name != NULL);
  #define STR_COPPER "COPPER"
  #define STR_PSC "PSC"
  #define STR_POET "POET"
  #define STR_DSC "DSC"
  if (strncmp(name, STR_COPPER, sizeof(STR_COPPER)) == 0) {
    controller = COPPER;
  } else if (strncmp(name, STR_PSC, sizeof(STR_PSC)) == 0) {
    controller = POWERCAP_SIMPLE;
  } else if (strncmp(name, STR_POET, sizeof(STR_POET)) == 0) {
    controller = DVFS_POET;
  } else if (strncmp(name, STR_DSC, sizeof(STR_DSC)) == 0) {
    controller = DVFS_SIMPLE;
  } else {
    fprintf(stderr, "Unknown controller type: %s\n", name);
    return -1;
  }
  return 0;
}

static const char* prog;
static const char short_options[] = "C:c:a:H:f:p:P:s:i:w:t:";
static const struct option long_options[] = {
  {"controller",  required_argument, NULL, 'C'},
  {"control-rf",  required_argument, NULL, 'c'},
  {"app-rf",      required_argument, NULL, 'a'},
  {"hb-dir",      required_argument, NULL, 'H'},
  {"freq",        required_argument, NULL, 'f'},
  {"pmin",        required_argument, NULL, 'p'},
  {"pmax",        required_argument, NULL, 'P'},
  {"pstart",      required_argument, NULL, 's'},
  {"iterations",  required_argument, NULL, 'i'},
  {"window",      required_argument, NULL, 'w'},
  {"target",      required_argument, NULL, 't'},
  {0, 0, 0, 0}
};

static void print_usage(int exit_code) {
  fprintf(exit_code ? stderr : stdout,
          "Usage:\n"
          "  %s [options]\n\n"
          "  -C, --controller=NAME  One of COPPER|PSC|POET|DSC\n"
          "  -c, --control-rf=FILE  DVFS Results file for the controller model\n"
          "  -a, --app-rf=FILE      DVFS or POWER results file for the app model\n"
          "  -H, --hb-dir=DIR       [Optional] Dir with heartbeat characterization logs\n"
          "  -f, --freq=KHz         Starting DVFS frequency in KHz\n"
          "  -p, --pmin=WATTS       Minimum power > 0\n"
          "  -P, --pmax=WATTS       Maximum power > minumum power\n"
          "  -s, --pstart=WATTS     Starting power >= minimum power and <= maximum power\n"
          "  -i, --iterations=N     Number of iterations to simulate\n"
          "  -w, --window=N         Size of the window period\n"
          "  -t, --target=N         Target performance\n"
          "  -h, --help             Print this message and exit\n\n"
          "The controller model must always be from a DVFS characterization.\n"
          "For COPPER|PSC controllers, app results file must be a POWER characterization.\n\n"
          "If using heartbeat logs to drive the simulation, once the max number of iterations "
          "is reached, the simulator returns to using the results file to determine app behavior.\n\n",
          prog);
  exit(exit_code);
}

int main(int argc, char** argv) {
  char c;

  int has_controller = 0;
  results_entry controller_model[MAX_RESULTS];
  uint32_t controller_freqs[MAX_RESULTS];
  uint32_t controller_model_size = 0;
  int has_controller_model = 0;
  const char* app_model_filename = NULL;
  const char* hb_dir = NULL;
  uint32_t freq_start = 0;
  double pmin = 0;
  double pmax = 0;
  double pstart = 0;
  uint64_t iterations = 0;
  uint64_t window_size = 0;
  double perf_target = 0;

  prog = argv[0];
  while ((c = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
    switch (c) {
      case 'h':
        print_usage(0);
        break;
      case 'C':
        if (set_controller(optarg)) {
          print_usage(1);
        }
        has_controller = 1;
        break;
      case 'c':
        // always DVFS type
        if ((controller_model_size = get_model_from_results_file(MODEL_TYPE_DVFS, controller_model, MAX_RESULTS, optarg)) == 0) {
          return 1;
        }
        print_model(MODEL_TYPE_DVFS, controller_model, controller_model_size, "Controller model", stdout);
        dvfs_model_to_sorted_freq_arr(controller_model, controller_model_size, controller_freqs);
        has_controller_model = 1;
        break;
      case 'H':
        hb_dir = optarg;
        break;
      case 'a':
        // can't open now - first must ensure that we get the controller type from the cmd line
        app_model_filename = optarg;
        break;
      case 'f':
        freq_start = strtoul(optarg, NULL, 0);
        break;
      case 'p':
        pmin = atof(optarg);
        break;
      case 'P':
        pmax = atof(optarg);
        break;
      case 's':
        pstart = atof(optarg);
        break;
      case 'i':
        iterations = strtoull(optarg, NULL, 0);
        break;
      case 'w':
        window_size = strtoull(optarg, NULL, 0);
        break;
      case 't':
        perf_target = atof(optarg);
        break;
      case '?':
      default:
        print_usage(1);
    }
  }

  if (!has_controller || app_model_filename == NULL || !has_controller_model || !freq_start ||
      pmin <= 0 || pmax <= pmin || pstart < pmin || pstart > pmax || !iterations || !window_size || perf_target <= 0) {
    print_usage(1);
  }

  switch (controller) {
    case COPPER:
    case POWERCAP_SIMPLE:
      app_model_type = MODEL_TYPE_POWER;
      break;
    case DVFS_POET:
    case DVFS_SIMPLE:
      app_model_type = MODEL_TYPE_DVFS;
      break;
    default:
      assert(0);
      return 1;
  }

  if ((app_model_size = get_model_from_results_file(app_model_type, app_model, MAX_RESULTS, app_model_filename)) == 0) {
    return 1;
  }
  qsort(app_model, app_model_size, sizeof(results_entry), cmp_results_entry_powercap);
  print_model(app_model_type, app_model, app_model_size, "Application model", stdout);

  if (hb_dir != NULL) {
    if (heartbeat_logs_load(app_model_type, app_model, app_model_size, iterations, hb_dir)) {
      return 1;
    }
  }
  if (setup(window_size, perf_target, controller_freqs, controller_model_size, freq_start,
            pmin, pmax, pstart)) {
    heartbeat_logs_cleanup(app_model, app_model_size);
    return 1;
  }

  work(iterations, window_size);

  heartbeat_logs_cleanup(app_model, app_model_size);
  if (teardown()) {
    return 1;
  }

  return 0;
}
