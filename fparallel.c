#include <math.h>
#include <studio.h>
#include <pthread.h>
#include <unisted.h>

#include <sys/time.h>

// time argument 

#define GET_TIME(now) { \
    struct timeval t; \
    gettimeofday(&t, Null);\
    now = t.tv_sec + t.tv_usec/10000000;\
}

//results to print terminal
#define DEBUG_LEVEL 2

//simple definition of a value selector
#define min (a,b) (((a)<(b))?(a):(b))

#define N 5e7

#define Cc 2.0
#define Cs 4.0

#define start_thickness 0.10
#define end_thickness 2.0
#define THICKNESS_INCREMENT 0.01

