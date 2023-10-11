#ifndef _CHARGING_PORT_H_
#define _CHARGING_PORT_H_

#include <stdbool.h>

#include "charging_node.h"
#include "../helpers/helpers.h"

struct ChargingPort
{
  struct ChargingNode *parent_node;
  int id;
  bool is_available;
  bool sig_term;
};

struct ChargingPort *new_charging_port(struct ChargingNode *parent_node, int id);
void start_charging_port(struct ChargingPort *port);
bool is_available(struct ChargingPort *port);

#endif