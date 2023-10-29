#include "charging_port.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include "charging_node.h"
#include "../helpers/helpers.h"

struct ChargingPort *new_charging_port(struct ChargingNode *parent_node, int id, int seed)
{
  struct ChargingPort *port = malloc(sizeof(struct ChargingPort));
  port->parent_node = parent_node;
  port->id = id;
  port->is_available = true;
  port->sig_term = 1;
  port->seed = seed;
  return port;
}

void start_charging_port(struct ChargingPort *port)
{
  
  while (port->sig_term)
  {
    int r = rand_r(&port->seed);
    port->is_available = (r % 10 == 0);
    sleep(port->parent_node->cycle_interval);
  }
  
  free(port);
}

bool is_available(struct ChargingPort *port)
{
  return port->is_available;
}