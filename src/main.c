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

int m, n;

MPI_Datatype MPI_ALERT_MESSAGE;
MPI_Datatype MPI_AVAILABLE_NODES;

void log_performance(int num_itr, int num_msg, double time_taken);

int main(int argc, char *argv[])
{
	FILE *log_file_handler = fopen("logs.txt", "a");

	struct timespec time_start, time_end;
	double time_taken;

	int world_rank, total_processes, provided;
	MPI_Status status;
	MPI_Comm world_comm = MPI_COMM_WORLD;
	MPI_Comm node_comm;
	MPI_Comm node_comm_cart;

	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

	define_mpi_alert_message(&MPI_ALERT_MESSAGE);
	define_mpi_available_nodes(&MPI_AVAILABLE_NODES);

	MPI_Comm_rank(world_comm, &world_rank);
	MPI_Comm_size(world_comm, &total_processes); // m * n + 1

	if (argc != 3)
	{
		m = DEFAULT_ROW;
		n = DEFAULT_COL;
	}
	else if (atoi(argv[1]) < 2 || atoi(argv[2]) < 2)
	{
		fprintf(stderr, "ERROR: Invalid grid size. Grid size must be greater or equal to 2x2.\n");
		goto cleanup_and_exit;
	}
	else if (total_processes != atoi(argv[1]) * atoi(argv[2]) + 1)
	{
		fprintf(stderr, "ERROR: Invalid no. of processors. No. of processors must greater than the (grid size + 1).\n");
		goto cleanup_and_exit;
	}
	else
	{
		m = atoi(argv[1]);
		n = atoi(argv[2]);
	}

	// Split into base station and node
	MPI_Comm_split(world_comm, world_rank == 0, 0, &node_comm);

	if (world_rank == 0) // Master
	{
		clock_gettime(CLOCK_MONOTONIC, &time_start);

		int grid_size = m * n;
		struct BaseStation *base_station = new_base_station(world_comm, grid_size, CYCLE_INTERVAL_S, log_file_handler);
		start_base_station(base_station);

		clock_gettime(CLOCK_MONOTONIC, &time_end);
		time_taken = get_time_taken(time_start, time_end);

		log_performance(base_station->num_iter, base_station->num_total_msg_sent, time_taken);
	}

	else // Slave
	{
		// initialize variables
		int dims[N_DIMS], coord[N_DIMS], wrap_around[N_DIMS];

		// Create cartesian mapping
		wrap_around[0] = wrap_around[1] = 0; // periodic shift is false
		dims[0] = m, dims[1] = n;

		int node_rank, total_nodes, grid_size;
		MPI_Comm_rank(node_comm, &node_rank);	// rank of the slave communicator
		MPI_Comm_size(node_comm, &total_nodes); // size of the slave communicator

		MPI_Dims_create(total_nodes, N_DIMS, dims);
		MPI_Cart_create(node_comm, N_DIMS, dims, wrap_around, REORDER, &node_comm_cart);
		MPI_Comm_size(node_comm_cart, &grid_size);

		struct ChargingNode *node = new_charging_node(NUM_PORTS, CYCLE_INTERVAL_S, node_rank, world_comm, node_comm_cart);
		start_charging_node(node);
	}

cleanup_and_exit:
	fclose(log_file_handler);
	MPI_Type_free(&MPI_ALERT_MESSAGE);
	MPI_Type_free(&MPI_AVAILABLE_NODES);
	MPI_Finalize();
	return 0;
}

/* Log the performance */
void log_performance(int num_itr, int num_msg, double time_taken)
{
	FILE *performance_file_handler = fopen("logs_performance.txt", "a");
	if (performance_file_handler == NULL)
	{
		perror("Error opening file");
		return;
	}
	fprintf(performance_file_handler, "Grid size: %d x %d\n", m, n);
	fprintf(performance_file_handler, "No. of charging ports: %d\n", NUM_PORTS);
	fprintf(performance_file_handler, "Time taken: %.2f (s)\n", time_taken);
	fprintf(performance_file_handler, "No. of runs: %d\n", num_itr);
	fprintf(performance_file_handler, "No. of reported messages: %d\n", num_msg);
	fprintf(performance_file_handler, "\n------------------------------------------------------------------------------------------------------------------------");
	fprintf(performance_file_handler, "\n\n");
	fclose(performance_file_handler);
}