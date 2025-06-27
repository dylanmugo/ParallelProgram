# Makefile for Serial and Parallel Programs

CC = gcc
CFLAGS = -g -Wall

# Header dependencies
DEPS = vars_defs_functions.h timer.h

# Object files for the serial program
SERIAL_OBJ = serial.o outputWRAT.o

# Object files for the parallel program
PARALLEL_OBJ = parallel.o 
# Default target: build both serial and parallel programs
all: serial parallel

# Rule to compile .c files into .o files
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

# Build the parallel program
parallel: $(PARALLEL_OBJ)
	$(CC) $(CFLAGS) $(PARALLEL_OBJ) -o $@ -lm -pthread

# Build the serial program
serial: $(SERIAL_OBJ)
	$(CC) $(CFLAGS) $(SERIAL_OBJ) -o $@ -lm

# Clean target: remove object files and executables
clean:
	rm -f $(SERIAL_OBJ) $(PARALLEL_OBJ) serial parallel
