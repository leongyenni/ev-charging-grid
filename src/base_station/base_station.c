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
void get_neighbours(struct BaseStation *base_station, int x_row, int y_col, struct AvailableNodes available_nodes);

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

    base_station->alert_messages = malloc(sizeof(struct AlertMessage) * base_station->grid_size);

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

        checkResetTimer(base_station);
        int num_alert_messages = 0;

        char currentTimestamp[TIMESTAMP_LEN];

        for (int i = 1; i <= base_station->grid_size; i++)
        {
            int has_alert = 0;
            get_timestamp(currentTimestamp);

            char listening_message_buf[100];
            sprintf(listening_message_buf, "%s listening to node %d", currentTimestamp, i - 1);
            log_base_station_event(base_station, listening_message_buf);

            MPI_Iprobe(i, ALERT_TAG, base_station->world_comm, &has_alert, &probe_status);
            if (has_alert == 1)
            {
                get_timestamp(currentTimestamp);
                printf("[Base Station] timestamp %s %d ALERT MESSAGES READ\n", currentTimestamp, num_alert_messages);

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

                base_station->alert_messages[num_alert_messages] = alert_message;
                base_station->node_availabilities[alert_message.reporting_node] = 0;

                printf("node %d\n", base_station->alert_messages[num_alert_messages].reporting_node);

                num_alert_messages += 1;
            }
        }

        MPI_Request report_request[num_alert_messages];
        MPI_Status report_status[num_alert_messages];

        get_timestamp(currentTimestamp);
        printf("[Base Station] timestamp %s %d ALERT MESSAGES READ. DONE READING.\n", currentTimestamp, num_alert_messages);

        for (int i = 0; i < num_alert_messages; i++)
        {
            printf("[Base Station] AFTER node %d \n", base_station->alert_messages[i].reporting_node);
        }

        for (int i = 0; i < num_alert_messages; i++)
        {
            get_timestamp(currentTimestamp);

            printf("[Base Station] timestamp %s\n", currentTimestamp);
            printf("node %d\n", base_station->alert_messages[i].reporting_node);

            int num_neighbours = base_station->alert_messages[i].num_neighbours;
            int max_nearby_nodes = num_neighbours * 4;

            printf("num neighbours %d\n", num_neighbours);

            struct AvailableNodes available_nodes;
            strcpy(available_nodes.timestamp, currentTimestamp);
            available_nodes.size = 0;
            available_nodes.nodes = (int *)malloc(max_nearby_nodes * sizeof(int));

            for (int j = 0; j < num_neighbours; j++)
            {

                int neighbouring_node_rank = base_station->alert_messages[i].neighbouring_nodes[j];
                printf("neighbouring node %d ", neighbouring_node_rank);

                if (base_station->node_availabilities[neighbouring_node_rank] == 1)
                {
                    int row = base_station->alert_messages[i].neighbouring_nodes_coord[j][0];
                    int col = base_station->alert_messages[i].neighbouring_nodes_coord[j][1];

                    get_neighbours(base_station, row, col, available_nodes);
                }
            }
            char received_message_buf[5000];
            sprintf(received_message_buf, "REPORT MESSAGE: { timestamp: %s, node size: %d, available nearby nodes: [ ",
                    available_nodes.timestamp, available_nodes.size);

            int first_entry = 1;
            for (int i = 0; i < available_nodes.size; i++)
            {

                char node_info[2000];

                if (!first_entry)
                {
                    strcat(received_message_buf, ", ");
                }
                else
                {
                    first_entry = 0;
                }

                sprintf(node_info, "node %d", available_nodes.nodes[i]);
                strcat(received_message_buf, node_info);
            }

            strcat(received_message_buf, "]} ");
            log_base_station_event(base_station, received_message_buf);

            printf("reporting to node %d\n", base_station->alert_messages[i].reporting_node + 1);


            MPI_Isend(&available_nodes, 1, MPI_AVAILABLE_NODES, base_station->alert_messages[i].reporting_node + 1, REPORT_TAG, base_station->world_comm, &report_request[i]);

            // log_base_station_event(base_station, received_message_buf);
            free(available_nodes.nodes);
            // free(available_nodes);
        }

        MPI_Waitall(num_alert_messages, report_request, report_status);

        log_base_station_event(base_station, "sent report messages to all nodes \n");

        sleep(2);
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

    char currentTimestamp[TIMESTAMP_LEN];
    get_timestamp(currentTimestamp);
    printf("[Base Station] %s sending termination messages to charging nodes \n", currentTimestamp);

    printf("grid size: %d", base_station->grid_size);

    int sig_term = 1;
    for (int world_rank = 1; world_rank < base_station->grid_size + 1; world_rank++)
    {
        int i = get_index(world_rank);
        printf("world rank %d", world_rank);
        MPI_Isend(&sig_term, 1, MPI_INT, world_rank, TERMINATION_TAG, base_station->world_comm, &send_request[world_rank-1]);
    }

    MPI_Waitall(base_station->grid_size, send_request, send_status);
    get_timestamp(currentTimestamp);
    printf("[Base Station] %s sent termination messages to charging grid \n", currentTimestamp);

    free(base_station->node_availabilities);
    free(base_station->alert_messages);
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

void get_neighbours(struct BaseStation *base_station, int x_row, int y_col, struct AvailableNodes available_nodes)
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
            int index = available_nodes.size;
            available_nodes.nodes[index] = neighbour_nodes[i];
            available_nodes.size += 1;
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

        char reset_message_buf[100];
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
    }
}