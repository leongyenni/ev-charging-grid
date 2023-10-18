#include "charging_node.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "../helpers/helpers.h"

void send_alert_message(struct ChargingNode *node);
void receive_available_nodes_message(struct ChargingNode *node);
int get_availability(struct ChargingNode *node);
void log_charging_node_event(struct ChargingNode *node, const char *message);
void log_event_time(struct ChargingNode *node, const char *communication_time);

struct ChargingNode *new_charging_node(int num_ports, float cycle_interval, int id, MPI_Comm world_comm, MPI_Comm grid_comm_cart)
{
    struct ChargingNode *node = malloc(sizeof(struct ChargingNode));
    int left_rank, right_rank, top_rank, bottom_rank;
    MPI_Cart_shift(grid_comm_cart, SHIFT_ROW, DISP, &top_rank, &bottom_rank);
    MPI_Cart_shift(grid_comm_cart, SHIFT_COL, DISP, &left_rank, &right_rank);

    int coord[N_DIMS];
    MPI_Cart_coords(grid_comm_cart, id, N_DIMS, coord);

    node->id = id;
    node->num_ports = num_ports;
    node->cycle_interval = cycle_interval;
    node->world_comm = world_comm;
    node->grid_comm_cart = grid_comm_cart;

    node->top_rank = top_rank;
    node->bottom_rank = bottom_rank;
    node->left_rank = left_rank;
    node->right_rank = right_rank;
    node->node_coord[0] = coord[0];
    node->node_coord[1] = coord[1];

    int pos_rank[MAX_NUM_NEIGHBOURS] = {node->top_rank,
                                        node->bottom_rank,
                                        node->left_rank,
                                        node->right_rank};

    for (int i = 0; i < MAX_NUM_NEIGHBOURS; i++)
    {
        if (pos_rank[i] < 0)
        {
            node->available_neighbour_nodes[i].id = -1;          // Indicate invalid rank
            node->available_neighbour_nodes[i].coord[0] = -1;
            node->available_neighbour_nodes[i].coord[1] = -1;
        }
        else
        {
            node->available_neighbour_nodes[i].id = pos_rank[i];
            int neighbour_coord[N_DIMS];
            MPI_Cart_coords(grid_comm_cart, pos_rank[i], N_DIMS, neighbour_coord); // coordinate of neighbour node

            node->available_neighbour_nodes[i].coord[0] = neighbour_coord[0];
            node->available_neighbour_nodes[i].coord[1] = neighbour_coord[1];
        }
        node->available_neighbour_nodes[i].num_ports = -1;
    }

    /*char buf[256];
    FILE *f;
    f = fopen("logs_cart.txt", "a");

    sprintf(buf, "Cart rank: %d. Coord: (%d, %d). Top: %d. Bottom: %d. Left: %d. Right: %d\n", id, coord[0], coord[1], top_rank, bottom_rank, left_rank, right_rank);
    fprintf(f, "%s", buf);
    printf("%s", buf);
    fclose(f); */

    // Initializes charging ports
    struct ChargingPort **ports = malloc(sizeof(struct ChargingPort *) * num_ports);
    for (int i = 0; i < num_ports; i++)
    {
        int id = i;
        int seed = (int)(intptr_t)&ports[i] + i + node->id;
        ports[i] = new_charging_port(node, id, seed);
    }
    node->ports = ports;

    return node;
}

/* Start charging port thread */
void *start_charging_port_thread(void *arg)
{
    struct ChargingPort *port = arg;
    start_charging_port(port);

    return NULL;
}

/* Start charging node */
void start_charging_node(struct ChargingNode *node)
{
    pthread_t charging_port_t[node->num_ports];
    for (int i = 0; i < node->num_ports; i++)
    {
        pthread_create(&charging_port_t[i], 0, start_charging_port_thread, (void *)node->ports[i]);
    }

    MPI_Status probe_status, status;
    int has_alert = 0;
    int sig_term = 1;

    while (1)
    {
        int availability = get_availability(node);

        char currentTimestamp[TIMESTAMP_LEN];
        get_timestamp(currentTimestamp);

        char listening_message_buf[1024];
        sprintf(listening_message_buf, "%s %d ports available", currentTimestamp, availability);
        log_charging_node_event(node, listening_message_buf);

        MPI_Request send_request[MAX_NUM_NEIGHBOURS], receive_request[MAX_NUM_NEIGHBOURS];
        MPI_Status send_status[MAX_NUM_NEIGHBOURS], receive_status[MAX_NUM_NEIGHBOURS];

        int top_availability, bottom_availability, left_availability, right_availability;
        int availabilities[MAX_NUM_NEIGHBOURS] = {top_availability, bottom_availability, left_availability, right_availability};
        int ranks[MAX_NUM_NEIGHBOURS] = {node->top_rank, node->bottom_rank, node->left_rank, node->right_rank};

        // Send readings to adjacent nodes and receive readings from adjacent nodes
        for (int i = 0; i < MAX_NUM_NEIGHBOURS; i++) {
            MPI_Isend(&availability, 1, MPI_INT, ranks[i], 0, node->grid_comm_cart, &send_request[i]);
            MPI_Irecv(&availabilities[i], 1, MPI_INT, ranks[i], 0, node->grid_comm_cart, &receive_request[i]);
        }
       
        // Receive termination message from base station
        MPI_Iprobe(BASE_STATION_RANK, TERMINATION_TAG, node->world_comm, &has_alert, &probe_status);
        
        if (has_alert == 1)
        {
            printf("receiving termination messages from base station\n");
            MPI_Recv(&sig_term, 1, MPI_INT, probe_status.MPI_SOURCE, TERMINATION_TAG, node->world_comm, &status);
            printf("received termination messages from base station\n");
            break;
        }


        MPI_Waitall(4, send_request, send_status);
        MPI_Waitall(4, receive_request, receive_status);

        char neighbour_message_buf[2024];
        sprintf(neighbour_message_buf, "%s available neighbour nodes: [", currentTimestamp);

        int first_entry = 1;
        int neighbours_availabilities = 0;
        for (int i = 0; i < MAX_NUM_NEIGHBOURS; i++)
        {
            if (node->available_neighbour_nodes[i].id < 0)
            {
                node->available_neighbour_nodes[i].num_ports = -1; // Indicate no neighbours
            }
            else if (availabilities[i] > 0 && availabilities[i] <= NUM_PORTS)
            {
                node->available_neighbour_nodes[i].num_ports = availabilities[i];
                neighbours_availabilities++;

                char neighbour_info[516];
                if (!first_entry)
                    strcat(neighbour_message_buf, ", ");
                else
                    first_entry = 0;

                sprintf(neighbour_info, "%s node %d (%d ports)", direction[i], node->available_neighbour_nodes[i].id, availabilities[i]);
                strcat(neighbour_message_buf, neighbour_info);
            }
            else if (availabilities[i] == 0 && node->available_neighbour_nodes[i].id >= 0)
            {
                node->available_neighbour_nodes[i].num_ports = 0; // Indicate heavy port
            }
        }

        char info[100];
        sprintf(info, "], no. available neighbours: %d", neighbours_availabilities);
        strcat(neighbour_message_buf, info);
        log_charging_node_event(node, neighbour_message_buf);

        if (availability == 0 && neighbours_availabilities == 0) 
        {
            send_alert_message(node); // Alert the base station that the node and its quadrant are all being used up.
            receive_available_nodes_message(node);
        }

        sleep(node->cycle_interval);
    }

    for (int i = 0; i < node->num_ports; i++)
    {
        node->ports[i]->sig_term = 0;
        pthread_join(charging_port_t[i], NULL);
    }

    // Deallocate memory
    free(node->ports);

    printf("[Node %d] terminating \n", node->id);
    free(node);
}

/* Send alert message to base station */
void send_alert_message(struct ChargingNode *node)
{
    char currentTimestamp[TIMESTAMP_LEN];
    get_timestamp(currentTimestamp);

    struct AlertMessage alert_message;
    strcpy(alert_message.timestamp, currentTimestamp);
    alert_message.reporting_node = node->id;
    alert_message.reporting_node_coord[0] = node->node_coord[0];
    alert_message.reporting_node_coord[1] = node->node_coord[1];

    int num_neighbours = 0;

    for (int i = 0; i < MAX_NUM_NEIGHBOURS; i++)
    {
        if (node->available_neighbour_nodes[i].num_ports == 0)
        {
            alert_message.neighbouring_nodes[num_neighbours] = node->available_neighbour_nodes[i].id;
            alert_message.neighbouring_nodes_coord[num_neighbours][0] = node->available_neighbour_nodes[i].coord[0];
            alert_message.neighbouring_nodes_coord[num_neighbours][1] = node->available_neighbour_nodes[i].coord[1];
            num_neighbours += 1;
        }
    }

    alert_message.num_neighbours = num_neighbours;

    char alert_message_buf[2024];
    sprintf(alert_message_buf, "ALERT MESSAGE = { timestamp: %s, reporting node: %d (%d, %d), neighbouring nodes: [",
            alert_message.timestamp, alert_message.reporting_node, alert_message.reporting_node_coord[0], alert_message.reporting_node_coord[1]);

    int first_entry = 1;
    for (int i = 0; i < num_neighbours; i++)
    {
        char neighbour_info[516];
        if (!first_entry)
            strcat(alert_message_buf, ", ");
        else
            first_entry = 0;
        sprintf(neighbour_info, "node %d (%d, %d)", alert_message.neighbouring_nodes[i],
                alert_message.neighbouring_nodes_coord[i][0], alert_message.neighbouring_nodes_coord[i][1]);
        strcat(alert_message_buf, neighbour_info);
    };

    sprintf(alert_message_buf + strlen(alert_message_buf), "], num_neighbours: %d }", alert_message.num_neighbours);
    log_charging_node_event(node, alert_message_buf);

    log_charging_node_event(node, "sending alert message to base station");

    MPI_Send(&alert_message, 1, MPI_ALERT_MESSAGE, BASE_STATION_RANK, ALERT_TAG, node->world_comm);

    log_charging_node_event(node, "sent alert message to base station");
}

/* Receive a list of available nodes from base station */
void receive_available_nodes_message(struct ChargingNode *node)
{
    log_charging_node_event(node, "receiving available nodes from base station");

    double timeout = 25;
    double start_time = MPI_Wtime();

    MPI_Request report_request
    struct AvailableNodes available_nodes;
    MPI_Recv(&available_nodes, 1, MPI_AVAILABLE_NODES, BASE_STATION_RANK, REPORT_TAG, node->world_comm, &report_request);

    int flag;
    while (1)
    {
        MPI_Test(&flag, MPI_STATUS_IGNORE);

        if (flag)
        {
            printf("[Node %d] Received data: %d\n", node->id, available_nodes.size);
            break;
        }

        if ((MPI_Wtime() - start_time) >= timeout)
        {
            printf("[Node %d] Receive operation timed out.\n", node->id);
            MPI_Cancel(&report_request);
            break;
        }
    }

    if (flag)
    {

        int size = available_nodes.size;
        int nearby_nodes[size];
        for (int i = 0; i < size; i++)
        {
            nearby_nodes[i] = available_nodes.nodes[i];
        }

        char report_message_buf[5000];
        sprintf(report_message_buf, "REPORT MESSAGE: { timestamp: %s, no. of available nearby nodes: %d, nearby nodes: [",
                available_nodes.timestamp, available_nodes.size);

        int first_entry = 1;
        for (int i = 0; i < available_nodes.size; i++)
        {
            char nearby_node_info[1000];
            if (!first_entry)
                strcat(report_message_buf, ", ");
            else
                first_entry = 0;

            sprintf(nearby_node_info, "node %d", nearby_nodes[i]);
            strcat(report_message_buf, nearby_node_info);
        }

        strcat(report_message_buf, "] }");
        log_charging_node_event(node, report_message_buf);
    }
}

/* Get port availabilities */
int get_availability(struct ChargingNode *node)
{
    int availability = 0;
    for (int i = 0; i < node->num_ports; i++)
    {
        if (is_available(node->ports[i]))
        {
            availability++;
        }
    }
    char node_avail_buf[50];
    sprintf(node_avail_buf, "Node %d num ports: %d", node->id, availability);
    return availability;
}

int get_neighbours_availability(struct ChargingNode *node)
{
    int neighbours_availability = 0;
    for (int i = 0; i < MAX_NUM_NEIGHBOURS; i++)
    {
        int neighbour_availability = node->available_neighbour_nodes[i].num_ports;

        if (neighbour_availability > 0 && neighbour_availability <= NUM_PORTS)
        {
            neighbours_availability = 1;
            break;
        }
    }

    return neighbours_availability;
}

/* Write charging node log message into terminal */
void log_charging_node_event(struct ChargingNode *node, const char *message)
{
    printf("[Node %d] %s\n", node->id, message);
    FILE *f;
    char file_buf[64];
    sprintf(file_buf, "logs_charging_node_%d.txt", node->id);
    f = fopen(file_buf, "a");
    fprintf(f, "[Node %d] %s\n", node->id, message);
    fclose(f);
}