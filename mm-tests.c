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
