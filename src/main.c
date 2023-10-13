#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <mpi.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>

#include "helpers/helpers.h"
#include "base_station/base_station.h"
#include "charging_node/charging_node.h"

int m,n;

int main(int argc, char *argv[])
{
	FILE *log_file_handler = fopen("logs.txt", "a");

	int world_rank, total_processes, provided;
	MPI_Status status;
	MPI_Comm world_comm = MPI_COMM_WORLD;
	MPI_Comm node_comm;

	MPI_Comm node_comm_cart;

	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
	MPI_Comm_rank(world_comm, &world_rank);
	MPI_Comm_size(world_comm, &total_processes); // m * n + 1

	if (argc != 3)
	{
		m = DEFAULT_ROW;
		n = DEFAULT_COL;
	} else {
		m = atoi(argv[1]);
		n = atoi(argv[2]);
	}
	

	// Split into base station and node
	MPI_Comm_split(world_comm, world_rank == 0, 0, &node_comm);

	if (world_rank == 0)
	{
		int grid_size = m * n;
		struct BaseStation *base_station = new_base_station(world_comm, grid_size, CYCLE_INTERVAL_S, log_file_handler);
		start_base_station(base_station);
	}

	else
	{
		// initialize variables
		int dims[N_DIMS], coord[N_DIMS], wrap_around[N_DIMS];

		/* Create cartesian mapping */
		wrap_around[0] = wrap_around[1] = 0; // periodic shift is false
		dims[0] = m, dims[1] = n;

		int node_rank, total_nodes, grid_size;
		MPI_Comm_rank(node_comm, &node_rank);	// rank of the slave communicator
		MPI_Comm_size(node_comm, &total_nodes); // size of the slave communicator

		MPI_Dims_create(total_nodes, N_DIMS, dims);

		if (node_rank == 0)
			printf("Node Rank: %d, Comm Size: %d: N_DIMS= %d, Grid Dimension = [%d x %d] \n", node_rank, total_nodes, N_DIMS, dims[0], dims[1]);

		MPI_Cart_create(node_comm, N_DIMS, dims, wrap_around, REORDER, &node_comm_cart);

		MPI_Comm_size(node_comm_cart, &grid_size);

		struct ChargingNode *node = new_charging_node(NUM_PORTS, CYCLE_INTERVAL_S, node_rank, world_comm, node_comm_cart);
		start_charging_node(node);
	}

cleanup_and_exit:
	fclose(log_file_handler);
	MPI_Finalize();
	return 0;
}
