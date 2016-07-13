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
