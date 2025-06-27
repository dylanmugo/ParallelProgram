#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include <sys/time.h>
/* The argument now should be a double (not a pointer to a double) */
#define GET_TIME(now) { \
   struct timeval t; \
   gettimeofday(&t, NULL); \
   now = t.tv_sec + t.tv_usec/1000000.0; \
}

// Use positive value here to print the results to the terminal
#define DEBUG_LEVEL 0

/* Simple definition of a minimum value selector */
#define min(a,b) (((a)<(b))?(a):(b))


/* ====================Definig some global simulation constants==================== */
#define N 1e7

#define Cc 2.0
#define Cs 4.0

#define START_THICKNESS 0.10
#define END_THICKNESS 2.0
#define THICKNESS_INCREMENT 0.01

#define C (Cc+Cs)
#define One_over_C (1.0/C)
#define Cc_over_C (Cc/C)


/* ====================Custom constructs for holding data (pthread output and input, respectively)==================== */
typedef struct {
  double W;
  unsigned long long int numR;
  unsigned long long int numA;
  unsigned long long int numT;
} output_WRAT_t;


typedef struct {
  int thread_id;
  double start_thickness;
  double end_thickness;
  output_WRAT_t *output_arr_ptr;
  unsigned long int output_arr_start;
} thread_args_t;


/* ====================This function will run for each different thread==================== */
void *process_neutron_loop(void *arg_struct){
  // retrieve the arguments that were passed to this function with void* typecast (below, in main() function)
  thread_args_t *args = (thread_args_t *)arg_struct;
  
  // state variable for random number generator (details of its working principle available in Linux-man page for drand48_r function)
  struct drand48_data randBuffer;
  // seeeding the RNG with (very) unique seeds for each different thread-id (+1 to avoid division by zero)
  srand48_r(time(NULL)/(args->thread_id+1), &randBuffer);
  
  // each thread has a different starting index for filling in the final output array
  // this solution is great for avoiding mutexes that generally slow down parallel programs
  unsigned long int output_index = args->output_arr_start;
  for(double plate_thickness=args->start_thickness; plate_thickness<args->end_thickness; plate_thickness+=THICKNESS_INCREMENT){
    // re-initialie the output variables for each unique plate-thickness value
    unsigned long long int numR = 0;
    unsigned long long int numA = 0;
    unsigned long long int numT = 0;
    
    // run a randomized simulation for each individual neutron
    for(int i=1;i<N;i++){
      // re-initialie starting paramters for the neutron
      double theta = 0.0;
      double x = 0.0;
      int stillBouncing = 1;
      
      // keep running this loop if the neutron is still bouncing around inside the lead-plate
      while(stillBouncing){
        // get random number (in the range 0.0 to 1.0, uniform probability distribution)
        double randomNum;
        drand48_r(&randBuffer, &randomNum);
        
        // length of the path the neutron will travel (determined by RNG)
        double L = -One_over_C*log(randomNum);
        
        // how much path the neutron has traveled (signed and directional) after initial launch into the plate
        x += L*cos(theta);
        
        // if the neutron has gone into the negative x-direction (reflected out of the plate)
        if(x<0){
          numR++;
          stillBouncing = 0;
        }
        // if the neutron has gone outside of the plate in the positive x-direction (transmitted out of the plate)
        else if(x>=plate_thickness){
          numT++;
          stillBouncing = 0;
        }
        // if the neutron is still inside the plate, and due to RNG has been absorbed into the lead-plate
        else if(randomNum < Cc_over_C){
          numA++;
          stillBouncing = 0;
        }
        // if none of the previous conditions were satisfied, and the neutron is bouncing around
        // change the direction it is currently going in
        else {
          theta = randomNum * M_PI;
        }
      }
    }

    // printf("%lf %lld %lld %lld\n", plate_thickness, numR, numA, numT);

    // fill in the output array buffer with simulation results, which will then be displayed and/or written to an external file
    args->output_arr_ptr[output_index].W = plate_thickness;
    args->output_arr_ptr[output_index].numR = numR;
    args->output_arr_ptr[output_index].numA = numA;
    args->output_arr_ptr[output_index].numT = numT;
    output_index++;
  }

  return 0;
}


/* ====================Helper function to write the simulation results to an external file==================== */
void write_WRAT_data(FILE *fp_WRAT, output_WRAT_t *out_array, long out_arr_len){
  for(long i=0;i<out_arr_len;i++){
    fprintf(fp_WRAT, "%lf %lld %lld %lld\n", out_array[i].W, out_array[i].numR, out_array[i].numA, out_array[i].numT);
  }
}


/* ====================Main function==================== */
int main(int argc, char **argv){
  // variables for measuring the execution times of the program
  double start_time, end_time, elapsed_time;
  GET_TIME(start_time);
  
  // pointer to the file where the simulation results are to be written in
  FILE *fp_WRAT;

  // determine the maximum number of CPU cores that can be used, without 
  // involving the OS-Scheduler (which will marginally slow down the program)
  long num_cores = sysconf(_SC_NPROCESSORS_ONLN);
  if(num_cores == -1){
    perror("\nError: Call to sysconf failed, unable to discover the number of processor cores available!");
    exit(EXIT_FAILURE);
  }
  
  // this part is used to parse the command-line inputs for this program 
  if (argc < 2){
    fprintf(stderr, "Usage: %s <number_of_threads> <file_to_write_simulation_results>\n"
                    "NOTE:\n"
                    "  - 1st paramter (number_of_threads) must be provided. Use 0 for maximum number of CPU cores available for use (%ld available).\n"
                    "  - 2nd parameter (file path to write simulation results) is optional\n", argv[0], num_cores);
    exit(EXIT_FAILURE);
  }
  if(argc>=2){
    num_cores = atoi(argv[1])>0 ? atoi(argv[1]) : num_cores;
    if(argc>=3){
      fp_WRAT = fopen(argv[2], "w");
    } else {
      fp_WRAT = fopen("data/WRAT_parallel.dat", "w");
    }
  }
  
  // self-explanatory variables for helping in equally dividing tasks among the threads
  long int num_thickness_iterations = ceil((END_THICKNESS - START_THICKNESS) / THICKNESS_INCREMENT);
  long int load_per_core = ceil((double)num_thickness_iterations / num_cores);
  double thickness_increment_per_core = load_per_core * THICKNESS_INCREMENT;
  
  pthread_t threads[num_cores];
  thread_args_t simulation_args[num_cores];
  output_WRAT_t output_WRAT_array[num_thickness_iterations];
  
  for(int i=0;i<num_cores;i++){
    // create the arguments meant to be supplied to each thread
    simulation_args[i].thread_id = i;
    simulation_args[i].start_thickness = START_THICKNESS + i*thickness_increment_per_core;
    simulation_args[i].end_thickness = min(START_THICKNESS + (i+1)*thickness_increment_per_core, END_THICKNESS);
    simulation_args[i].output_arr_ptr = output_WRAT_array;
    simulation_args[i].output_arr_start = i*load_per_core;
    
    // create the threads
    if(pthread_create(&threads[i], NULL, process_neutron_loop, (void *)&simulation_args[i]) != 0){
      perror("Error: Unable to create a new thread!");
      exit(EXIT_FAILURE);
    }
  }
  
  // wait for all the threads to finish execution
  for(int i=0;i<num_cores;i++){
    if(pthread_join(threads[i], NULL) != 0){
      perror("Error: Unable to join thread!");
      exit(EXIT_FAILURE);
    }
  }
  
#if DEBUG_LEVEL > 0
  for(int i=0;i<num_thickness_iterations;i++){
    printf("%lf %lld %lld %lld\n", output_WRAT_array[i].W, output_WRAT_array[i].numR, output_WRAT_array[i].numA, output_WRAT_array[i].numT);
  }
#endif
  // write the simulation results into the given (or default) external file
  write_WRAT_data(fp_WRAT, output_WRAT_array, num_thickness_iterations);
  fclose(fp_WRAT);

  GET_TIME(end_time);
  elapsed_time = end_time - start_time;
  printf("Elapsed time = %lf seconds = %lf minutes\n\n", elapsed_time, elapsed_time/60.0);

  return EXIT_SUCCESS;
}
