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
