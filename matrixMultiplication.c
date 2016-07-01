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

void emitCopyMatrixFromCUToApes(int cuAddress, int apeAddress) {
    // Copies N*N 16 bit data words from CU Data Memory starting at
    // cuAddress to the Ape grid, in Ape[0..N-1, 0..N-1]Mem[apeAddress].

    // Sets the Chip row and column to zero.
    eCUC(cuSet, cuRChipRow, _, 0);
    eCUC(cuSet, cuRChipCol, _, 0);

    // Sets the location in CU memory.
    eCUX(cuSetRWAddress, _, _, cuAddress);

    // CUFor is different from a C for loop, because it is only one
    // instruction.  The CU will simply continue to reread that instruction
    // a certain number of times.  In a C for loop, the distinction is that
    // the same instruction is given multiple times, rather than simply
    // reread.  A C for loop is better if the instruction is only being given
    // a few times (few being relative).  However, if the loop is gone
    // through many times, a C for loop will take up all the instruction memory.

    // HELP: cuRApeRow, cuRapeCol, cuRChipRow, etc. - how does this work?

    // This CUFor loops through each Ape row between 0 and N-1,
    // incrementing up by 1 each time.
    int col;
    CUFor(cuRApeRow, IntConst(0), IntConst(N-1), IntConst(1));
    
    // Sets the Ape column to 0, then loops through each column
    // with a C for loop.
    eCUC(cuSet, cuRApeCol, _, 0);
    for (col=0; col<N; col++) {
        // Incrementing the Ape column number, it takes what’s at that Ape
        // address and writes it into the CU memory. HELP - don’t fully
        // understand.
        eCUC(cuWrite, _, rwIgnoreMasks|rwUseCUMemory|rwIncApeCol,
             apeAddress);
    }
    CUForEnd();
} // End emitCopyMatrixFromCUToApes.

void emitCopyMatrixFromApesToCU(int apeAddress, int cuAddress) {
    // Copies N*N 16 bit data words from the Ape grid, in
    // Ape[0..N-1, 0..N-1]Mem[apeAddress], to CU Data Memory starting at
    // cuAddress.
    // This code uses CU register 11 (cuR11) and ape register zero (apeR0),
    // destroying what was in those locations.

    // Reserves apeR0 register for A matrix.
    eControl(controlOpReserveApeReg,apeR0);

    // Loads matrix from the apeAddress given into apeR0.
    eApeC(apeLoad, apeR0, _, apeAddress);

    // Reads the matrix into CU Data memory.
    // First, sets the chip row and column to start at zero.
    // HELP? Limited understanding of why this is necessary.
    eCUC(cuSet, cuRChipRow, _, 0);
    eCUC(cuSet, cuRChipCol, _, 0);

    // Sets the location in memory to the given cuAddress.
    eCUX(cuSetRWAddress, _, _, cuAddress);

    // For every ape row, starting at row 0, working incrementally up
    // until row N-1...
    int col;
    CUFor(cuRApeRow, IntConst(0), IntConst(N-1), IntConst(1));
    // Starts at ape column zero, and works it’s way up the columns.
    eCUC(cuSet, cuRApeCol, _, 0);
    for (col=0; col<N; col++) {
        int propDelay = 4;  // This delay allows the CU enough time to
        // complete its previous command.
        // The cu reads each ape register 0 into its data memory.
        eCUC(cuRead, _, rwIgnoreMasks|rwUseCUMemory|rwIncApeCol,
             (propDelay<<8)|apeR0);
    }
    CUForEnd();

    // Releases apeR0.
    eControl(controlOpReleaseApeReg,apeR0);

} // End emitCopyMatrixFromApesToCU.

void copyBToCU () {
    // First, converts each value in the CPU’s B matrix of float values
    // (floatB) into the CPU’s matrix of approx values, (approxM), using
    // the tools in scArithmetic178.h.
    int row, col;
    for (row=0; row<N; row++) {
        for (col=0; col<N; col++) {
            ((scApprox *)approxM)[N*row+col] = cvtApprox(floatB[row][col]);
        }
    }

    // Copies approxM[N][N] matrix, in row major order, to CUDataMem[0..N*N-1].
    scWriteCUDataMemoryBlock(2*N*N, (uintptr_t)approxM, 0 /* CU Data Memory
                                                          starting address */);

} // End copyBToCU.

void copyAFromCU () {
    // Copies matrix A in CU Data Memory to matrix A (floatA) in CPU
    // (and converts the values from approx to float).

    // First, copies CUDataMem[0..N*N-1] to approxM[N][N] matrix, in row
    // major order.
    scReadCUDataMemoryBlock(2*N*N, (uintptr_t)approxM, 0 /* CU Data Memory 
                                                         starting address */);

    // Next, converts each value in approxM to floatA in the CPU.
    int row, col;
    for (row=0; row<N; row++) {
        for (col=0; col<N; col++) {
            floatA[row][col] = cvtFloat(((scApprox *)approxM)[N*row+col]);
        }
    }

} // End copyAFromCU.

void emitMatrixSet () {
    Set(A, B);
}

// Used getApe before, but that isn't set up for a torus configuration,
// which we're using in the matrix multiply.  HELP: don't fully understand.
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
    // Emit code for matrix multiply:  A = A + B.
    // See Cypher and Sanz 5.6 for a description of this algorithm.
    
    int i;
    
    // Create variables in each ape for the ape’s row and column numbers.
    // Set row and column to zero initially.
    DeclareApeVar(row, Int);
    DeclareApeVar(col, Int);
    Set(row,IntConst(0));
    Set(col,IntConst(0));

    // Need to use Ape variables to manipulate matrices A and B
    DeclareApeVar(Aloaded, Approx);
    Set(Aloaded, A);
    DeclareApeVar(Bloaded, Approx);
    Set(Bloaded, B);
    
    // Preserve matrix B, since this function should not alter
    // it permanently.
    DeclareApeVar(Bsaved, Approx);
    Set(Bsaved, B);

    // We must number the row and column variables, because right now they are
    // all set to zero.
    for (i = 0; i < N; i++){
        // Using apeGet from the North will give us a zero in the top row of
        // Apes, since apeGet does not use a torus configuration.
        eApeC(apeGet, row, row, getNorth);
        Set(row, Add(row,IntConst(1)));

        // Using apeGet from the West will give us a zero in the left-most
        // column of Apes, since apeGet does not use a torus configuration.
        eApeC(apeGet, col, col, getWest);
        Set(col, Add(col,IntConst(1)));
    }

    // We added one too many IntConst(1)s, because we still want the extra
    // shift in the for loop.  It’s easier to subtract IntConst(1) than to
    // do another shift after the for loop.  Now we subtract IntConst(1).
    Set(row, Sub(row,IntConst(1)));   // Sub() is a subtraction function found
                                      // in scNova.h
    Set(col, Sub(col,IntConst(1)));

    // Uncomment the following trace commands to print the row and column
    // of all the Apes.
    //TraceOneRegisterAllApes(row);
    //TraceOneRegisterAllApes(col);

    // Shift each row i of matrix A to the left i times.
    // Shift each column j of matrix B upwards j times.
    for (i = 0; i < N; i++){

        // If you want to see the shifts as they happen, uncomment
        // the following section of trace commands.
        // Each time the value that was in A[3][6] should have moved to A[3][5].
        // Each time the value that was in B[3][5] should have moved to B[4][5].
        // Should only shift 3 times for matrix A, because we're looking at
        // row 3.
        // Should only shift 5 times for matrix B, because we're looking at
        // column 5.
        //TraceMessage("\nMatrix A[3][5] and A[3][6]\n");
        //TraceOneRegisterOneApe(A, 3, 5);
        //TraceOneRegisterOneApe(A, 3, 6);
        //TraceMessage("\nMatrix B[3][5] and B[4][5].\n");
        //TraceOneRegisterOneApe(B, 3, 5);
        //TraceOneRegisterOneApe(B, 4, 5);
        //
        // Alternatively:
        // TraceOneRegisterAllApes(A);
        // TraceOneRegisterAllApes(B);
        
        // The Aloaded matrix is the same as matrix A.  Shift every
        // Aloaded value to the left by one position. 
        emitGetTorus(Aloaded, getEast);
        
        // Check whether the row number of each ape is greater than i.
        // This masks the other rows.
        ApeIf(Gt(row, IntConst(i)));
        // If the row number is greater than i, then we want the matrix
        // A value in that row to shift to the left by one position.
        // This is easily done by setting the non-masked values of matrix
        // A to equal the corresponding values of Aloaded, which we
        // already shifted.
        Set(A, Aloaded);
        ApeFi(); // Clear the masking.

        // The Bloaded matrix is the same as matrix B.  Shift every
        // Bloaded value upwards by one position.
        emitGetTorus(Bloaded, getSouth);
        
        // Check whether the column number of each ape is greater than i.
        // This will mask apes in smaller columns.
        ApeIf(Gt(col, IntConst(i)));
        // If the  column number is greater than i, then we want to shift
        // the matrix B values in that column upwards by one position.
        // We do this by setting the non-masked values of matrix B to equal
        // the corresponding values of Bloaded, which we already shifted.
        Set(B, Bloaded);
        ApeFi(); // Clear the masking from ApeIf().
                
    } // Ends for(i=0; i < N; i++).

    // emitGetTorus won’t allow us to get values directly from matrix A
    // or matrix B.  In order to use emitGetTorus we need to use a level
    // of misdirection and use the ape variables Aloaded and Bloaded.
    // First, we reset Aloaded and Bloaded to equal matrix A and matrix B.
    Set(Aloaded, A);
    Set(Bloaded, B);
    
    // We want a variable in each ape that holds the running total of the
    // matrix multiplication.
    DeclareApeVar(runningTotal, Approx);
    // Initially the running total is set to zero.
    Set(runningTotal,ApproxConst(0));
    
    i = 0;

    // Multiplies the Aloaded and Bloaded elements in each Ape and adds
    // the result to the running total.  Then shifts Aloaded to the left
    // and Bloaded upwards by one position. Repeats this N times.  When
    // this loop is finished, the running total will hold the result of
    // the matrix multiplication.
    do {
        // If we want to see the shifts as they happen, uncomment
        // the following Trace functions.
        //TraceMessage("runningTotal, Aloaded, Bloaded:\n");
        //TraceOneRegisterAllApes(runningTotal);
        //TraceOneRegisterAllApes(Aloaded);
        //TraceOneRegisterAllApes(Bloaded);
        
        // runningTotal = runningTotal + (Aloaded * Bloaded)
        Set(runningTotal, Add(runningTotal, Mul(Aloaded, Bloaded)));
        
        emitGetTorus(Aloaded, getEast); // Shifts Aloaded to the left.
        emitGetTorus(Bloaded, getSouth); // Shifts Bloaded upwards.
        
        i++;
    } while(i < N);

    // Resets matrix B to what it was before the multiplication, since we
    // didn’t want this function to alter matrix B.
    Set(B, Bsaved);
    
    // Sets matrix A equal to the runningTotal.
    // Matrix A now will hold the result of matrix A * B.
    Set(A, runningTotal);
    
} // End of matrix multiplication function.


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
    int i,j;

    // Enables conditionals (if statements, aka: Ape masking).
    eCUC(cuSetMaskMode, _, _, 1);
    
    // copy B from CU to Apes
    emitCopyMatrixFromCUToApes(0 /* cuAddress */, MemAddress(B));

    // A = B
    emitMatrixSet();

    // A = A * B
    emitMatrixMul();

    // Copy A from Apes to CU, send signal to CPU and wait for it to say
    // to continue.
    emitCopyMatrixFromApesToCU(MemAddress(A), 0 /* cuAddress */);
    eCUC(cuSetSignal, _, _, _);
    eCUC(cuWaitForClearSignal, _, _, _);

    // In order to check whether emitMatrixMul multiplied correctly, we
    // must calculate what results it should've given us.  We do this
    // by calculating matrix A * matrix B.
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

    // Compares the correct matrix multiplication results against the
    // results emitMatrixMul gave us.
    for (i=0; i<N; i++) {
        for (j=0; j<N; j++) {
            check("Correct matrix multiplication", i, j, CorrectMultiply[i][j]);
        }
    }

    // Emit Halt, waiting.
    eCUC(cuHalt, _, _, _);

    // Emit the low level translation of the high level kernel instructions.
    ellNewKernelInstructions();

    // Load, free, and start low level kernel.
    scLLKernelLoad (llKernel, 0);
    scLLKernelFree(llKernel);
    scLLKernelExecute(0);

    // Allow S1 to continue, pull the flag down.
    scClearCUSignal();

    // Wait for S1 to complete A = A * B test, before allowing S1 to continue.
    scLLKernelWaitSignal();
    copyAFromCU();
    scClearCUSignal();
} // End tests().

#include "mm-main.c"
