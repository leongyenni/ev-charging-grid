compile: src/helpers/*.c src/*.c src/base_station/*.c src/charging_node/*.c 
	mpicc src/helpers/*.c src/*.c src/base_station/*.c src/charging_node/*.c  -lm -o bin/main
run:
	mpirun --oversubscribe -np 10 bin/main 3 3
clean:
	rm bin/main