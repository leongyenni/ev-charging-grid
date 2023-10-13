#include "base_station.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "mpi.h"
#include "../helpers/helpers.h"

time_t lastResetTime = 0;

int get_sig_term();
int get_world_rank(int i);
int get_index(int world_rank);
int write_to_logs(struct BaseStation *base_station, struct Log *log);
void resetNodeAvailabilities(struct BaseStation *base_station);
void checkResetTimer(struct BaseStation *base_station);
void get_neighbours(struct BaseStation *base_station, int x_row, int y_col, struct AvailableNodes *available_nodes);

void log_base_station_event(struct BaseStation *base_station, const char *message)
{
    printf("[Base Station] %s\n", message);
    FILE *f;
    f = fopen("logs_base_station.txt", "a");
    fprintf(f, "[Base Station] %s\n", message); // Log the message
    fclose(f);
}

struct BaseStation *new_base_station(MPI_Comm world_comm, int grid_size, float listening_frenquency_s, FILE *log_file_handler)
{
    struct BaseStation *base_station = malloc(sizeof(struct BaseStation));
    base_station->world_comm = world_comm;
    // base_station->grid_comm = grid_comm;
    base_station->listen_frequency_s = CYCLE_INTERVAL_S;
    base_station->log_file_handler = log_file_handler;
    base_station->grid_size = grid_size;

    base_station->node_availabilities = malloc(sizeof(int) * base_station->grid_size);
    for (int i = 0; i < base_station->grid_size; i++)
    {
        base_station->node_availabilities[i] = 1;
    }

    // MPI_Comm_size(grid_comm, &base_station->grid_size);
    return base_station;
}

void start_base_station(struct BaseStation *base_station)
{
    printf("starting base station \n");

    MPI_Status probe_status, status;
    int num_iter = 0;

    MPI_Datatype MPI_ALERT_MESSAGE;
    define_mpi_alert_message(&MPI_ALERT_MESSAGE);

    MPI_Datatype MPI_AVAILABLE_NODES;
    define_mpi_available_nodes(&MPI_AVAILABLE_NODES);

    // TODO: (2f)  send or receive MPI messages using POSIX thread
    while (num_iter < ITERATION)
    {
        int total_alerts, total_sent_messages = 0;

        num_iter += 1;
        // int sig_term = get_sig_term();
        // if (sig_term == 1)
        // {
        //     break;
        // }

        //checkResetTimer(base_station);

        for (int i = 1; i <= base_station->grid_size; i++)
        {
            int has_alert = 0;
            char currentTimestamp[TIMESTAMP_LEN];
            get_timestamp(currentTimestamp);

            char listening_message_buf[100];
            sprintf(listening_message_buf, "%s listening to node %d", currentTimestamp, i - 1);
            log_base_station_event(base_station, listening_message_buf);

            MPI_Iprobe(i, ALERT_TAG, base_station->world_comm, &has_alert, &probe_status);
            if (has_alert == 1)
            {
                int alert_source_rank = i;

                char rev_message_buf[1024];
                sprintf(rev_message_buf, "receiving alert message from charging node %d", alert_source_rank - 1);
                log_base_station_event(base_station, rev_message_buf);

                struct AlertMessage alert_message;
                MPI_Recv(&alert_message, 1, MPI_ALERT_MESSAGE, alert_source_rank, ALERT_TAG, base_station->world_comm, &status);

                sprintf(rev_message_buf, "received alert message from charging node %d", alert_source_rank - 1);
                log_base_station_event(base_station, rev_message_buf);

                // TODO: format the receive message

                int reporting_node = alert_message.reporting_node;
                int reporting_node_coord[N_DIMS];
                reporting_node_coord[0] = alert_message.reporting_node_coord[0];
                reporting_node_coord[1] = alert_message.reporting_node_coord[1];
                int num_neighbours = alert_message.num_neighbours;
                int neighbouring_nodes[num_neighbours];
                int neighbouring_nodes_coord[num_neighbours][N_DIMS];

                for (int i = 0; i < num_neighbours; i++)
                {
                    neighbouring_nodes[i] = alert_message.neighbouring_nodes[i];
                    neighbouring_nodes_coord[i][0] = alert_message.neighbouring_nodes_coord[i][0];
                    neighbouring_nodes_coord[i][1] = alert_message.neighbouring_nodes_coord[i][1];
                };

                char alert_message_buf[1024];
                sprintf(alert_message_buf, "ALERT MESSAGE = { timestamp: %s, reporting node: %d (%d, %d), neighbouring nodes: [",
                        alert_message.timestamp, reporting_node, reporting_node_coord[0], reporting_node_coord[1]);

                int first_entry = 1;
                for (int i = 0; i < num_neighbours; i++)
                {
                    char neighbour_info[516];
                    if (!first_entry)
                        strcat(alert_message_buf, ", ");
                    else
                        first_entry = 0;
                    sprintf(neighbour_info, "node %d (%d, %d)", neighbouring_nodes[i], neighbouring_nodes_coord[i][0],
                            neighbouring_nodes_coord[i][1]);
                    strcat(alert_message_buf, neighbour_info);
                }

                strcat(alert_message_buf, "] }");
                log_base_station_event(base_station, alert_message_buf);

                total_alerts++;

                // char sending_message_buf[64];
                // sprintf(sending_message_buf, "sending node availabilities to node %d", alert_source_rank - 1);
                // log_base_station_event(base_station, sending_message_buf);

                // MPI_Send(&available_nodes, 1, MPI_AVAILABLE_NODES, BASE_STATION_RANK, 0, base_station->world_comm);

                // char message[] = "REPORT MESSAGE";
                // MPI_Send(&message, 1, MPI_CHAR, reporting_node + 1, REPORT_TAG, base_station->world_comm);
                
                // MPI_Send(&available_nodes, 1, MPI_CHAR, BASE_STATION_RANK, 0, base_station->world_comm);

                // sprintf(sending_message_buf, "sent node availabilities to node %d", alert_source_rank - 1);
                // log_base_station_event(base_station, sending_message_buf);
            }
        }

        // for (int i = 0; i < base_station->grid_size; i++)
        // {
            // TODO (2c): report nearest available node

            // printf("Node availabilities: ");

            // base_station->node_availabilities[reporting_node] = 0;

            // for (int i = 0; i < base_station->grid_size; i++)
            // {
            //     printf("%d ", base_station->node_availabilities[i]);
            // }

            // int max_nearby_nodes = num_neighbours * 4;
            // get_timestamp(currentTimestamp);

            // struct AvailableNodes *available_nodes;
            // strcpy(available_nodes->timestamp, currentTimestamp);
            // available_nodes->size = 0;
            // available_nodes->nodes = (int *)malloc(max_nearby_nodes * sizeof(int));

            // printf("declare available nodes\n");

            // for (int i = 0; i < num_neighbours; i++)
            // {
                // int neighbouring_node_rank = neighbouring_nodes[i];
                // printf("reporting node: neighbour node %d ", neighbouring_nodes[i]);
                // printf("neighbouring node %d availability: %d ", neighbouring_node_rank, base_station->node_availabilities[neighbouring_node_rank]);

                // if (base_station->node_availabilities[neighbouring_node_rank] == 1)
                // {
                //     int x_row = neighbouring_nodes_coord[i][0];
                //     int y_col = neighbouring_nodes_coord[i][1];

                //     printf("before get neighbours");
                //     get_neighbours(base_station, x_row, y_col, available_nodes);
                // }

                // MPI_Send(&available_nodes, 1, MPI_AVAILABLE_NODES, BASE_STATION_RANK, 0, base_station->world_comm);
            // }

            // TODO (2d): write more stats to output file
            // struct Log *log = malloc(sizeof(struct Log));

            // char currentTimestamp[TIMESTAMP_LEN];
            // get_timestamp(currentTimestamp);

            // strcpy(log->timestamp, currentTimestamp);
            // log->grid_size = base_station->grid_size;
            // log->total_alerts = total_alerts;
            // log->total_sent_messages = total_sent_messages;

            // write_to_logs(base_station, log);

            // log_base_station_event(base_station, "listening to charging grid");
            //
            //  sleep(2);

            // free(available_nodes->nodes);
        // }

        sleep(base_station->listen_frequency_s);
    }

    MPI_Type_free(&MPI_ALERT_MESSAGE);
    MPI_Type_free(&MPI_AVAILABLE_NODES);

    printf("closing base station \n");
    close_base_station(base_station);
    printf("base station closed \n");
}

void close_base_station(struct BaseStation *base_station)
{
    MPI_Request send_request[base_station->grid_size];
    MPI_Status send_status[base_station->grid_size];

    printf("sending termination messages to charging grid \n");

    int sig_term = 1;
    for (int world_rank = 0; world_rank < base_station->grid_size + 1; world_rank++)
    {
        int i = get_index(world_rank);
        MPI_Isend(&sig_term, 1, MPI_INT, world_rank, TERMINATION_TAG, base_station->world_comm, &send_request[i]);
    }

    MPI_Waitall(base_station->grid_size, send_request, send_status);
    printf("sent termination messages to charging grid \n");

    free(base_station->node_availabilities);
    free(base_station);
}

int get_world_rank(int i)
{
    int world_rank = i;
    if (i >= BASE_STATION_RANK)
    {
        world_rank++;
    }
    return world_rank;
}

int get_index(int world_rank)
{
    int i = world_rank;
    if (world_rank == BASE_STATION_RANK)
    {
        return -1;
    }
    if (world_rank > BASE_STATION_RANK)
    {
        i = world_rank - 1;
    }
    return i;
}

// NIT: reuse file handler
int get_sig_term()
{
    FILE *f;
    f = fopen("sig_term.txt", "r");
    if (!f)
        return -1;
    char s[2];
    fgets(s, 2, f);
    fclose(f);
    if (strcmp(s, "0") == 0)
        return 1;
    else
        return 0;
}

void get_neighbours(struct BaseStation *base_station, int x_row, int y_col, struct AvailableNodes *available_nodes)
{

    printf("get neighbour function: row %d, col %d", x_row, y_col);
    int top_rank = (x_row)*n + (y_col - 1);
    int bottom_rank = (x_row)*n + (y_col + 1);
    int right_rank = (x_row + 1) * n + (y_col);
    int left_rank = (x_row - 1) * n + (y_col);

    int neighbour_nodes[MAX_NUM_NEIGHBOURS] = {top_rank, bottom_rank, left_rank, right_rank};

    for (int i = 0; i < MAX_NUM_NEIGHBOURS; i++)
    {
        if (neighbour_nodes[i] >= 0 && neighbour_nodes[i] < base_station->grid_size)
        {
            printf("%d", neighbour_nodes[i]);
            int index = available_nodes->size;
            available_nodes->nodes[index] = neighbour_nodes[i];
            available_nodes->size += 1;
        }
    }
}

/**
 * return value: 0 is success, 1 is failure
 */
// TODO: add more stats to logs
int write_to_logs(struct BaseStation *base_station, struct Log *log)
{
    printf("writing to logs \n");
    if (base_station->log_file_handler == NULL)
    {
        return 1;
    }

    printf("[%s] grid size=%d, total alerts=%d, total sent messages=%d\n",
           log->timestamp, log->grid_size, log->total_alerts, log->total_sent_messages);

    fprintf(base_station->log_file_handler, "[%s] grid size=%d, total alerts=%d, total sent messages=%d\n",
            log->timestamp, log->grid_size, log->total_alerts, log->total_sent_messages);

    printf("wrote to logs \n");

    return 0;
}

void resetNodeAvailabilities(struct BaseStation *base_station)
{
    for (int i = 0; i < base_station->grid_size; i++)
    {
        base_station->node_availabilities[i] = 1;
    }
}

void checkResetTimer(struct BaseStation *base_station)
{
    time_t currentTime = time(NULL);

    if (currentTime - lastResetTime >= RESET_INTERVAL_S)
    {
        resetNodeAvailabilities(base_station);
        lastResetTime = currentTime;

        char reset_message_buf[40];
        sprintf(reset_message_buf, "RESET MESSAGE: { available nodes: ");

        int is_first = 1;
        for (int i = 0; i < base_station->grid_size; i++)
        {
            char node_info[20];
            if (!is_first)
            {
                strcat(node_info, ", ");
            }
            else
            {
                is_first = 0;
            }
            sprintf(node_info, "node %d: %d ", i, base_station->node_availabilities[i]);
            strcat(reset_message_buf, node_info);
        }

        strcat(reset_message_buf, " }");
        log_base_station_event(base_station, reset_message_buf);
        // printf('\n');
    }
}