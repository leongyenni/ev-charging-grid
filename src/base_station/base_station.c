#include "base_station.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "mpi.h"
#include "../helpers/helpers.h"

time_t lastResetTime = 0;

void receive_alert_message(struct BaseStation *base_station);
void process_alert_message(struct BaseStation *base_station, struct AlertMessage *alert_message);
void send_available_nodes_message(struct BaseStation *base_station);
void write_to_logs(struct BaseStation *base_station, struct Log *logger);
void reset_availabilities(struct BaseStation *base_station);
void reset_timer(struct BaseStation *base_station);
void add_to_available_nodes(struct AvailableNodes *available_nodes, struct Log *logger, int node);
void get_nearby_nodes(struct BaseStation *base_station, struct Log *logger, int x_row, int y_col, struct AvailableNodes *available_nodes);

void log_base_station_event(struct BaseStation *base_station, const char *message)
{
    printf("[Base Station] %s\n", message);
}

struct BaseStation *new_base_station(MPI_Comm world_comm, int grid_size, float listening_frenquency_s, FILE *log_file_handler)
{
    struct BaseStation *base_station = malloc(sizeof(struct BaseStation));
    base_station->world_comm = world_comm;
    base_station->listen_frequency_s = CYCLE_INTERVAL_S;
    base_station->log_file_handler = log_file_handler;
    base_station->grid_size = grid_size;

    base_station->node_availabilities = malloc(sizeof(int) * base_station->grid_size);
    base_station->nearby_availabilities = malloc(sizeof(int) * base_station->grid_size);
    for (int i = 0; i < base_station->grid_size; i++)
    {
        base_station->node_availabilities[i] = 1;
        base_station->nearby_availabilities[i] = 1;
    }

    base_station->alert_messages = malloc(sizeof(struct AlertMessage) * base_station->grid_size);
    base_station->num_alert_messages = 0;
    base_station->num_iter = 0;

    base_station->logger = malloc(sizeof(struct Log) * base_station->grid_size);

    return base_station;
}

/* Start base station */
void start_base_station(struct BaseStation *base_station)
{
    printf("starting base station \n");

    // TODO: (2f)  send or receive MPI messages using POSIX thread
    while (base_station->num_iter < ITERATION)
    {

        base_station->num_iter++;
        receive_alert_message(base_station);
        send_available_nodes_message(base_station);
        reset_timer(base_station);
        sleep(15);
    }

    printf("closing base station \n");
    close_base_station(base_station);
    printf("base station closed \n");
}

/* Terminate base station */
void close_base_station(struct BaseStation *base_station)
{
    MPI_Request send_request[base_station->grid_size];
    MPI_Status send_status[base_station->grid_size];

    char currentTimestamp[TIMESTAMP_LEN];
    get_timestamp(currentTimestamp);
    printf("[Base Station] %s sending termination messages to charging nodes \n", currentTimestamp);

    // Send termination message to all charging nodes 
    int sig_term = 1;
    for (int world_rank = 0; world_rank < base_station->grid_size + 1; world_rank++)
    {
        MPI_Isend(&sig_term, 1, MPI_INT, world_rank, TERMINATION_TAG, base_station->world_comm, &send_request[world_rank - 1]);
    }

    MPI_Waitall(base_station->grid_size, send_request, send_status);

    get_timestamp(currentTimestamp);
    printf("[Base Station] %s sent termination messages to charging grid \n", currentTimestamp);

    // Deallocate memory 
    free(base_station->node_availabilities);
    free(base_station->nearby_availabilities);
    free(base_station->alert_messages);
    free(base_station->logger);
    free(base_station);
}

/* Receive message from charging nodes */
void receive_alert_message(struct BaseStation *base_station)
{
    MPI_Status probe_status, status;
    char currentTimestamp[TIMESTAMP_LEN];
    base_station->num_alert_messages = 0;
    base_station->alert_messages = malloc(sizeof(struct AlertMessage) * base_station->grid_size);

    for (int i = 1; i <= base_station->grid_size; i++)
    {
        int has_alert = 0;
        get_timestamp(currentTimestamp);

        char listening_message_buf[100];
        sprintf(listening_message_buf, "%s listening to node %d", currentTimestamp, i - 1);
        log_base_station_event(base_station, listening_message_buf);

        // Test if a charging node send an alert message 
        MPI_Iprobe(i, ALERT_TAG, base_station->world_comm, &has_alert, &probe_status);

        if (has_alert == 1)
        {
            char rev_message_buf[1024];
            sprintf(rev_message_buf, "receiving alert message from charging node %d", (i - 1));
            log_base_station_event(base_station, rev_message_buf);

            struct AlertMessage alert_message;
            MPI_Recv(&alert_message, 1, MPI_ALERT_MESSAGE, i, ALERT_TAG, base_station->world_comm, &status);
            base_station->alert_messages[base_station->num_alert_messages] = alert_message;

            sprintf(rev_message_buf, "received alert message from charging node %d", (i - 1));
            log_base_station_event(base_station, rev_message_buf);

            process_alert_message(base_station, &alert_message);
        }
    }

    get_timestamp(currentTimestamp);
    printf("[Base Station] timestamp %s %d ALERT MESSAGES READ\n", currentTimestamp, base_station->num_alert_messages);
}

/* Process received message */
void process_alert_message(struct BaseStation *base_station, struct AlertMessage *alert_message)
{
    int num_neighbours = alert_message->num_neighbours;
    int reporting_node = alert_message->reporting_node;

    // Record received data in logger
    strcpy(base_station->logger[reporting_node].alert_timestamp, alert_message->timestamp);
    base_station->logger[reporting_node].reporting_node = reporting_node;
    base_station->logger[reporting_node].reporting_node_coord[0] = alert_message->reporting_node_coord[0];
    base_station->logger[reporting_node].reporting_node_coord[1] = alert_message->reporting_node_coord[1];
    base_station->logger[reporting_node].num_neighbours = num_neighbours;
    base_station->logger[reporting_node].num_port = 0;
    base_station->logger[reporting_node].num_messages_sent = 1;
    base_station->logger[reporting_node].num_iter = base_station->num_iter;

    for (int i = 0; i < num_neighbours; i++)
    {
        base_station->logger[reporting_node].neighbouring_nodes[i] = alert_message->neighbouring_nodes[i];
        base_station->logger[reporting_node].neighbouring_nodes_coord[i][0] = alert_message->neighbouring_nodes_coord[i][0];
        base_station->logger[reporting_node].neighbouring_nodes_coord[i][1] = alert_message->neighbouring_nodes_coord[i][1];
    }

    int reporting_node_coord[N_DIMS];
    reporting_node_coord[0] = alert_message->reporting_node_coord[0];
    reporting_node_coord[1] = alert_message->reporting_node_coord[1];
    int neighbouring_nodes[num_neighbours];
    int neighbouring_nodes_coord[num_neighbours][N_DIMS];

    for (int i = 0; i < num_neighbours; i++)
    {
        neighbouring_nodes[i] = alert_message->neighbouring_nodes[i];
        neighbouring_nodes_coord[i][0] = alert_message->neighbouring_nodes_coord[i][0];
        neighbouring_nodes_coord[i][1] = alert_message->neighbouring_nodes_coord[i][1];
    };

    char alert_message_buf[2024];
    sprintf(alert_message_buf, "ALERT MESSAGE = { timestamp: %s, reporting node: %d (%d, %d), neighbouring nodes: [",
            alert_message->timestamp, reporting_node, reporting_node_coord[0], reporting_node_coord[1]);

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

    // Update node availabilities
    base_station->node_availabilities[reporting_node] = 0;
    base_station->nearby_availabilities[reporting_node] = 0;

    for (int i = 0; i < num_neighbours; i++)
    {
        int neighbouring_node = alert_message->neighbouring_nodes[i];
        base_station->nearby_availabilities[neighbouring_node] = 0;
    }

    // Update no. alert messages received 
    base_station->num_alert_messages++;

    char currentTimestamp[TIMESTAMP_LEN];
    get_timestamp(currentTimestamp);

    printf("[Base Station] timestamp %s %d ALERT MESSAGES READ. DONE READING.\n", currentTimestamp, base_station->num_alert_messages);
}

/* Send a list of available nodes to all reporting nodes */
void send_available_nodes_message(struct BaseStation *base_station)
{
    MPI_Request report_request[base_station->num_alert_messages];
    MPI_Status report_status[base_station->num_alert_messages];

    char reporting_node_buf[500];
    sprintf(reporting_node_buf, "Reporting node: [");

    int first_entry = 1;
    for (int i = 0; i < base_station->num_alert_messages; i++)
    {
        char node_buf[100];
        if (!first_entry)
        {
            strcat(reporting_node_buf, ", ");
        }
        else
        {
            first_entry = 0;
        }
        sprintf(node_buf, "Node %d", base_station->alert_messages[i].reporting_node);
        strcat(reporting_node_buf, node_buf);

        send_to_reporting_node(base_station, i, report_request);
    }

    MPI_Waitall(base_station->num_alert_messages, report_request, report_status);

    strcat(reporting_node_buf, "]");
    log_base_station_event(base_station, reporting_node_buf);
    log_base_station_event(base_station, "sent report messages to all nodes \n");
}

/* Send a list of adjacent node to each reporting node */
void send_to_reporting_node(struct BaseStation *base_station, int i, MPI_Request *report_request)
{
    int num_neighbours = base_station->alert_messages[i].num_neighbours;
    int reporting_node = base_station->alert_messages[i].reporting_node;

    char currentTimestamp[TIMESTAMP_LEN];
    get_timestamp(currentTimestamp);

    struct AvailableNodes available_nodes;
    strcpy(available_nodes.timestamp, currentTimestamp);
    available_nodes.size = 0;
    available_nodes.nodes[MAX_NUM_NEARBY];

    base_station->logger[reporting_node].num_nearby_nodes = 0;

    for (int j = 0; j < num_neighbours; j++)
    {

        int neighbouring_node_rank = base_station->alert_messages[i].neighbouring_nodes[j];

        if (base_station->node_availabilities[neighbouring_node_rank] == 1)
        {
            int row = base_station->alert_messages[i].neighbouring_nodes_coord[j][0];
            int col = base_station->alert_messages[i].neighbouring_nodes_coord[j][1];

            get_nearby_nodes(base_station, &base_station->logger[reporting_node], row, col, &available_nodes);
        }
    }

    MPI_Isend(&available_nodes, 1, MPI_AVAILABLE_NODES, base_station->alert_messages[i].reporting_node + 1, REPORT_TAG, base_station->world_comm, &report_request[i]);

    base_station->logger[reporting_node].num_messages_sent++;
    get_timestamp(base_station->logger[reporting_node].logged_timestamp);


    // Base station log
    write_to_logs(base_station, &base_station->logger[reporting_node]);

    char received_message_buf[2000];
    sprintf(received_message_buf, "REPORTING NODE %d: REPORT MESSAGE: { timestamp: %s, node size: %d, available nearby nodes: [ ",
            base_station->alert_messages[i].reporting_node, available_nodes.timestamp, available_nodes.size);

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
}

/* Get the nearby nodes */
void get_nearby_nodes(struct BaseStation *base_station, struct Log *logger, int x_row, int y_col, struct AvailableNodes *available_nodes)
{
    int top_rank = (x_row)*n + (y_col - 1);
    int bottom_rank = (x_row)*n + (y_col + 1);
    int right_rank = (x_row + 1) * n + (y_col);
    int left_rank = (x_row - 1) * n + (y_col);

    int nearby_nodes[MAX_NUM_NEIGHBOURS] = {top_rank, bottom_rank, left_rank, right_rank};

    for (int i = 0; i < MAX_NUM_NEIGHBOURS; i++)
    {
        if (is_valid_rank(nearby_nodes[i], base_station->grid_size))
        {
            add_to_nearby_nodes(logger, nearby_nodes[i]);

            if (base_station->nearby_availabilities[nearby_nodes[i]] == 1)
            {
                add_to_available_nodes(available_nodes, logger, nearby_nodes[i]);
            }
        }
    }
}

/* Check if a rank is a valid */
int is_valid_rank(int rank, int grid_size)
{
    return rank >= 0 && rank < grid_size;
}

/* Add to a list of nearby nodes */
void add_to_nearby_nodes(struct Log *logger, int node)
{
    int already_exists = 0;

    for (int i = 0; i < logger->num_nearby_nodes; i++)
    {
        if (logger->nearby_nodes[i] == node)
        {
            already_exists = 1;
            break;
        }
    }

    if (!already_exists)
    {
        int index = logger->num_nearby_nodes;
        logger->nearby_nodes[index] = node;
        logger->num_nearby_nodes += 1;
        get_coordinates(node, &logger->nearby_nodes_coord[index][0], &logger->nearby_nodes_coord[index][1]);
    }
}

/* Add to a list of available nodes */
void add_to_available_nodes(struct AvailableNodes *available_nodes, struct Log *logger, int node)
{
    int already_exists = 0;

    for (int i = 0; i < available_nodes->size; i++)
    {
        if (available_nodes->nodes[i] == node)
        {
            already_exists = 1;
            break;
        }
    }

    if (!already_exists)
    {
        int index = available_nodes->size;
        available_nodes->nodes[index] = node;
        available_nodes->size += 1;

        logger->available_nodes[index] = node;
        logger->num_available_nodes += 1;
    }
}

/* Get the coordinates of a rank*/
void get_coordinates(int rank, int *row, int *col)
{
    *row = rank / n;
    *col = rank % n;
}

/* Reset node availability */
void reset_availabilities(struct BaseStation *base_station)
{
    for (int i = 0; i < base_station->grid_size; i++)
    {
        base_station->node_availabilities[i] = 1;
        base_station->nearby_availabilities[i] = 1;
    }
}

/* Reset node availability within a timeframe */
void reset_timer(struct BaseStation *base_station)
{
    time_t currentTime = time(NULL);

    if (currentTime - lastResetTime >= RESET_INTERVAL_S)
    {
        reset_availabilities(base_station);
        lastResetTime = currentTime;

        char reset_message_buf[1000];
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

/**
 * return value: 0 is success, 1 is failure
 */
// TODO: add more stats to logs
void write_to_logs(struct BaseStation *base_station, struct Log *logger)
{
    printf("WRITE TO LOGS TIMESTAMP %s", logger->alert_timestamp);
    log_base_station_event(base_station, "writing to logs");

    FILE *f;
    f = fopen("logs.txt", "a");
    if (f == NULL)
    {
        perror("Error opening file");
        return; // Exit the function if the file cannot be opened
    }
    fprintf(f, "Iteration: %d\n", logger->num_iter);
    fprintf(f, "Logged time:          %s\n", logger->logged_timestamp);
    fprintf(f, "Alert reported time:  %s\n", logger->alert_timestamp);
    fprintf(f, "No. of adjacent nodes: %d\n", logger->num_neighbours);
    fprintf(f, "No. of available nearby nodes: %d\n", logger->num_available_nodes);

    fprintf(f, "\n");

    fprintf(f, "+------------------------------------------------------------------------+\n");
    fprintf(f, "|  Reporting Node    |   Coord    |   Port Value  |    Available Port    |\n");
    fprintf(f, "+------------------------------------------------------------------------+\n");
    fprintf(f, "|        %d           |  ( %d, %d )  |       %d       |          %d           |\n",
            logger->reporting_node, logger->reporting_node_coord[0], logger->reporting_node_coord[1], NUM_PORTS, 0);
    fprintf(f, "+------------------------------------------------------------------------+\n");

    fprintf(f, "\n");

    fprintf(f, "+------------------------------------------------------------------------+\n");
    fprintf(f, "|   Adjacent Nodes   |   Coord     |  Port Value  |    Available Port    |\n");
    fprintf(f, "+------------------------------------------------------------------------+\n");

    for (int i = 0; i < logger->num_neighbours; i++)
    {
        fprintf(f, "|        %d           |   ( %d, %d )  |      %d       |          %d           |\n",
                logger->neighbouring_nodes[i], logger->neighbouring_nodes_coord[i][0], logger->neighbouring_nodes_coord[i][1], NUM_PORTS, 0);
        fprintf(f, "+------------------------------------------------------------------------+\n");
    }

    fprintf(f, "\n");

    fprintf(f, "+---------------------------------+\n");
    fprintf(f, "|   Nearby Nodes    |   Coord     |\n");
    fprintf(f, "+---------------------------------+\n");
    for (int i = 0; i < logger->num_nearby_nodes; i++)
    {
        fprintf(f, "|        %d          |   ( %d, %d )  |\n",
                logger->nearby_nodes[i], logger->nearby_nodes_coord[i][0], logger->nearby_nodes_coord[i][1]);
        fprintf(f, "+----------------------------------+\n");
    }

    fprintf(f, "\n");

    fprintf(f, "Available nearby nodes (no report in last 3 iterations): ");

    int first_entry = 1;
    for (int i = 0; i < logger->num_available_nodes; i++)
    {
        if (!first_entry)
        {
            fprintf(f, ", ");
        }
        else
        {
            first_entry = 0;
        }
        fprintf(f, "%d", logger->available_nodes[i]);
    }

    fprintf(f, "\n");

    time_t startTime = timestampToTimeT(logger->alert_timestamp);
    time_t endTime = timestampToTimeT(logger->logged_timestamp);

    time_t timeDifference = difftime(endTime, startTime);
    double timeInSeconds = (double)timeDifference;

    fprintf(f, "Communication time (s): %.2lf\n", timeInSeconds);
    fprintf(f, "Total messages send between reporting node and base station: %d\n", logger->num_messages_sent);

    fprintf(f, "------------------------------------------------------------------------------------------------------------------------");
    fprintf(f, "\n\n\n");

    fclose(f);
}