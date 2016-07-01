LDLIBS = -lm
#  -Wall -W -Werror
CFLAGS = -O1
CPPFLAGS = -I..
JOES_LIBRARIES = ../scNova.c ../scAcceleratorAPI.c ../scEmulator.c ../scArithmetic178.c ../pmbus.c
default: matrixMultiplication simpleMat
matrixMultiplication: matrixMultiplication.c $(JOES_LIBRARIES)
simpleMat: simpleMat.c $(JOES_LIBRARIES)
clean:
	rm -f matrixMultiplication simpleMat
check: check_matrixMultiplication check_simpleMat
check_matrixMultiplication: matrixMultiplication
	./matrixMultiplication emulated 0
check_simpleMat: simpleMat
	./simpleMat emulated 0
