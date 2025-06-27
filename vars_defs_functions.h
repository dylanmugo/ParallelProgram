//
// Global Variables, Definitions and Function Prototypes
//

#ifndef VARS_AND_DEFS
#define VARS_AND_DEFS

#define DEBUG_LEVEL 3

#define N 1e7
#define OUTPUT_FREQ 1

#define START_THICKNESS 0.10
#define END_THICKNESS 2.0
#define THICKNESS_INCREMENT 0.01

//Cross-section of Capture
#define Cc 2.0

//Cross-section of Scattering
#define Cs 4.0

#include <stdio.h>

//Global Variables. Note the extern keyword. These must also be declared above main()
//without the extern keywor
// H = Plate Thickness, R = #Reflected, A = #Absorbed, T = #Transmitted
extern unsigned long long int numR; //unsigned means positive number
extern unsigned long long int numA; //unsigned means positive number
extern unsigned long long int numT; //unsigned means positive number

extern double C; //C is known as the total cross-section
extern double One_over_C; //Used in calculations later
extern double Cc_over_C; //Probability that a neutron will be absorbed

extern double W; //Plate thickness
extern long double pi;

//Function Prototypes

void outputWRAT(FILE *fp_WRAT);


#endif
