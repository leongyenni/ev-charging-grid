#include "charging_port.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include "charging_node.h"
#include "../helpers/helpers.h"

struct ChargingPort *new_charging_port(struct ChargingNode *parent_node, int id)
{
  struct ChargingPort *port = malloc(sizeof(struct ChargingPort));
  port->parent_node = parent_node;
  port->id = id;
  port->is_available = true;
  port->sig_term = 1;
  return port;
}

void start_charging_port(struct ChargingPort *port)
{
  printf("starting charging port %d on node %d \n", port->id, port->parent_node->id);
  // printf("Node, ")

  while (port->sig_term)
  {
    // TODO: handle terminating signal (DONE)
    // int sig_term = port->sig_term;
    // if (!sig_term)
    // {
    //   break;
    // }
    port->is_available = rand_bool();
    sleep(port->parent_node->cycle_interval);
  }

  printf("terminating charging port %d on node %d \n", port->id, port->parent_node->id);

  free(port);
}

bool is_available(struct ChargingPort *port)
{
  return port->is_available;
}