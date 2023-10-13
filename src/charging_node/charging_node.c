#include "charging_node.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "../helpers/helpers.h"

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
            node->available_neighbour_nodes[i].id = -1;
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

    char buf[256];
    FILE *f;
    f = fopen("logs_cart.txt", "a");

    sprintf(buf, "Cart rank: %d. Coord: (%d, %d). Top: %d. Bottom: %d. Left: %d. Right: %d\n", id, coord[0], coord[1], top_rank, bottom_rank, left_rank, right_rank);
    fprintf(f, "%s", buf);
    printf("%s", buf);
    fclose(f);

    struct ChargingPort **ports = malloc(sizeof(struct ChargingPort *) * num_ports);
    for (int i = 0; i < num_ports; i++)
    {
        int id = i;
        ports[i] = new_charging_port(node, id);
    }
    node->ports = ports;

    return node;
}

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

void *start_charging_port_thread(void *arg)
{
    struct ChargingPort *port = arg;
    start_charging_port(port);

    return NULL;
}

void start_charging_node(struct ChargingNode *node)
{
    pthread_t charging_port_t[node->num_ports];
    for (int i = 0; i < node->num_ports; i++)
    {
        pthread_create(&charging_port_t[i], 0, start_charging_port_thread, (void *)node->ports[i]);
    }

    MPI_Status probe_status, status;
    int has_alert = 0;
    int sig_term;

    MPI_Datatype MPI_ALERT_MESSAGE;
    define_mpi_alert_message(&MPI_ALERT_MESSAGE);

    MPI_Datatype MPI_AVAILABLE_NODES;
    define_mpi_available_nodes(&MPI_AVAILABLE_NODES);

    while (1)
    {

        MPI_Iprobe(BASE_STATION_RANK, TERMINATION_TAG, node->world_comm, &has_alert, &probe_status);
        if (has_alert == 1)
        {
            printf("receiving termination messages from base station\n");
            MPI_Recv(&sig_term, 1, MPI_INT, probe_status.MPI_SOURCE, TERMINATION_TAG, node->world_comm, &status);
            printf("received termination messages from base station\n");
            break;
        }

        // listen to port and write to log
        int availability = get_availability(node);

        char currentTimestamp[TIMESTAMP_LEN];
        get_timestamp(currentTimestamp);

        char listening_message_buf[1024];
        sprintf(listening_message_buf, "%s %d ports available", currentTimestamp, availability); // TODO 1b: Add timestamp (DONE)
        log_charging_node_event(node, listening_message_buf);

        MPI_Request send_request[MAX_NUM_NEIGHBOURS], receive_request[MAX_NUM_NEIGHBOURS];
        MPI_Status send_status[MAX_NUM_NEIGHBOURS], receive_status[MAX_NUM_NEIGHBOURS];

        int top_availability, bottom_availability, left_availability, right_availability;

        // Send readings to adjacent nodes
        MPI_Isend(&availability, 1, MPI_INT, node->top_rank, 0, node->grid_comm_cart, &send_request[0]);
        MPI_Isend(&availability, 1, MPI_INT, node->bottom_rank, 0, node->grid_comm_cart, &send_request[1]);
        MPI_Isend(&availability, 1, MPI_INT, node->left_rank, 0, node->grid_comm_cart, &send_request[2]);
        MPI_Isend(&availability, 1, MPI_INT, node->right_rank, 0, node->grid_comm_cart, &send_request[3]);

        // Request readings from adjacent nodes
        MPI_Irecv(&top_availability, 1, MPI_INT, node->top_rank, 0, node->grid_comm_cart, &receive_request[0]);
        MPI_Irecv(&bottom_availability, 1, MPI_INT, node->bottom_rank, 0, node->grid_comm_cart, &receive_request[1]);
        MPI_Irecv(&left_availability, 1, MPI_INT, node->left_rank, 0, node->grid_comm_cart, &receive_request[2]);
        MPI_Irecv(&right_availability, 1, MPI_INT, node->right_rank, 0, node->grid_comm_cart, &receive_request[3]);

        MPI_Waitall(4, send_request, send_status);
        MPI_Waitall(4, receive_request, receive_status);

        // TODO (1e): Indicate available neighbour nodes (DONE)

        int availabilities[MAX_NUM_NEIGHBOURS] = {top_availability, bottom_availability, left_availability, right_availability};

        char neighbour_message_buf[1024];
        sprintf(neighbour_message_buf, "%s available neighbour nodes: [", currentTimestamp);

        int first_entry = 1;
        for (int i = 0; i < MAX_NUM_NEIGHBOURS; i++)
        {
            if (node->available_neighbour_nodes[i].id < 0)
            {
                node->available_neighbour_nodes[i].num_ports = -1; // indicate no neighbours
            }
            else if (availabilities[i] > 0 && availabilities[i] <= NUM_PORTS)
            {
                node->available_neighbour_nodes[i].num_ports = availabilities[i];

                char neighbour_info[256];
                if (!first_entry)
                    strcat(neighbour_message_buf, ", ");
                else
                    first_entry = 0;

                sprintf(neighbour_info, "%s node %d (%d ports)", direction[i], node->available_neighbour_nodes[i].id, availabilities[i]);
                strcat(neighbour_message_buf, neighbour_info);
            }
            else if (availabilities[i] == 0 && node->available_neighbour_nodes[i].id >= 0)
            {
                node->available_neighbour_nodes[i].num_ports = 0; // indicate heavy port
            }
        }

        strcat(neighbour_message_buf, "]");
        log_charging_node_event(node, neighbour_message_buf);

        if (availability == 0)
        {
            if ((node->top_rank < 0 || top_availability == 0) &&
                (node->bottom_rank < 0 || bottom_availability == 0) &&
                (node->left_rank < 0 || left_availability == 0) &&
                (node->right_rank < 0 || right_availability == 0))
            {
                // TODO (1f): Add timestamp, Add which nodes are not available (in list), Add total messages sent

                // TODO: Process neighbour nodes

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

                char alert_message_buf[1024];
                sprintf(alert_message_buf, "ALERT MESSAGE = { timestamp: %s, reporting node: %d (%d, %d), neighbouring nodes: [",
                        alert_message.timestamp, alert_message.reporting_node, alert_message.reporting_node_coord[0], alert_message.reporting_node_coord[1]);

                int first_entry = 1;
                for (int i = 0; i < num_neighbours; i++)
                {
                    char neighbour_info[256];
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

                // TODO: (1e, f) if no available adjacent ports, MPI_Send alert base station (DONE)

                log_charging_node_event(node, "sending alert message to base station");

                // char message[] = "Hello, Process 1!";
                // MPI_Send(&message, 1, MPI_CHAR, BASE_STATION_RANK, ALERT_TAG, node->world_comm);

                MPI_Send(&alert_message, 1, MPI_ALERT_MESSAGE, BASE_STATION_RANK, ALERT_TAG, node->world_comm);

                log_charging_node_event(node, "sent alert message to base station");

                // TODO: (1g) MPI_Recv base station nearest available node
                log_charging_node_event(node, "receiving available nodes from base station");

                MPI_Request report_request;
                double timeout = 10;
                double start_time = MPI_Wtime();

                struct AvailableNodes available_nodes;
                MPI_Irecv(&available_nodes, 1, MPI_AVAILABLE_NODES, BASE_STATION_RANK, REPORT_TAG, node->world_comm, &report_request);

                int flag;
                while (1)
                {
                    MPI_Test(&report_request, &flag, MPI_STATUS_IGNORE);

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

                /*if (flag)
                {
                    char received_message_buf[200];
                    sprintf(received_message_buf, "REPORT MESSAGE: { timestamp: %s, node %d, available nearby nodes: [ ",
                    available_nodes->timestamp, available_nodes->size);

                    first_entry = 0;
                    for (int i = 0; i < available_nodes->size; i++){

                        char node_info[100];

                        if (!first_entry) {
                            strcat(node_info, ", ");
                        } else {
                            first_entry = 0;
                        }

                        sprintf(node_info, "node %d", available_nodes->nodes[i]);
                        strcat(received_message_buf, node_info);
                    }

                    strcat(received_message_buf, "]} ");
                    log_charging_node_event(node, received_message_buf);

                    struct AvailableNodes *available_nodes;

                    MPI_Recv(available_nodes, 1, MPI_AVAILABLE_NODES, BASE_STATION_RANK, ALERT_TAG, node->world_comm, MPI_STATUS_IGNORE);

                    log_charging_node_event(node, "received available nodes from base station");
                } 

                int size = available_nodes->size;
                int nearby_nodes[size];

                for (int i = 0; i < size; i++)
                {
                    nearby_nodes[i] = available_nodes->nodes[i];
                }

                char report_message_buf[256];
                sprintf(report_message_buf, "REPORT MESSAGE: { timestamp: %s, no. of available nearby nodes: %d, nearby nodes: [",
                        available_nodes->timestamp, available_nodes->size);

                first_entry = 1;
                for (int i = 0; i < available_nodes->size; i++)
                {
                    char nearby_node_info[64];
                    if (!first_entry)
                        strcat(report_message_buf, ", ");
                    else
                        first_entry = 0;

                    sprintf(nearby_node_info, "node %d", available_nodes->nodes[i]);
                    strcat(report_message_buf, nearby_node_info);
                }

                strcat(report_message_buf, "] }");
                log_charging_node_event(node, report_message_buf); */
            }
            else
            {
                // TODO: prettify message
                char message[100];
                sprintf(message, "Node %d top_availability: %d, bottom_availability: %d, left_availability: %d, right_availability: %d",
                        node->id, top_availability, bottom_availability, left_availability, right_availability);
                log_charging_node_event(node, message);
            }
        }
        sleep(node->cycle_interval);
    }

    for (int i = 0; i < node->num_ports; i++)
    {
        node->ports[i]->sig_term = 0;
        pthread_join(charging_port_t[i], NULL);
    }

    free(node->ports);

    MPI_Type_free(&MPI_ALERT_MESSAGE);
    MPI_Type_free(&MPI_AVAILABLE_NODES);

    printf("[Node %d] terminating \n", node->id);
    free(node);
}

int get_availability(struct ChargingNode *node)
{
    int availability = 0;
    for (int i = 0; i < node->num_ports; i++)
    {
        if (node->ports[i]->is_available)
        {
            availability++;
        }
    }
    return availability;
}
