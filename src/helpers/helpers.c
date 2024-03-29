#include "helpers.h"

#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "mpi.h"

const char *direction[] = {"top", "bottom", "left", "right"};

/* Define custom data type (MPI_ALERT_MESSAGE) */
void define_mpi_alert_message(MPI_Datatype *CUSTOM_MPI_ALERT_MESSAGE)
{
	const int fields = 6;
	int blocklengths[6] = {TIMESTAMP_LEN, 1, N_DIMS, MAX_NUM_NEIGHBOURS, MAX_NUM_NEIGHBOURS * N_DIMS, 1};

	MPI_Datatype types[6] = {MPI_CHAR, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT};
	MPI_Aint offsets[6];

	offsets[0] = offsetof(struct AlertMessage, timestamp);
	offsets[1] = offsetof(struct AlertMessage, reporting_node);
	offsets[2] = offsetof(struct AlertMessage, reporting_node_coord);
	offsets[3] = offsetof(struct AlertMessage, neighbouring_nodes);
	offsets[4] = offsetof(struct AlertMessage, neighbouring_nodes_coord);
	offsets[5] = offsetof(struct AlertMessage, num_neighbours);

	MPI_Type_create_struct(fields, blocklengths, offsets, types, CUSTOM_MPI_ALERT_MESSAGE);
	MPI_Type_commit(CUSTOM_MPI_ALERT_MESSAGE);
}

/* Define custom data type (MPI_AVAILABLE_NODES) */
void define_mpi_available_nodes(MPI_Datatype *CUSTOM_MPI_AVAILABLE_NODES)
{
	const int fields = 3;
	int blocklengths[3] = {TIMESTAMP_LEN, 1, MAX_NUM_NEARBY};

	MPI_Datatype types[3] = {MPI_CHAR, MPI_INT, MPI_INT};
	MPI_Aint offsets[3];

	offsets[0] = offsetof(struct AvailableNodes, timestamp);
	offsets[1] = offsetof(struct AvailableNodes, size);
	offsets[2] = offsetof(struct AvailableNodes, nodes);

	MPI_Type_create_struct(fields, blocklengths, offsets, types, CUSTOM_MPI_AVAILABLE_NODES);
	MPI_Type_commit(CUSTOM_MPI_AVAILABLE_NODES);
}

/* Get current timestamp */
void get_timestamp(char *curentTimestamp)
{
	time_t now;
	struct tm *currentTime;

	time(&now);
	currentTime = localtime(&now);

	strftime(curentTimestamp, TIMESTAMP_LEN, "%Y-%m-%d %H:%M:%S", currentTime);
}

/* Convert to time_t */
time_t timestampToTimeT(const char *timestamp)
{
	struct tm tm_time;
	strptime(timestamp, "%Y-%m-%d %H:%M:%S", &tm_time);
	return mktime(&tm_time);
}

/* Calculate time taken */
double get_time_taken(struct timespec start, struct timespec end)
{
	double seconds = end.tv_sec - start.tv_sec;
	double nanoseconds = end.tv_nsec - start.tv_nsec;
	return seconds + nanoseconds / 1e9; // Convert nanoseconds to seconds
}

/* Random float maths function */
float rand_float(float min, float max)
{
	float scale = rand() / (float)RAND_MAX;
	return min + scale * (max - min);
}

/* Random boolean maths function */
bool rand_bool()
{
	return (int)rand_float(1, 10) % 2 == 0;
}