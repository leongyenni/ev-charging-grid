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
  struct AlertMessage *alert_messages;
  FILE *log_file_handler; // TODO
  pthread_t base_station_t; // TODO
};

// function to create new base station
// struct BaseStation *new_base_station(MPI_Comm world_comm, MPI_Comm grid_comm_cart, float listen_frequency_s, FILE *log_file_handler);
struct BaseStation *new_base_station(MPI_Comm world_comm, int grid_size, float listen_frequency_s, FILE *log_file_handler);
void start_base_station(struct BaseStation *base_station);
void close_base_station(struct BaseStation *base_station);

struct Log
{
  char timestamp[TIMESTAMP_LEN];
  int grid_size, total_alerts, total_sent_messages;
};

#endif