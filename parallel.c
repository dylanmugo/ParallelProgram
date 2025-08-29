// neutron plate sim, counts reflects absorbs and transmita across plate thicknesses

#include <math.h>        // maths libary
#include <stdio.h>       // file io and prints
#include <stdlib.h>      // mallocs and exits and that for general utils
#include <pthread.h>     // threads from pthreads
#include <unistd.h>      // sysconf for core count
#include <sys/time.h>    // timing with gettimeofday

// this macro grabs a timestamp in seconds
#define GET_TIME(now) { \
   struct timeval t; \
   gettimeofday(&t, NULL); \
   now = t.tv_sec + t.tv_usec/1000000.0; \
}

// set above zero if you want prints to the terminal
#define DEBUG_LEVEL 0

//helper for choosing the smaller of two things 
#define min(a,b) (((a)<(b))?(a):(b))

// global sim constants
#define N 1e7                   // number of neutrons per thickness
#define Cc 2.0                  // capture cross-section
#define Cs 4.0                  // scatter cross-section
#define START_THICKNESS 0.10    // start of plate sweep
#define END_THICKNESS 2.0       // end of plate sweep
#define THICKNESS_INCREMENT 0.01// step per thickness sample

// Macros that will come again
#define C (Cc+Cs)               // this is the total cross-section
#define One_over_C (1.0/C)      // inverse thAt saves divides
#define Cc_over_C (Cc/C)        // absorb the probability threshold

// small structure for what each thickness could output 
typedef struct {
  double W;
  unsigned long long int numR;
  unsigned long long int numA;
  unsigned long long int numT;
} output_WRAT_t;

// each thread thickness to run and where to ouput results
typedef struct {
  int thread_id;
  double start_thickness;
  double end_thickness;
  output_WRAT_t *output_arr_ptr;
  unsigned long int output_arr_start;
} thread_args_t;

// to run on each thread it loops its own range of thickness values and runs the neutron bounces for each thread
void *process_neutron_loop(void *arg_struct){
  // grab typed args 
  thread_args_t *args = (thread_args_t *)arg_struct;
  
  // rng state per thread drand48_r  to make sure its safe across threads
  struct drand48_data randBuffer;
  // seed with time and thread id so seeds differ 
  srand48_r(time(NULL)/(args->thread_id+1), &randBuffer);
  
  // each thread writes to its own window in the output array avoids locks
  unsigned long int output_index = args->output_arr_start;
  for(double plate_thickness=args->start_thickness; plate_thickness<args->end_thickness; plate_thickness+=THICKNESS_INCREMENT){
    // reset counts for this thickness
    unsigned long long int numR = 0;
    unsigned long long int numA = 0;
    unsigned long long int numT = 0;
    
    // fire N neutrons at the plate for this thickness to track what happens
    for(int i=1;i<N;i++){
      // set up each neutron at the left face going straight in
      double theta = 0.0;
      double x = 0.0;
      int stillBouncing = 1;
      
      // it will bounce about until it exits left or rightor gets absorbed
      while(stillBouncing){
        //random 0..1 this drives both free path and scatter angle
        double randomNum;
        drand48_r(&randBuffer, &randomNum);
        
        // sample the path length from exponential
        double L = -One_over_C*log(randomNum);
        
        // move along x by the horizontal component
        x += L*cos(theta);
        
        // left of zero means it bounced back out the entry side this will be reflected
        if(x<0){
          numR++;
          stillBouncing = 0;
        }
        // beyond plate thickness means it made it through to the far side will be transmitted
        else if(x>=plate_thickness){
          numT++;
          stillBouncing = 0;
        }
        // still inside random draw under Cc/C means it absorbed
        else if(randomNum < Cc_over_C){
          numA++;
          stillBouncing = 0;
        }
        // otherwise it scatters somewhere between 0 and pi, we reuse the same draw for angle
        else {
          theta = randomNum * M_PI;
        }
      }
    }

    // keep the results into the shared buffer 
    args->output_arr_ptr[output_index].W = plate_thickness;
    args->output_arr_ptr[output_index].numR = numR;
    args->output_arr_ptr[output_index].numA = numA;
    args->output_arr_ptr[output_index].numT = numT;
    output_index++;
  }
  return 0;
}

// store the buffered results to a file, one line per thickness
void write_WRAT_data(FILE *fp_WRAT, output_WRAT_t *out_array, long out_arr_len){
  for(long i=0;i<out_arr_len;i++){
    fprintf(fp_WRAT, "%lf %lld %lld %lld\n", out_array[i].W, out_array[i].numR, out_array[i].numA, out_array[i].numT);
  }
}

// main part of program
int main(int argc, char **argv){
  double start_time, end_time, elapsed_time;
  GET_TIME(start_time);
  
  // where the WRAT data will be written
  FILE *fp_WRAT;

  // logical cores and workload spilt 
  long num_cores = sysconf(_SC_NPROCESSORS_ONLN);
  if(num_cores == -1){
    perror("\nError: error getting number of cores syscon failed");
    exit(EXIT_FAILURE);
  }
  
  // parse args, first arg is thread count, zero means use all cores 
  if (argc < 2){
    fprintf(stderr, "Usage: %s <number_of_threads> <file_to_write_simulation_results>\n"
                    "NOTE:\n"
                    "  - first parameter (number_of_threads) is required, use 0 to use all available cores (%ld found)\n"
                    "  - second parameter (output file path) is optional\n", argv[0], num_cores);
    exit(EXIT_FAILURE);
  }
  
  if(argc>=2){
    num_cores = atoi(argv[1])>0 ? atoi(argv[1]) : num_cores; 
    if(argc>=3){
      fp_WRAT = fopen(argv[2], "w"); // write to the chosen file, will overwrite
    } else {
      fp_WRAT = fopen("data/WRAT_parallel.dat", "w"); // default output to this file
    }
  }
  
  // dividing up the thickness iterations across the threads
  long int num_thickness_iterations = ceil((END_THICKNESS - START_THICKNESS) / THICKNESS_INCREMENT);
  long int load_per_core = ceil((double)num_thickness_iterations / num_cores);
  double thickness_increment_per_core = load_per_core * THICKNESS_INCREMENT;
  
  // stack allocations for threads and the output buffer
  pthread_t threads[num_cores];
  thread_args_t simulation_args[num_cores];
  output_WRAT_t output_WRAT_array[num_thickness_iterations];
  
  // spin up each thread with its own start and end thickness and where to write results in the shared array
  for(int i=0;i<num_cores;i++){
    simulation_args[i].thread_id = i; // handy for seeding and debug prints
    simulation_args[i].start_thickness = START_THICKNESS + i*thickness_increment_per_core; // start of our slice
    simulation_args[i].end_thickness = min(START_THICKNESS + (i+1)*thickness_increment_per_core, END_THICKNESS); // cap at end
    simulation_args[i].output_arr_ptr = output_WRAT_array; // everyone points at the same buffer
    simulation_args[i].output_arr_start = i*load_per_core; // offset into the buffer so we don’t step on each other

    // make the thread if this fails exit code
    if(pthread_create(&threads[i], NULL, process_neutron_loop, (void *)&simulation_args[i]) != 0){
      perror("Error: couldn’t create a new thread failed");
      exit(EXIT_FAILURE);
    }
  }
  
  // wait for all threads to finish.
  for(int i=0;i<num_cores;i++){
    if(pthread_join(threads[i], NULL) != 0){
      perror("Error: couldn’t join a thread back, leaving now");
      exit(EXIT_FAILURE);
    }
  }
  
#if DEBUG_LEVEL > 0
  // if debug level is set print the results to the terminal
  for(int i=0;i<num_thickness_iterations;i++){
    printf("%lf %lld %lld %lld\n", output_WRAT_array[i].W, output_WRAT_array[i].numR, output_WRAT_array[i].numA, output_WRAT_array[i].numT);
  }
#endif

  // write results to file and close it clean and simple format one row per W
  write_WRAT_data(fp_WRAT, output_WRAT_array, num_thickness_iterations);
  fclose(fp_WRAT);

  // timing will be printed in seconds and minutes 
  GET_TIME(end_time);
  elapsed_time = end_time - start_time;
  printf("Elapsed time = %lf seconds = %lf minutes\n\n", elapsed_time, elapsed_time/60.0);

  // all good exit code
  return EXIT_SUCCESS;
}
