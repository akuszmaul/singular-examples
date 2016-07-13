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
