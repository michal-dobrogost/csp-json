/* urbcsp.c -- generates uniform random binary constraint satisfaction problems
*/
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "../../cj-csp-json.h"

/* function declarations */
float ran2(int32_t *idum);
CjError StartCSP(int N, int D, int K, int C, int T, int32_t S, int Instance, CjCsp* csp);
CjError EndCSP(CjCsp* csp, bool print);
CjError AddConstraint(int var1, int var2, CjConstraint* constraint);
CjError AddNogood(int tupleIdx, int val1, int val2, CjConstraintDef* constraintDef);
int MakeURBCSP(int N, int D, int K, int C, int T, int32_t S, int32_t *Seed, bool print);

/*********************************************************************
  This file has 5 parts:
  0. This introduction.
  1. A main() function, which can be used to demonstrate MakeURBCSP().
  2. MakeURBCSP().
  3. ran2(), a random number generator.
  4. The four functions StartCSP(), AddConstraint(), AddNogood(), and
     EndCSP(), which are called by MakeURBCSP().  The versions
     of these functions given here print out each instance, listing
     the incompatible value pairs of each constraint.  You will need
     to replace these functions with versions that mesh with your
     system and data structures.
*********************************************************************/


/*********************************************************************
  1. A simple main() function which reads in command line parameters
     and generates CSPs.
*********************************************************************/

int main(int argc, char* argv[])
{
  int N, D, K, C, T, I, i;
  int32_t S, Seed;

  if (argc != 7 && argc != 8) {
    fprintf(
      stderr,
      "Usage: cj-gen-urbcsp #vars #vals #constraints #nogoods seed #instances [#constraintDefs]\n"
      "\n"
      "  If #constraintDefs is missing it is set to equal #constraints which matches the\n"
      "  behaviour of the original urbcsp which didn't allow this argument.\n");
    return 1;
  }

  N = atoi(argv[1]);
  D = atoi(argv[2]);
  C = atoi(argv[3]);
  T = atoi(argv[4]);
  S = atoi(argv[5]);
  I = atoi(argv[6]);

  K = C;
  if (argc == 8) {
    K = atoi(argv[7]);
  }

  /* Seed passed to ran2() must initially be negative. */
  Seed = S;
  if (Seed > 0) {
    Seed = -Seed;
  }

  for (i=0; i<=I; ++i) {
    if (!MakeURBCSP(N, D, K, C, T, S, &Seed, i == I)) {
      return 2;
    }
  }

  return 0;
}


/*********************************************************************
  2. MakeURBCSP() creates a uniform binary constraint satisfaction
     problem with a specified number of variables, domain size,
     tightness, and number of constraints.  MakeURBCSP() calls
     four functions, StartCSP(), AddConstraint(), AddNogood(), and
     EndCSP(), which actually create the CSP (that is, build a data
     structure).  Feel free to change the signatures of these functions.
     Note that numbering starts from 0: the variables are numbered 0..N-1,
     and the values are numbered 0..K-1.

  INPUT PARAMETERS:
   N: number of variables
   D: size of each variable's domain
   K: number of constraint definitions
   C: number of constraints
   T: number of incompatible value pairs in each constraint
   S: the original seed specified on the command line
   Seed: a negative number means start a new sequence of
      pseudo-random numbers; a positive number means continue
      with the same sequence.  S is turned positive by ran2().
  RETURN VALUE:
      Returns 0 if there is a problem; 1 for normal completion.
*********************************************************************/

int MakeURBCSP(int N, int D, int K, int C, int T, int32_t S, int32_t *Seed, bool print)
{
  int PossibleCTs, PossibleNGs;       /* CT means "constraint" */
  uint32_t *CTarray, *NGarray;   /* NG means "nogood pair" */
  int32_t selectedCT, selectedNG;
  int i, c, r, t;
  int var1, var2, val1, val2;
  static int instance;

  PossibleCTs = N * (N - 1) / 2;
  PossibleNGs = D * D;

  /* Check for valid values of N, D, C, and T. */
  if (N < 2)
    {
      fprintf(stderr, "MakeURBCSP: ***Illegal value for N: %d (N >= 2)\n", N);
      return 0;
    }
  if (D < 2)
    {
      fprintf(stderr, "MakeURBCSP: ***Illegal value for D: %d (D >= 2)\n", D);
      return 0;
    }
  if (K < 1)
    {
      fprintf(stderr, "MakeURBCSP: ***Illegal value for K: %d (C >= 1)\n", K);
      return 0;
    }
  if (K > C)
    {
      fprintf(stderr, "MakeURBCSP: ***Illegal value for K: %d (K <= C)\n", K);
      return 0;
    }
  if (C < 1)
    {
      fprintf(stderr, "MakeURBCSP: ***Illegal value for C: %d (C >= 1)\n", C);
      return 0;
    }
  if (C > PossibleCTs)
    {
      fprintf(stderr, "MakeURBCSP: ***Illegal value for C: %d (C <= N*(N-1)/2 = %d)\n", C, PossibleCTs);
      return 0;
    }
  if (T < 1)
    {
      fprintf(stderr, "MakeURBCSP: ***Illegal value for T: %d (T >= 1)\n", T);
      return 0;
    }
  if (T >= PossibleNGs)
    {
      fprintf(stderr, "MakeURBCSP: ***Illegal value for T: %d (T < D*D = %d)\n", T, PossibleNGs);
      return 0;
    }

  if (*Seed < 0)      /* starting a new sequence of random numbers */
    instance = 0;
  else
    ++instance;       /* increment static variable */

  CjCsp csp = cjCspInit();
  CjError err = StartCSP(N, D, K, C, T, S, instance, &csp);
  if (err != CJ_ERROR_OK) { return 0; }

  /* The program has to choose randomly and uniformly m values from
     n possibilities.  It uses the following logic for both constraints
     and nogood value pairs:
           1. Let t[] be an array of the n possibilities
           2. for i = 0 to m-1
           3.    r = random(i, n-1)    ; random() returns an int in [i,n-1]
           4.    swap t[i] and t[r]
           5. end-for
     At the end of the for loop, the elements from t[0] to t[m-1] are
     the m randomly selected elements.
   */

  /* Create an array for each possible binary constraint. */
  CTarray = (uint32_t*) malloc(PossibleCTs * 4);

  /* Create an array for each possible value pair. */
  NGarray = (uint32_t*) malloc(PossibleNGs * 4);

  /* Initialize the CTarray.  Each entry has one var in the high two
     bytes, and the other in the low two bytes. */
  i=0;
  for (var1=0; var1<(N-1); ++var1)
    for (var2=var1+1; var2<N; ++var2)
      CTarray[i++] = (var1 << 16) | var2;

  /* Select C constraints. */
  for (c=0; c<C; ++c)
    {
      /* Choose a random number between c and PossibleCTs - 1, inclusive. */
      r =  c + (int) (ran2(Seed) * (PossibleCTs - c));

      /* Swap elements [c] and [r]. */
      selectedCT = CTarray[r];
      CTarray[r] = CTarray[c];
      CTarray[c] = selectedCT;

      /* Broadcast the constraint. */
      err = AddConstraint((int)(CTarray[c] >> 16), (int)(CTarray[c] & 0x0000FFFF), &csp.constraints[c]);
      if (err != CJ_ERROR_OK) { return 0; }

      /* For each constraint, select T illegal value pairs. */

      /* Initialize the NGarray. */
      for (i=0; i<(D*D); ++i)
        NGarray[i] = i;

      /* Select T incompatible pairs. */
      for (t=0; t<T; ++t)
        {
          /* Choose a random number between t and PossibleNGs - 1, inclusive.*/
          r =  t + (int) (ran2(Seed) * (PossibleNGs - t));
          selectedNG = NGarray[r];
          NGarray[r] = NGarray[t];
          NGarray[t] = selectedNG;

          /* Broadcast the nogood value pair. */
          err = AddNogood(t, (int)(NGarray[t] / D), (int)(NGarray[t] % D), &csp.constraintDefs[csp.constraints[c].id]);
          if (err != CJ_ERROR_OK) { return 0; }
        }
    }

  free(CTarray);
  free(NGarray);

  err = EndCSP(&csp, print);
  if (err != CJ_ERROR_OK) { return 0; }

  return 1;
}



/*********************************************************************
  3. This random number generator is from William H. Press, et al.,
     _Numerical Recipes in C_, Second Ed. with corrections (1994),
     p. 282.  This excellent book is available through the
     WWW at http://nr.harvard.edu/nr/bookc.html.
     The specific section concerning ran2, Section 7.1, is in
     http://cfatab.harvard.edu/nr/bookc/c7-1.ps
*********************************************************************/

#define IM1   2147483563
#define IM2   2147483399
#define AM    (1.0/IM1)
#define IMM1  (IM1-1)
#define IA1   40014
#define IA2   40692
#define IQ1   53668
#define IQ2   52774
#define IR1   12211
#define IR2   3791
#define NTAB  32
#define NDIV  (1+IMM1/NTAB)
#define EPS   1.2e-7
#define RNMX  (1.0 - EPS)

/* ran2() - Return a random floating point value between 0.0 and
   1.0 exclusive.  If idum is negative, a new series starts (and
   idum is made positive so that subsequent calls using an unchanged
   idum will continue in the same sequence). */

float ran2(int32_t *idum)
{
  int j;
  int32_t k;
  static int32_t idum2 = 123456789;
  static int32_t iy = 0;
  static int32_t iv[NTAB];
  float temp;

  if (*idum <= 0) {                             /* initialize */
    if (-(*idum) < 1)                           /* prevent idum == 0 */
      *idum = 1;
    else
      *idum = -(*idum);                         /* make idum positive */
    idum2 = (*idum);
    for (j = NTAB + 7; j >= 0; j--) {           /* load the shuffle table */
      k = (*idum) / IQ1;
      *idum = IA1 * (*idum - k*IQ1) - k*IR1;
      if (*idum < 0)
        *idum += IM1;
      if (j < NTAB)
        iv[j] = *idum;
    }
    iy = iv[0];
  }

  k = (*idum) / IQ1;
  *idum = IA1 * (*idum - k*IQ1) - k*IR1;
  if (*idum < 0)
    *idum += IM1;
  k = idum2/IQ2;
  idum2 = IA2 * (idum2 - k*IQ2) - k*IR2;
  if (idum2 < 0)
    idum2 += IM2;
  j = iy / NDIV;
  iy = iv[j] - idum2;
  iv[j] = *idum;
  if (iy < 1)
    iy += IMM1;
  if ((temp = AM * iy) > RNMX)
    return RNMX;                                /* avoid endpoint */
  else
    return temp;
}


/*********************************************************************
  4. An implementation of StartCSP, AddConstraint, AddNogood, and EndCSP
     which generates a CSP-JSON instance.
*********************************************************************/

CjError StartCSP(int N, int D, int K,int C, int T, int32_t S, int Instance, CjCsp* csp)
{
  if (!csp) { return CJ_ERROR_ARG; }

  *csp = cjCspInit();

  // Populate meta fields.
  const size_t strAllocSize = 1024;
  csp->meta.id = malloc(strAllocSize);
  if (!csp->meta.id) { return CJ_ERROR_NOMEM; }
  int stat = snprintf(csp->meta.id, strAllocSize, "urbcsp/n%dd%dc%dt%ds%di%dk%d", N, D, C, T, S, Instance, K);
  if (stat < 0) { return CJ_ERROR; }

  csp->meta.algo = malloc(strAllocSize);
  if (!csp->meta.algo) { return CJ_ERROR_NOMEM; }
  stat = snprintf(csp->meta.algo, strAllocSize, "urbcsp");
  if (stat < 0) { return CJ_ERROR; }

  csp->meta.paramsJSON = malloc(strAllocSize);
  if (!csp->meta.paramsJSON) { return CJ_ERROR_NOMEM; }
  stat = snprintf(csp->meta.paramsJSON, strAllocSize, "{\"n\": %d, \"d\": %d, \"c\": %d, \"t\": %d, \"s\": %d, \"i\": %d, \"k\": %d}", N, D, C, T, S, Instance, K);
  if (stat < 0) { return CJ_ERROR; }

  // Make a single domain [0, D-1].
  csp->domainsSize = 1;
  csp->domains = (CjDomain*) malloc(sizeof(CjDomain));
  if (!csp->domains) { return CJ_ERROR_NOMEM; }
  csp->domains[0] = cjDomainInit();
  csp->domains[0].type = CJ_DOMAIN_VALUES;
  CjError err = cjIntTuplesAlloc(D, -1, &csp->domains[0].values);
  for (int i = 0; i < D; ++i) {
    csp->domains[0].values.data[i] = i;
  }

  // Reference the single domain by all variables.
  err = cjIntTuplesAlloc(N, -1, &csp->vars);
  for (int i = 0; i < N; ++i) {
    csp->vars.data[i] = 0;
  }

  // Allocate and init constraintDefs.
  csp->constraintDefsSize = K;
  csp->constraintDefs = (CjConstraintDef*) malloc(K * sizeof(CjConstraintDef));
  for (int i = 0; i < K; ++i) {
    csp->constraintDefs[i] = cjConstraintDefInit();
    csp->constraintDefs[i].type = CJ_CONSTRAINT_DEF_NO_GOODS;
    const int size = T;
    const int arity = 2;
    err = cjIntTuplesAlloc(size, arity, &csp->constraintDefs[i].noGoods);
    if (err != CJ_ERROR_OK) { return err; }
    for (int iData = 0; iData < size*arity; ++iData) {
      csp->constraintDefs[i].noGoods.data[i] = 0;
    }
  }

  // Alloc and init constraints.
  // Reference relevant constraintDef.
  csp->constraintsSize = C;
  csp->constraints = (CjConstraint*) malloc(C * sizeof(CjConstraint));
  for (int i = 0; i < C; ++i) {
    csp->constraints[i] = cjConstraintInit();
    csp->constraints[i].id = i % K;
  }

  return CJ_ERROR_OK;
}

CjError AddConstraint(int var1, int var2, CjConstraint* constraint)
{
  if (!constraint) { return CJ_ERROR_ARG; }
  const int size = 2;
  const int arity = -1;
  CjError err = cjIntTuplesAlloc(size, arity, &constraint->vars);
  if (err != CJ_ERROR_OK) { return err; }
  constraint->vars.data[0] = var1;
  constraint->vars.data[1] = var2;

  return CJ_ERROR_OK;
}

CjError AddNogood(int tupleIdx, int val1, int val2, CjConstraintDef* constraintDef)
{
  if (!constraintDef || !constraintDef->noGoods.data) { return CJ_ERROR_ARG; }
  if (constraintDef->type != CJ_CONSTRAINT_DEF_NO_GOODS) { return CJ_ERROR_ARG; }

  constraintDef->noGoods.data[tupleIdx * 2 + 0] = val1;
  constraintDef->noGoods.data[tupleIdx * 2 + 1] = val2;

  return CJ_ERROR_OK;
}

CjError EndCSP(CjCsp* csp, bool print)
{
  if (!csp) { return CJ_ERROR_ARG; }
  int err = cjCspNormalize(csp);
  if (err != CJ_ERROR_OK) { return err; }
  if (print) {
    return cjCspJsonPrint(stdout, csp);
  }
  return CJ_ERROR_OK;
}

