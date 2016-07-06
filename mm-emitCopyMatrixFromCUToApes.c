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
