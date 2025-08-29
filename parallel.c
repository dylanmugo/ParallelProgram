#include <math.h>       // Provides log(), cos() and M_PI
#include <stdio.h>      // Provides printf(), fprintf(), FILE operations
#include <stdlib.h>     // Provides exit(), malloc()
#include <pthread.h>    // Provides POSIX thread functionality
#include <sys/time.h>   // Provides gettimeofday() for timing

// Macro to capture the current time in seconds (with microsecond precision)
#define GET_TIME(t) \
  { struct timeval tv; gettimeofday(&tv, NULL); t = tv.tv_sec + tv.tv_usec / 1e6; }

// Number of neutrons to fire at the plate per thickness
#define NUM_NEUTRONS    1e5

// Capture and scattering cross-sections
#define CAPTURE_XS      2.0
#define SCATTER_XS      4.0

// Range and increment for plate thickness sweep
#define THICKNESS_START 0.10
#define THICKNESS_END   2.0
#define THICKNESS_STEP  0.01

// Fixed thread count for parallel execution
#define THREAD_COUNT    4

// Derived constants for neutron behaviour
const double TOTAL_XS     = CAPTURE_XS + SCATTER_XS;       // Sum of cross-sections
const double INV_TOTAL_XS = 1.0 / TOTAL_XS;                // Reciprocal for path calculation
const double ABSORB_PROB  = CAPTURE_XS / TOTAL_XS;         // Probability of absorption

// Structure to store results for a single thickness
typedef struct {
  double thickness;               // Current plate thickness
  unsigned long long reflected;   // Number of reflected neutrons
  unsigned long long absorbed;    // Number of absorbed neutrons
  unsigned long long transmitted; // Number of transmitted neutrons
} SimulationResult;

// Parameters passed to each worker thread
typedef struct {
  int                   id;          // Thread identifier
  double                startW;      // Starting thickness for this thread
  double                endW;        // Ending thickness for this thread
  SimulationResult     *results;     // Pointer to shared results array
  unsigned int          offset;      // Index offset in results array
} ThreadParams;

// Worker routine: simulates neutron transport over an assigned thickness range
void *thread_simulation(void *arg) {
  ThreadParams *p = (ThreadParams *)arg;
  struct drand48_data rng;
  srand48_r(time(NULL) + p->id, &rng);  // Seed RNG uniquely per thread

  unsigned int index = p->offset;
  for (double w = p->startW; w < p->endW; w += THICKNESS_STEP) {
    unsigned long long r = 0, a = 0, t = 0;

    for (int i = 0; i < NUM_NEUTRONS; i++) {
      double x = 0.0, theta = 0.0, rnd, L;
      int active = 1;

      while (active) {
        drand48_r(&rng, &rnd);                  // Generate uniform random [0,1)
        L = -INV_TOTAL_XS * log(rnd);           // Sample exponential path length
        x += L * cos(theta);                    // Move neutron horizontally

        if (x < 0) {                            // Reflected out on left
          r++; active = 0;
        }
        else if (x >= w) {                      // Transmitted through right
          t++; active = 0;
        }
        else if (rnd < ABSORB_PROB) {           // Absorbed inside plate
          a++; active = 0;
        }
        else {                                  // Scattered within plate
          theta = rnd * M_PI;                   // New direction in [0,π]
        }
      }
    }

    p->results[index].thickness    = w;
    p->results[index].reflected    = r;
    p->results[index].absorbed     = a;
    p->results[index].transmitted  = t;
    index++;
  }
  return NULL;
}

// Write the assembled simulation results to a file
void write_results(const char *path, SimulationResult *res, int count) {
  FILE *fp = fopen(path, "w");
  if (!fp) {
    perror("Unable to open output file");
    exit(EXIT_FAILURE);
  }
  for (int i = 0; i < count; i++) {
    fprintf(fp, "%lf %llu %llu %llu\n",
            res[i].thickness,
            res[i].reflected,
            res[i].absorbed,
            res[i].transmitted);
  }
  fclose(fp);
}

// Main programme: divides work, launches threads, and writes results
int main(void) {
  double t0, t1;
  GET_TIME(t0);

  int total_steps     = (int)ceil((THICKNESS_END - THICKNESS_START) / THICKNESS_STEP);
  int steps_per_thread= (int)ceil((double)total_steps / THREAD_COUNT);

  SimulationResult results[total_steps];
  pthread_t        threads[THREAD_COUNT];
  ThreadParams     params[THREAD_COUNT];

  for (int i = 0; i < THREAD_COUNT; i++) {
    double startW = THICKNESS_START + i * steps_per_thread * THICKNESS_STEP;
    double endW   = fmin(startW + steps_per_thread * THICKNESS_STEP, THICKNESS_END);

    params[i].id     = i;
    params[i].startW = startW;
    params[i].endW   = endW;
    params[i].results= results;
    params[i].offset = i * steps_per_thread;

    pthread_create(&threads[i], NULL, thread_simulation, &params[i]);
  }

  for (int i = 0; i < THREAD_COUNT; i++) {
    pthread_join(threads[i], NULL);
  }

  write_results("WRAT_parallel.dat", results, total_steps);

  GET_TIME(t1);
  printf("Elapsed time: %.2f seconds\n", t1 - t0);

  return 0;
}
