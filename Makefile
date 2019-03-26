
#
# Makefile for simulated video feedback using ArrayFire library
# (LD_LIBRARY_PATH must include /opt/arrayfire/lib64)
#

# choose one of these, CUDA is about 40x as fast
#LIBS = -lafcpu
LIBS = -lafcuda

INCLUDES = -I/opt/arrayfire/include
LIB_PATHS = -L/opt/arrayfire/lib64

CC = g++ 
CFLAGS = -g $(INCLUDES)

all: fb

fb: fb.o
	$(CC) $(CFLAGS) -o fb fb.o $(LIB_PATHS) $(LIBS)

.cpp.o:
	$(CC) $(CFLAGS) -c $< -o $*.o

clean:
	rm -f *.o fb
