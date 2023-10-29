compile: src/helpers/*.c src/*.c src/base_station/*.c src/charging_node/*.c 
	mpicc src/helpers/*.c src/*.c src/base_station/*.c src/charging_node/*.c  -lm -o out
run:
	mpirun --oversubscribe -np 10 out 3 3
clean:
	rm out
