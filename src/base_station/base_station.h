#ifndef _BASE_STATION_H_
#define _BASE_STATION_H_

#include <stdio.h>
#include <pthread.h>

#include "mpi.h"
#include "../helpers/helpers.h"

struct BaseStation
{
  MPI_Comm world_comm;
  int grid_size;
  float listen_frequency_s;
  int *node_availabilities;
  int *nearby_availabilities;
  struct AlertMessage *alert_messages;
  int num_alert_messages;
  int num_iter;
  struct Log *logger;
  FILE *log_file_handler;  
};

struct BaseStation *new_base_station(MPI_Comm world_comm, int grid_size, float listen_frequency_s, FILE *log_file_handler);
void start_base_station(struct BaseStation *base_station);
void close_base_station(struct BaseStation *base_station);

struct Log 
{
  char logged_timestamp[TIMESTAMP_LEN];
  char alert_timestamp[TIMESTAMP_LEN];
  int reporting_node;
  int reporting_node_coord[N_DIMS];
  int neighbouring_nodes[MAX_NUM_NEIGHBOURS];
  int neighbouring_nodes_coord[MAX_NUM_NEIGHBOURS][N_DIMS];
  int nearby_nodes[MAX_NUM_NEARBY];
  int nearby_nodes_coord[MAX_NUM_NEARBY][N_DIMS];
  int available_nodes[MAX_NUM_NEARBY];
  int num_port;
  int num_neighbours;
  int num_nearby_nodes;
  int num_available_nodes;
  int num_iter;
  int num_messages_sent;
  int communication_time;
};

#endif