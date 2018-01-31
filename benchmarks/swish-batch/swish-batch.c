#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <copper-eval.h>

static int counter = 0;
void swish_query(const char* search_binary, const char* index_file, const char* queries_file, int m) {
  char word[140];
  int count = 0;
  FILE* f;
  FILE* cret;
  char search_query[500];
  int results = 0;
  char results_string[8];
  float accuracy;

  f = fopen(queries_file, "r");
  if (f == NULL) {
    perror(queries_file);
    exit(1);
  }

  while (fscanf(f, "%s", word) != EOF) {
    snprintf(search_query, sizeof(search_query), "%s -m %d -i %s %s | awk '/results/ {print $3}' > /dev/null", search_binary, m, index_file, word);
    cret = popen(search_query, "r");
    if (cret == NULL) {
      perror(search_query);
      exit(1);
    }
    while (fgets(results_string, sizeof(results_string), cret) != NULL) {
      results = atoi(results_string);
    }
    pclose(cret);

    if (results == 0 || results < m) {
      accuracy = 1.0;
    } else {
      accuracy = (2.0*1.0* ((float) m)/((float) results))/ (1.0 + (((float) m)/((float) results)));
    }
    printf("%d %s %d %f\n", count, word, results, accuracy);
    int old_counter = __sync_fetch_and_add(&counter, 1);
    copper_eval_iteration(old_counter, 0);
  }

  fclose(f);
}

int main(int argc, char** argv) {
  if (argc < 5) {
    fprintf(stderr, "Usage: %s <search_binary> <index_file> <queries_file> <max_results> <iterations>\n", argv[0]);
    exit(1);
  }
  const int m = atoi(argv[4]);
  const int loops = atoi(argv[5]);
  int i;
  if (copper_eval_init()) {
    perror("copper_eval_init");
    return 1;
  }

#pragma omp parallel for
  for (i = 0; i < loops; i++) {
    // pointers must be inside loop
    const char* search_binary = argv[1];
    const char* index_file = argv[2];
    const char* queries_file = argv[3];
    swish_query(search_binary, index_file, queries_file, m);
  }

  if (copper_eval_finish()) {
    perror("copper_eval_finish");
  }

  return 0;
}
