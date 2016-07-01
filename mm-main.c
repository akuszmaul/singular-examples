int main (int argc, char *argv[]) {

    // argError keeps track of the command line arguments and whether they were
    // given correctly.
    // nextArg keeps track of which command line argument is being processed.
    int argError = 0;
    int nextArg = 1;

    // There should be three command line arguments, so if argc is less than
    // or equal to one, there’s been an error.
    if (argc<= nextArg) argError = 1;

    // Processes whether the machine should be real or emulated.
    else if (strcmp(argv[nextArg],"real")==0 ) {
        emulated = 0;
    } else if (strcmp(argv[nextArg],"emulated")==0 ) {
        emulated = 1;
    } else {
        printf("Machine argument not 'real' or 'emulated'.\n");
        argError = 1;
    }

    // Moves on to process the next command line argument.
    nextArg += 1;

    // There should be more than 2 command line arguments.  There should be 3.
    if (argc<= nextArg) argError = 1;

    // Saves which trace flags should be shown.
    else {
        traceFlags = atoi(argv[nextArg]);
        nextArg += 1;
    }

    // There should only be 3 command line arguments, no more.
    if (argc > nextArg) {
        printf("Too many command line arguments.\n");
        argError = 1;
    }

    // If there’s a problem with the command line arguments, an error
    // message prints.
    if (argError) {
        printf("  Error: Command line arguments invalid.\n");
        printf("  Command line arguments are:\n");
        printf("  <machine>  'real' or 'emulated'\n");
        printf("  <trace>    ‘0’, ‘1’ , ‘2’ , ‘3’ , ‘4’ or ‘5’\n");
        printf("  <trace>    Translate | Emit | API | States | Instructions\n");
        exit(1);
    }

    // Initialize Singular arithmetic on CPU.  This simply allows the CPU
    // to do arithmetic.
    initSingularArithmetic ();

    // Creates a machine with 1 chip.  In the real machine, a chip has 48
    // ape rows and 44 ape columns.  However, for the sake of making this
    // code easier, and since we will be running it on an emulated machine,
    // we can specify that the number of ape rows and columns are equal to
    // the size of the square matrix we are multiplying.
    chipRows = 1;
    chipCols = 1;
    apeRows = N; // 48 in a real chip.
    apeCols = N; // 44 in a real chip.

    // Initializes a machine that is either emulated or real, depending
    // on the command line argument, has 1 chip, has 8x8 apes within each
    // chip, uses the trace flags in the command line argument, DDR (HELP?),
    // randomize(HELP?), and is a torus.
    scInitializeMachine ((emulated ? scEmulated : scRealMachine),
                         chipRows, chipCols, apeRows, apeCols,
                         traceFlags, 0 /* DDR */, 0 /* randomize */,
                         1 /* torus */);

    // Exit if S1 is still running.
    // (scInitializeMachine is supposed to completely reset the machine,
    // so this should not be able to happen, but current CU has a bug.)
    if (scReadCURunning() != 0) {
        printf("S1 is RUNNING AFTER RESET.  Terminating execution.\n");
        exit(1);
    }

    // Initializes the kernel creating code.
    scKernelInit();
    scEmitLLKernelCreate();

    // Defines some Nova names.
    defineNames();

    // Runs the tests.
    tests();

    // Terminates the machine.
    scTerminateMachine();

    return 0;
}
