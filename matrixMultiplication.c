/*********************************************************************************
    (c) Copyright 2011-2016 Singular Computing LLC

    This file and related materials are Confidential Information
    and Proprietary Property of Singular Computing LLC.

**********************************************************************************/


/*

  Simple matrix multiplication example code for S1 using Nova.

  To compile: 
  gcc -lm -o matrixMultiplication matrixMultiplication.c scNova.c scAcceleratorAPI.c scEmulator.c scArithmetic178.c pmbus.c


  We keep a matrix (A) stored persistently in the S1,
  and multiply it with a temporary matrix (B), storing the result in A.

  For this code, A and B have fixed, identical, square NxN shapes, where N=8.
  Matrix elements are stored one per core.

*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

// Include the Singular Software libraries.
#include "scAcceleratorAPI.h"
#include "scNova.h"


int emulated; // 1 for emulated machine.  0 for real S1 hardware.

// If emulated, scTotalCyclesTake is the total number of cycles
// taken to run the last kernel.
extern int scTotalCyclesTaken;

// If emulated, *scEmulatedMachineState is a pointer to the
// emulated machine's state.
extern MachineState *scEmulatedMachineState;  

extern LLKernel *llKernel;

// Variables for the size info of the machine.
int chipRows;
int chipCols;
int apeRows;
int apeCols;

int traceFlags;

// Define the length of the square matrices.
#define N 8

// Declare space for matrices A and B on the CPU, in float format.
float floatA[N][N];
float floatB[N][N];

// Declare space for a matrix on the CPU, in approx format (16 bits),
// aligned at 64 bits.
uint64_t approxM[(N*N)/4];

// Declare often used Nova Constants.
Declare(a0);
Declare(a1);

// Declare names of A and B matrices in Ape memory.
Declare(A);
Declare(B);

void defineNames () {
// Initialization routine to define the names above.
    a0 = AConst(0);
    a1 = AConst(1);
    ApeMem(A, Approx);
    ApeMem(B, Approx);
}

#include "mm-emitCopyMatrixFromCUToApes.c"

#include "mm-emitCopyMatrixFromApesToCU.c"

#include "mm-copyBFromCU.c"

#include "mm-copyAFromCU.c"

void emitMatrixSet () {
    Set(A, B);
}

#include "mm-emitGetTorus.c"

#include "mm-emitMatrixMul.c"


#include "mm-check.c"

#include "mm-tests.c"

#include "mm-main.c"
