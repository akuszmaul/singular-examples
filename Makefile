LDLIBS = -lm -lsingular
LDFLAGS = -L.
CFLAGS = -O1 -Wall -W -Werror
SINGULAR_CFLAGS = -O1 # cannot handle -Wall
CPPFLAGS = -I..
default: matrixMultiplication simpleMat mm.pdf

mm.pdf: mm.tex mm-main.c
	pdflatex -shell-escape mm

matrixMultiplication: matrixMultiplication.c mm-main.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< $(LDLIBS) -o $@

matrixMultiplication simpleMat: libsingular.a

# Build the library containing the singular emulation.

libsingular.a: \
  libsingular.a(scNova.o) \
  libsingular.a(scAcceleratorAPI.o) \
  libsingular.a(scEmulator.o) \
  libsingular.a(scArithmetic178.o) \
  libsingular.a(pmbus.o) \
  # This line intentionally left blank

scNova.o: ../scNova.c
	$(CC) $(SINGULAR_CFLAGS) -c -o $@ $<
scAcceleratorAPI.o: ../scAcceleratorAPI.c
	$(CC) $(SINGULAR_CFLAGS) -c -o $@ $<
scEmulator.o: ../scEmulator.c
	$(CC) $(SINGULAR_CFLAGS) -c -o $@ $<
scArithmetic178.o: ../scArithmetic178.c
	$(CC) $(SINGULAR_CFLAGS) -c -o $@ $<
pmbus.o: ../pmbus.c
	$(CC) $(SINGULAR_CFLAGS) -c -o $@ $<

clean:
	rm -f matrixMultiplication simpleMat matrixMultiplication.o simpleMat.o
reallyclean: clean
	rm -rf libsingular.a scNova.o scAcceleratorAPI.o scEmulator.o scArithmetic178.o pmbus.o

check: check_matrixMultiplication check_simpleMat
check_matrixMultiplication: matrixMultiplication
	./matrixMultiplication emulated 0
check_simpleMat: simpleMat
	./simpleMat emulated 0
