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
