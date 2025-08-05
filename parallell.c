#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

#define NEUTRONS 50000000
#define CAPTURE 2.0
#define SCATTER 4.0
#define TOTAL (CAPTURE + SCATTER)
#define ONE_OVER_TOTAL (1.0 / TOTAL)
#define CAPTURE_CHANCE (CAPTURE / TOTAL)

#define THICKNESS_START 0.10
#define THICKNESS_END 2.0
#define THICKNESS_STEP 0.01

#define GET_TIME(t) { struct timeval tv; gettimeofday(&tv, NULL); t = tv.tv_sec + tv.tv_usec / 1e6; }

typedef struct {
    double thickness;
    unsigned long long reflected;
    unsigned long long absorbed;
    unsigned long long transmitted;
} Result;

typedef struct {
    int thread_id;
    double start_thickness;
    double end_thickness;
    Result *result_array;
    long start_index;
} ThreadInput;

void *simulate_neutrons(void *input) {
    ThreadInput *data = (ThreadInput *)input;
    struct drand48_data rng;
    srand48_r(time(NULL) + data->thread_id, &rng);

    long result_index = data->start_index;

    for (double thickness = data->start_thickness; thickness < data->end_thickness; thickness += THICKNESS_STEP) {
        unsigned long long reflected = 0, absorbed = 0, transmitted = 0;

        for (int i = 0; i < NEUTRONS; i++) {
            double position = 0.0, angle = 0.0, distance, rand_value;
            int moving = 1;

            while (moving) {
                drand48_r(&rng, &rand_value);
                distance = -ONE_OVER_TOTAL * log(rand_value);
                position += distance * cos(angle);

                if (position < 0) {
                    reflected++; moving = 0;
                } else if (position >= thickness) {
                    transmitted++; moving = 0;
                } else if (rand_value < CAPTURE_CHANCE) {
                    absorbed++; moving = 0;
                } else {
                    angle = rand_value * M_PI;
                }
            }
        }

        data->result_array[result_index++] = (Result){thickness, reflected, absorbed, transmitted};
    }

    return NULL;
}

int main(int argc, char **argv) {
    double start_time, end_time;
    GET_TIME(start_time);

    if (argc < 2) {
        printf("Usage: %s <threads> [output_file]\n", argv[0]);
        return 1;
    }

    int num_threads = atoi(argv[1]);
    if (num_threads < 1) num_threads = sysconf(_SC_NPROCESSORS_ONLN);

    FILE *output_file = (argc >= 3) ? fopen(argv[2], "w") : fopen("WRAT_output.dat", "w");

    long total_steps = (THICKNESS_END - THICKNESS_START) / THICKNESS_STEP;
    long steps_per_thread = (total_steps + num_threads - 1) / num_threads;
    double thickness_range = steps_per_thread * THICKNESS_STEP;

    pthread_t threads[num_threads];
    ThreadInput inputs[num_threads];
    Result results[total_steps];

    for (int i = 0; i < num_threads; i++) {
        inputs[i] = (ThreadInput){
            .thread_id = i,
            .start_thickness = THICKNESS_START + i * thickness_range,
            .end_thickness = fmin(THICKNESS_START + (i + 1) * thickness_range, THICKNESS_END),
            .result_array = results,
            .start_index = i * steps_per_thread
        };
        pthread_create(&threads[i], NULL, simulate_neutrons, &inputs[i]);
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

   for (int i = 0; i < total_steps; i++) {
    double w = results[i].thickness;
    unsigned long long r = results[i].reflected;
    unsigned long long a = results[i].absorbed;
    unsigned long long t = results[i].transmitted;

    fprintf(output_file, "%lf %llu %llu %llu\n", w, r, a, t);
}

    fclose(output_file);

    GET_TIME(end_time);
    double runtime = end_time - start_time;
    printf("Time taken: %.2f seconds (%.2f minutes)\n", runtime, runtime / 60.0);

    return 0;
}
 