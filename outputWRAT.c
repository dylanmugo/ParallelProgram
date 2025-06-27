#include "vars_defs_functions.h"


void outputWRAT(FILE *fp_WRAT){

  #if DEBUG_LEVEL > 0
  printf("%lf %lld %lld %lld\n", W, numR, numA, numT);
  #endif

  fprintf(fp_WRAT, "%lf %lld %lld %lld\n", W, numR, numA, numT);

}
