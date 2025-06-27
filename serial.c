// This program simulates a simplified model of neutrons striking
// a homogenous plate of thickness, W, and of infinite height.
// The neutrons enter from the LEFT, and hit the plate. Once inside
// the plate, a neutron may get scattered, and "bounce" off atoms
// in the plate several times before being either:
//
//   ~ Reflected back out of the plate to the left where it came from, or
//   ~ Absorbed by an atom in the plate, or
//   ~ Transmitted; i.e. eventually travel through the plate and escape
//     to the right of the plate
//
// Each time a neutron bounces off an atom, it travels a distance L
// in the direction (angle) theta. We use these two values and a
// uniform random number to calculate how far in the horizontal
// direction the neutron travels between bounces before it is
// reflected, absorbed or transmitted.
//
// Dr. Kevin Farrell, TU Dublin. 9 June 2025.


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "timer.h"
#include "vars_defs_functions.h"

/////////////////////////  GLOBAL VARIABLES ///////////////////////////

// We should use long long int because we could have very large
// quantities of neutrons.
// unsigned means it is always a positive number.
unsigned long long int numR; //quantity of neutrons reflected
unsigned long long int numA; //quantity of neutrons absorbed
unsigned long long int numT; //quantity of neutrons transmitted

double C; //C is known as the total cross-section
double One_over_C; //Used in calculations later
double Cc_over_C; //Probability that a neutron will be absorbed

double W; //Plate thickness
long double pi;

int main(){

  double start, finish, elapsed; // Local variables for timing
  GET_TIME(start);

  // Initial values
  C = Cc + Cs; //C is known as the total cross-section
  One_over_C = 1.0/C; //Used in calculations later
  Cc_over_C = Cc/C; //Probability that a neutron will be absorbed

  //////////////////////////  LOCAL VARIABLES //////////////////////////////

  // This variable is the distance a neutron travels in the plate before
  // interacting with atom in the plate. We can use this value along with
  // the direction (angle theta) of travel of the neutron to work out the
  // distance that the neutron travels in the horizontal direction.
  double L;

  // This variable is the direction (angle) that a neutron travels in the
  // plate. It is measured in radians between 0 and pi (0 to 180 degrees).
  double theta;

  // Variable for uniform pseudo-random number (between 0.0 and 1.0)
  double uniformRandNum;
  struct drand48_data randBuffer; //random buffer for random numbers

  //Horizontal position of particle in the plate
  double x;

  //Variable to indicate if a neutron still bouncing: 1 = True, 0 = False
  int stillBouncing = 1;

  long long int i; //Used in for-loop over each neutron in the sample

  //Calculation of pi (i.e. 3.14159...) - needed later
  pi = 4.0*atan(1.0); //The tan(pi/4.0) = 1.0 => pi = 4.0*tan^-1(1.0)

  //File handle for output file, WRAT.dat
  FILE *fp_WRAT;

  // Open file WRAT.dat for writing
  // W = Plate Thickness, R = #Reflected, A = #Absorbed, T = #Transmitted
  // *** Note: You must create the data directory before running the program
  // Otherwise, you will get a segmentation fault error
  fp_WRAT = fopen("data/WRAT.dat", "w");

  // Generate random seed
  srand48_r(time(NULL), &randBuffer);

  /******************************************************************************
   * PLATE THICKNESS LOOP: Go from thin plate to thick plate
   *****************************************************************************/
  for(W = START_THICKNESS; W <= END_THICKNESS; W+=THICKNESS_INCREMENT){

    //Reset quantity of neutrons Reflected, Absorbed and Transmitted
    // for different plate thickness each time
    numR = 0; 
    numA = 0;
    numT = 0;

    ///////////////////////// NEUTRON SAMPLES LOOP ///////////////////////////
    //          There are N neutrons striking the plate to examine          //
    //        The value of N is set in the vars_defs_functions.h file       //
    //////////////////////////////////////////////////////////////////////////
    for(i = 1; i <= N; i++){

      //Reset values for next neutron
      theta = 0.0;
      x = 0.0;
      stillBouncing = 1;

      //Single neutron while loop: a neutron strikes the plate from the LEFT
      // Once inside the plate, the neutron may get scattered, and "bounce"
      // off atoms in the plate several times before being either:
      //
      // ~ Reflected back out of the plate to the left where it came from
      // ~ Absorbed by an atom in the plate
      // ~ Transmitted; i.e. eventually travel through the plate and escape
      //   to the right of the plate
      //
      while(stillBouncing == 1){ //A value of 1 is True: neutron still bouncing

        //Generate a uniformly distributed random double between 0.0 and 1.0
        // and store it in uniformRandNum
        drand48_r(&randBuffer, &uniformRandNum);

        //The distance a neutron travels in the plate before interacting with
        // an atom is given by the formula:
        L = -One_over_C*log(uniformRandNum);

        //Calculate the distance in the horizontal direction moved by the neutron
        // Initially, angle theta = 0.0 (as above):
        //   => neutron enters plate travelling along the x-axis
        // But neutron will change direction as it hits an atom, travelling a
        // distance, L. The HORIZONTAL component of this distance is added to
        // the existing horiztonal distance, x:
        x += L*cos(theta);//Note the "+=" here: adding L*cos(theta) to x

#if DEBUG_LEVEL > 3
        printf("x = %lf\n", x);
#endif

        if(x < 0){ //Neutron to the left of the plate => Reflected	  
          numR++; //Increment the quantity of Reflected neutrons
          stillBouncing = 0; //A value of 0 is False: i.e. no longer bouncing

#if DEBUG_LEVEL > 3
          printf("Reflected: numR = %lld\n", numR);
#endif	  
        }
        else if(x >= W){ //Neutron to the right of the plate => Transmitted	  
          numT++;
          stillBouncing = 0; //A value of 0 is False: i.e. no longer bouncing

#if DEBUG_LEVEL > 3
          printf("Transmitted: numT = %lld\n", numT);
#endif	  
        }
        //Probability of being absorbed is Cc/C:
        //So, if uniform random number (between 0.0 and 1.0) < Cc/C
        // => Neutron is Absorbed by an atom in the plate
        else if(uniformRandNum < Cc_over_C){ 	  
          numA++;
          stillBouncing = 0; //A value of 0 is False: i.e. no longer bouncing

#if DEBUG_LEVEL > 3
          printf("Absorbed: numA = %lld\n", numA);
#endif
        }

        else{ //Scattered inside the plate

          //Direction (angle) in radians that the neutron is scattered
          // Possibilties:
          //  theta = 0 radians => keep going to the right
          //  theta = pi radians => bounce straight back towards the left
          //  theta = angle between 0 and pi => travel in some other direction
          theta = uniformRandNum*pi; //pi radians = 180 degrees

#if DEBUG_LEVEL > 3
          printf("Scattered: theta = %lf\n", theta);
#endif
        }

      } ///   End of single neutron while loop ////

    } ////////////////////// END OF NEUTRON SAMPLES LOOP ////////////////////////

    //Output W, numR, numA, numT to file
    outputWRAT(fp_WRAT);

  }
  /******************************************************************************
   *  END OF PLATE THICKNESS LOOP
   *****************************************************************************/

  fclose(fp_WRAT);

  GET_TIME(finish);
  elapsed = finish - start;
  printf("Elapsed time = %lf seconds = %lf minutes\n\n", elapsed, elapsed/60.0);

  return 0;
}
