/*********************************************************************************
    (c) Copyright 2011-2016 Singular Computing LLC

    This file and related materials are Confidential Information
    and Proprietary Property of Singular Computing LLC.

**********************************************************************************/


/* 

   Simple matrix arithmetic example code for S1 using Nova

   gcc -lm -o simpleMat simpleMat.c scNova.c scAcceleratorAPI.c scEmulator.c scArithmetic178.c pmbus.c


   We keep a matrix (A) stored persistently in the S1,
   and apply operations to it using a temporary matrix (B):
   A = B
   A += b (scalar)
   A *= b (scalar)
   A = b/A (scalar)
   A += B
   A *= B

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

#include "scAcceleratorAPI.h"
#include "scNova.h"


int emulated;                     // 1 for emulated machine, 0 for real S1 hardware
extern int scTotalCyclesTaken;    // if emulated, total cycles taken to run last kernel
extern MachineState *scEmulatedMachineState;   // if emulated, pointer to emulated machine's state
extern LLKernel *llKernel;

// Size info of the real or emulated machine
int chipRows;
int chipCols;
int apeRows;
int apeCols;

int traceFlags;

#define N 8

// Declare space for A and B matrices on CPU, in float format.
float floatA[N][N];
float floatB[N][N];

// Declare space for a matrix on CPU, in approx format (16 bits), aligned at 64 bits.
uint64_t approxM[(N*N)/4];

// Declare often used Nova Constants
Declare(a0);
Declare(a1);

// Declare names of A and B matrices in Ape memory.
Declare(A);
Declare(B);

// Initialization routine to define the names above
void defineNames () {
    a0 = AConst(0);
    a1 = AConst(1);
    ApeMem(A, Approx);
    ApeMem(B, Approx);
}


void emitCopyMatrixFromCUToApes(int cuAddress, int apeAddress) {
    // Copy N*N 16 bit data words
    // from CU Data Memory starting at cuAddress 
    // to the Ape grid, in Ape[0..N-1, 0..N-1]Mem[apeAddress].

    eCUC(cuSet, cuRChipRow, _, 0);
    eCUC(cuSet, cuRChipCol, _, 0);
    int col;
    eCUX(cuSetRWAddress, _, _, cuAddress);
    CUFor(cuRApeRow, IntConst(0), IntConst(N-1), IntConst(1));
    eCUC(cuSet, cuRApeCol, _, 0);
    for (col=0; col<N; col++) {
        eCUC(cuWrite, _, rwIgnoreMasks|rwUseCUMemory|rwIncApeCol, apeAddress);
    }
    CUForEnd();
}

void emitCopyMatrixFromApesToCU(int apeAddress, int cuAddress) {
    // Copy N*N 16 bit data words
    // from the Ape grid, in Ape[0..N-1, 0..N-1]Mem[apeAddress],
    // to CU Data Memory starting at cuAddress.
    // This code destroys cuR11 and apeR0.

    // Reserve apeR0 register for A matrix
    eControl(controlOpReserveApeReg,apeR0);

    // Load matrix into apeR0
    eApeC(apeLoad, apeR0, _, apeAddress);

    // Read matrix into CU Data memory
    eCUC(cuSet, cuRChipRow, _, 0);
    eCUC(cuSet, cuRChipCol, _, 0);
    int col;
    eCUX(cuSetRWAddress, _, _, cuAddress);
    CUFor(cuRApeRow, IntConst(0), IntConst(N-1), IntConst(1));
    eCUC(cuSet, cuRApeCol, _, 0);
    for (col=0; col<N; col++) {
        int propDelay = 4;  // this is plenty long
        eCUC(cuRead, _, rwIgnoreMasks|rwUseCUMemory|rwIncApeCol, (propDelay<<8)|apeR0);
    }
    CUForEnd();

    // Release apeR0
    eControl(controlOpReleaseApeReg,apeR0);

}

void copyBToCU () {
    // Convert floatB to approx and copy to B in CU Data Memory

    // Convert floatB to approxM
    int row, col;
    for (row=0; row<N; row++) {
        for (col=0; col<N; col++) {
            ((scApprox *)approxM)[N*row+col] = cvtApprox(floatB[row][col]);
        }
    }

    // Copy approxM[N][N] matrix, in row major order, to CUDataMem[0..N*N-1]
    scWriteCUDataMemoryBlock(2*N*N, (uintptr_t)approxM, 0 /* CU Data Mem starting address */);

}

void copyAFromCU () {
    // Copy A in CU Data Memory to floatA in CPU (and convert from approx to float)

    // Copy CUDataMem[0..N*N-1] to approxM[N][N] matrix, in row major order
    scReadCUDataMemoryBlock(2*N*N, (uintptr_t)approxM, 0 /* CU Data Mem starting address */);

    // Convert approxM to floatA
    int row, col;
    for (row=0; row<N; row++) {
        for (col=0; col<N; col++) {
            floatA[row][col] = cvtFloat(((scApprox *)approxM)[N*row+col]);
        }
    }

}


void emitPrintARowCol (int row, int col) {
    // Emit kernel code to print the value at A[row,col].
    // This only works when running in the Emulator.  The real S1 ignores these operations.
    // Note: This code leaves MaskMode turned on.

    // Note: We need to turn off Mask Mode in order to make the loads work
    eCUC(cuSetMaskMode, _, _, 0);

    // Allocate a string to hold the text part of the message
    char *str = malloc(30);
    // The above is NEVER FREED, but we expect the program to terminate with no problem
    sprintf(str, "  A[%0d,%0d]=\n", row, col);
    TraceMessage(str);

    TraceOneRegisterOneApe(A, row, col);

    // turn mask mode back on
    eCUC(cuSetMaskMode, _, _, 1);
}

void emitScalarSet (float f) {
    Set(A, AConst(f));
}

void emitScalarAdd (float f) {
    Set(A, Add(A, AConst(f)));
}

void emitScalarMul (float f) {
    Set(A, Mul(A, AConst(f)));
}

void emitScalarReciprocal (float f) {
    Set(A, Div(AConst(f), A));
}

void emitMatrixSet () {
    Set(A, B);
}

void emitMatrixAdd () {
    Set(A, Add(A, B));
}

// used getApe before, but that isn't set
// up for a torus config, which we're using in the matrix multiply
void emitGetTorus(scExpr x, int dir){
    eApeC(apeGetGStart, _, _, dir);
    eApeC(apeGetGStartDone, _, x, _);
    int i;
    for(i = 0; i< 16; i++){
        eApeC(apeGetGMove, _, _, _);
    }
    eApeC(apeGetGMoveDone, _, _, _);
    eApeC(apeGetGEnd, x, x, dir);
}

void emitMatrixMul () {
    // emit code for matrix multiply:  A = A + B
    // See Cypher and Sanz 5.6 for a description of this algorithm
    int i;
    // create variables in each ape for the apes row and column numbers
    // set them to zero initially
    DeclareApeVar(row, Int);
    DeclareApeVar(col, Int);
    Set(row,IntConst(0));
    Set(col,IntConst(0));

    // Need to use Ape variables to manipulate matrices A and B
    DeclareApeVar(Aloaded, Approx);
    Set(Aloaded, A);
    DeclareApeVar(Bloaded, Approx);
    Set(Bloaded, B);
    // Save B for later, don't ruin it
    DeclareApeVar(Bsaved, Approx);
    Set(Bsaved, B);
    
    // every ape gets it's row number and column number
    for (i = 0; i < N; i++){
        eApeC(apeGet, row, row, getNorth);
        Set(row, Add(row,IntConst(1)));
        eApeC(apeGet, col, col, getWest);
        Set(col, Add(col,IntConst(1)));
    }
    // Added one too many intconst1s, but easier to subtract
    // because we still want the extra shift
    Set(row, Sub(row,IntConst(1)));
    Set(col, Sub(col,IntConst(1)));

    // print row, column, and some matrix A values
    // to confirm these values are properly initialized
    // one value in one ape
    // first argument says which value
    // 3, 5 is the ape coordinates
    //TraceOneRegisterOneApe(row, 3, 5);
    //TraceOneRegisterOneApe(col, 3, 5);
    //TraceOneRegisterOneApe(A, 3, 7);
    //TraceOneRegisterOneApe(row, 7, 7);
    //TraceOneRegisterOneApe(col, 7, 7);
    //TraceOneRegisterOneApe(A, 7, 7);

    // Shift the rows of matrix A and cols of matrix B
    for (i = 0; i < N; i++){

        // check in to see if shifts are going well
        // each time around whatever was in 3,6 or 4,5 last time
        // should be in 3,5 or 3,5 this time.
        // should only shift x times for A in row x because of (row > i) ApeIf
        // should only shift 5 times for B in col 5 because of (col > i) ApeIf
        //TraceMessage("\nMatrix A at coordinate 3, 6 and 3,7\n");
        //TraceOneRegisterOneApe(A, 3, 5); // could say TraceOneRegisterAllApes(A);
        //TraceOneRegisterOneApe(A, 3, 7);
        //TraceMessage("\nMatrix B at coordinate 3, 5 and 4, 5\n");
        //TraceOneRegisterOneApe(B, 3, 5);
        //TraceOneRegisterOneApe(B, 4, 5);

        // NOTE:  HELP.  THIS TORUS GET MUST NOT WORK??  BECAUSE 7,7 keeps gettingEast 0s
        emitGetTorus(Aloaded, getEast);
        ApeIf(Gt(row, IntConst(i))); // yell out instructions to mask
             Set(A, Aloaded);
        ApeFi(); // clear masking from if
        
        emitGetTorus(Bloaded, getSouth);
        ApeIf(Gt(col, IntConst(i))); // yell out instructions to mask
             Set(B, Bloaded);
        ApeFi(); // clear masking from if
        
    } // end for i < N

    // Need to maniuplate Aloaded, for Get to work
    Set(Aloaded, A);
    Set(Bloaded, B);
    
    // multiply the Aloaded and Bloaded element in each Ape, saving their sum and shifting
    DeclareApeVar(runningTotal, Approx);
    Set(runningTotal,ApproxConst(0));
    i = 0;
    do{
        // should start off shifted and zeroed, so:
        //TraceMessage("runningTotal, Aloaded, Bloaded:\n");
        //TraceOneRegisterOneApe(runningTotal, 3, 5);
        //TraceOneRegisterOneApe(Aloaded, 3, 5);
        //TraceOneRegisterOneApe(Bloaded, 3, 5);
        //TraceMessage("\n");TraceMessage("Matrix Bloaded\n");
        // runningTotal = runningTotal + (Aloaded * Bloaded)
        Set(runningTotal, Add(runningTotal, Mul(Aloaded, Bloaded)));
        emitGetTorus(Aloaded, getEast);
        emitGetTorus(Bloaded, getSouth);
        i++;
    }while(i < N);

    Set(B, Bsaved);
    Set(A, runningTotal);
}


void check (char *testname, int i, int j, float expected) {
    // Print an error if floatA[i][j] is not close to expected.
    // If expected is 0 then we need to not divide by 0.
    float error = fabs((floatA[i][j] - expected) /
                       (expected != 0 ? expected : 1e-15));
    if (error > .02) {
        printf("On test '%s', A[%0d][%0d]=%e but expected %e\n",
               testname, i, j, floatA[i][j], expected);
    }
}


void tests () {
    // Run some tests
    int i,j;

    eCUC(cuSetMaskMode, _, _, 1); // enable conditionals


    // A=17
    emitScalarSet(17);

    // copy A from Apes to CU, send signal to CPU and wait for it to say to continue
    emitCopyMatrixFromApesToCU(MemAddress(A), 0 /* cuAddress */);
    eCUC(cuSetSignal, _, _, _);
    eCUC(cuWaitForClearSignal, _, _, _);


    // copy B from CU to Apes
    emitCopyMatrixFromCUToApes(0 /* cuAddress */, MemAddress(B));

    // A = B
    emitMatrixSet();

    // copy A from Apes to CU, send signal to CPU and wait for it to say to continue
    emitCopyMatrixFromApesToCU(MemAddress(A), 0 /* cuAddress */);
    eCUC(cuSetSignal, _, _, _);
    eCUC(cuWaitForClearSignal, _, _, _);


    // A = A + B
    emitMatrixAdd();

    // copy A from Apes to CU, send signal to CPU and wait for it to say to continue
    emitCopyMatrixFromApesToCU(MemAddress(A), 0 /* cuAddress */);
    eCUC(cuSetSignal, _, _, _);
    eCUC(cuWaitForClearSignal, _, _, _);


    // A = A * .5
    emitScalarMul(.5);

    // copy A from Apes to CU, send signal to CPU and wait for it to say to continue
    emitCopyMatrixFromApesToCU(MemAddress(A), 0 /* cuAddress */);
    eCUC(cuSetSignal, _, _, _);
    eCUC(cuWaitForClearSignal, _, _, _);


    // A = A * B
    emitMatrixMul();

    // copy A from Apes to CU, send signal to CPU and wait for it to say to continue
    emitCopyMatrixFromApesToCU(MemAddress(A), 0 /* cuAddress */);
    eCUC(cuSetSignal, _, _, _);
    eCUC(cuWaitForClearSignal, _, _, _);

    // check matrix A is what we want, compare to correct answers
    float CorrectMultiply[N][N];
    int k;
    for(i=0; i<N; i++){
        for(j=0; j<N; j++){
            CorrectMultiply[i][j] = 0;
            for(k=0; k<N; k++){
                CorrectMultiply[i][j] += floatB[i][j] * floatA[k][j];
            }
        }
    }
    for (i=0; i<N; i++) {
        for (j=0; j<N; j++) {
            check("Correct matrix multiplication", i, j, CorrectMultiply[i][j]);
        }
    }


    // emit Halt
    eCUC(cuHalt, _, _, _);

    // emit the low level translation of the high level kernel instructions
    ellNewKernelInstructions();

    // Load, free, start low level kernel
    scLLKernelLoad (llKernel, 0);
    scLLKernelFree(llKernel);
    scLLKernelExecute(0);


    // Wait for S1 to complete A=17 test
    // check if A[*,*] ~ 17
    scLLKernelWaitSignal();
    copyAFromCU();
        
    for (i=0; i<N; i++) {
        for (j=0; j<N; j++) {
            check("Set to 17", i, j, 17);
        }
    }

    // generate B[i][j]=i+j
    for (i=0; i<N; i++) {
        for (j=0; j<N; j++) {
            floatB[i][j] = i+j;
        }
    }

    // Copy B array to CU 
    copyBToCU();

    // allow S1 to continue, pull the flag down
    scClearCUSignal();


    // Wait for S1 to complete A=B test
    // check if A[i,j] ~ i+j
    // allow S1 to continue
    scLLKernelWaitSignal();
    copyAFromCU();
    
    for (i=0; i<N; i++) {
        for (j=0; j<N; j++) {
            check("A=B", i, j, i+j);
        }
    }
    scClearCUSignal();

    // Wait for S1 to complete A=A+B test
    // check if A[i,j] ~ 2*(i+j)
    // allow S1 to continue
    scLLKernelWaitSignal();
    copyAFromCU();
    for (i=0; i<N; i++) {
        for (j=0; j<N; j++) {
            check("A=A+B", i, j, 2*(i+j));
        }
    }
    scClearCUSignal();


    // Wait for S1 to complete A*=0.5 test
    // check if A[i,j] ~ i+j
    // allow S1 to continue
    scLLKernelWaitSignal();
    copyAFromCU();
    for (i=0; i<N; i++) {
        for (j=0; j<N; j++) {
            check("A*=0.5", i, j, i+j);
        }
    }
    scClearCUSignal();


    // Wait for S1 to complete A = A * B test
    // check if A[i,j] ~ floatB x floatB
    // allow S1 to continue
    scLLKernelWaitSignal();
    copyAFromCU();
    // check
    scClearCUSignal();


}




int main (int argc, char *argv[]) {
    //      {real | emulated}
    //      <trace>  see trace flags

    // process the command line arguments
    int argError = 0;
    int nextArg = 1;

    if (argc<= nextArg) argError = 1;
    else if (strcmp(argv[nextArg],"real")==0 ) {
        emulated = 0;
    } else if (strcmp(argv[nextArg],"emulated")==0 ) {
        emulated = 1;
    } else {
        printf("Machine argument not 'real' or 'emulated'.\n");
        argError = 1;
    }
    nextArg += 1;

    if (argc<= nextArg) argError = 1;
    else {
        traceFlags = atoi(argv[nextArg]);
        nextArg += 1;
    }

    if (argc > nextArg) {
        printf("Too many command line arguments.\n");
        argError = 1;
    }

    if (argError) {
        printf("  Command line arguments are:\n");
        printf("       <machine>        'real' or 'emulated'\n");
        printf("       <trace>          Translate | Emit | API | States | Instructions\n");
        exit(1);
    }

    // Initialize Singular arithmetic on CPU
    initSingularArithmetic ();

    // Create a machine
    chipRows = 1;
    chipCols = 1;
    apeRows = N; // 48
    apeCols = N; // 44
    scInitializeMachine ((emulated ? scEmulated : scRealMachine),
                         chipRows, chipCols, apeRows, apeCols,
                         traceFlags, 0 /* DDR */, 0 /* randomize */, 1 /* torus */);

    // Exit if S1 is still running.
    // (scInitializeMachine is supposed to completely reset the machine,
    // so this should not be able to happen, but current CU has a bug.)
    if (scReadCURunning() != 0) {
        printf("S1 is RUNNING AFTER RESET.  Terminating execution.\n");
        exit(1);
    }

    // Initialize the kernel creating code
    scKernelInit();
    scEmitLLKernelCreate();

    // Define some Nova names
    defineNames();

    // Run some tests
    tests();

    // Terminate the machine
    scTerminateMachine();

}




