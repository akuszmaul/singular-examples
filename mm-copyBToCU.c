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
