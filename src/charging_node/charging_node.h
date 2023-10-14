#ifndef _CHARGING_NODE_H_
#define _CHARGING_NODE_H_

#include <pthread.h>

#include "mpi.h"
#include "charging_port.h"
#include "../helpers/helpers.h"

struct NeighbouringNode
{
  int id;
  int coord[N_DIMS];
  int num_ports;
};

struct ChargingNode
{
  int id;
  int num_ports;
  float cycle_interval;
  int top_rank, bottom_rank, left_rank, right_rank;
  int node_coord[N_DIMS];
  struct NeighbouringNode available_neighbour_nodes[MAX_NUM_NEIGHBOURS];
  struct ChargingPort **ports;
  MPI_Comm world_comm, grid_comm_cart;
  pthread_t charging_port_t[NUM_PORTS];
};

struct ChargingNode *new_charging_node(int num_ports, float cycle_interval, int id, MPI_Comm world_comm, MPI_Comm grid_comm_cart);
void start_charging_node(struct ChargingNode *node);
int get_availability(struct ChargingNode *node);

#endif