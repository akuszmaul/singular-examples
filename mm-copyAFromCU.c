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
