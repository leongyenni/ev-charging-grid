# Default values
M = 3
N = 3
N_PROC = 10

compile: src/helpers/*.c src/*.c src/base_station/*.c src/charging_node/*.c 
	mpicc src/helpers/*.c src/*.c src/base_station/*.c src/charging_node/*.c  -lm -o out
run:
	mpirun --oversubscribe -np $(np) out $(m) $(n)
clean:
	rm out

np ?= $(N_PROC)
m ?= $(M)
n ?= $(N)