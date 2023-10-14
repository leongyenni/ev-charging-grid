#ifndef _HELPERS_H_
#define _HELPERS_H_

#include <stdbool.h>
#include "mpi.h"

#define TERMINATION_TAG 10
#define ALERT_TAG 20
#define REPORT_TAG 30

#define CYCLE_INTERVAL_S 10
#define RESET_INTERVAL_S 40

#define ITERATION 20

#define DEFAULT_ROW 3
#define DEFAULT_COL 3

#define NUM_PORTS 5
#define MAX_NUM_NEIGHBOURS 4
#define N_DIMS 2
#define REORDER 1

#define BASE_STATION_RANK 0
#define SHIFT_ROW 0
#define SHIFT_COL 1
#define DISP 1

#define TIMESTAMP_LEN 20

extern const char *direction[MAX_NUM_NEIGHBOURS];
extern int m;
extern int n;

extern MPI_Datatype MPI_ALERT_MESSAGE;
extern MPI_Datatype MPI_AVAILABLE_NODES;

struct AlertMessage
{
    char timestamp[TIMESTAMP_LEN];
    int reporting_node;
    int reporting_node_coord[N_DIMS];
    int neighbouring_nodes[MAX_NUM_NEIGHBOURS];
    int neighbouring_nodes_coord[MAX_NUM_NEIGHBOURS][N_DIMS];
    int num_neighbours;
};

struct AvailableNodes
{
    char timestamp[TIMESTAMP_LEN];
    int size;
    int nodes[MAX_NUM_NEIGHBOURS*MAX_NUM_NEIGHBOURS];
};

void define_mpi_alert_message(MPI_Datatype *MPI_ALERT_MESSAGE);
void define_mpi_available_nodes(MPI_Datatype *MPI_AVAILABLE_NODES);
void get_timestamp(char *curentTimestamp);
float rand_float(float min, float max);
bool rand_bool();

#endif