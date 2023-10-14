#include "base_station.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "mpi.h"
#include "../helpers/helpers.h"

time_t lastResetTime = 0;

MPI_Datatype MPI_ALERT_MESSAGE;
MPI_Datatype MPI_AVAILABLE_NODES;

int get_world_rank(int i);
int get_index(int world_rank);
int write_to_logs(struct BaseStation *base_station, struct Log *log);
void resetNodeAvailabilities(struct BaseStation *base_station);
void checkResetTimer(struct BaseStation *base_station);
void add_to_available_nodes(struct AvailableNodes *available_nodes, int node);
void get_nearby_nodes(struct BaseStation *base_station, int x_row, int y_col, struct AvailableNodes *available_nodes);

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
    base_station->num_alert_messages = 0;

    // MPI_Comm_size(grid_comm, &base_station->grid_size);
    return base_station;
}

void start_base_station(struct BaseStation *base_station)
{
    printf("starting base station \n");

    int num_iter = 0;
    define_mpi_alert_message(&MPI_ALERT_MESSAGE);
    define_mpi_available_nodes(&MPI_AVAILABLE_NODES);

    // TODO: (2f)  send or receive MPI messages using POSIX thread
    while (num_iter < ITERATION)
    {

        num_iter++;
        receive_alert_message(base_station);
        send_report_messages(base_station);
        checkResetTimer(base_station);
        sleep(8);
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

    int sig_term = 1;
    for (int world_rank = 0; world_rank < base_station->grid_size + 1; world_rank++)
    {
        int i = get_index(world_rank);
        MPI_Isend(&sig_term, 1, MPI_INT, world_rank, TERMINATION_TAG, base_station->world_comm, &send_request[world_rank - 1]);
    }

    MPI_Waitall(base_station->grid_size, send_request, send_status);
    get_timestamp(currentTimestamp);
    printf("[Base Station] %s sent termination messages to charging grid \n", currentTimestamp);

    free(base_station->node_availabilities);
    free(base_station->alert_messages);
    free(base_station);
}

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

        MPI_Iprobe(i, ALERT_TAG, base_station->world_comm, &has_alert, &probe_status);

        if (has_alert == 1)
        {
            process_alert_message(base_station, i, probe_status, status);
        }
    }
}

void process_alert_message(struct BaseStation *base_station, int source_rank, MPI_Status *probe_status, MPI_Status *status)
{
    char currentTimestamp[TIMESTAMP_LEN];
    get_timestamp(currentTimestamp);
    printf("[Base Station] timestamp %s %d ALERT MESSAGES READ\n", currentTimestamp, base_station->num_alert_messages);

    char rev_message_buf[1024];
    sprintf(rev_message_buf, "receiving alert message from charging node %d", (source_rank - 1));
    log_base_station_event(base_station, rev_message_buf);

    struct AlertMessage alert_message;
    MPI_Recv(&alert_message, 1, MPI_ALERT_MESSAGE, source_rank, ALERT_TAG, base_station->world_comm, &status);

    sprintf(rev_message_buf, "received alert message from charging node %d", (source_rank - 1));
    log_base_station_event(base_station, rev_message_buf);

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

    char alert_message_buf[2024];
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

    base_station->alert_messages[base_station->num_alert_messages] = alert_message;
    base_station->node_availabilities[alert_message.reporting_node] = 0;
    base_station->num_alert_messages++;

    get_timestamp(currentTimestamp);
    printf("[Base Station] timestamp %s %d ALERT MESSAGES READ. DONE READING.\n", currentTimestamp, base_station->num_alert_messages);
}

void send_report_messages(struct BaseStation *base_station)
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

        // int num_neighbours = base_station->alert_messages[i].num_neighbours;
        // get_timestamp(currentTimestamp);

        // struct AvailableNodes available_nodes;
        // strcpy(available_nodes.timestamp, currentTimestamp);
        // available_nodes.size = 0;
        // available_nodes.nodes[MAX_NUM_NEIGHBOURS * MAX_NUM_NEIGHBOURS];

        // for (int j = 0; j < num_neighbours; j++)
        // {

        //     int neighbouring_node_rank = base_station->alert_messages[i].neighbouring_nodes[j];
        //     // printf("neighbouring node %d \n", neighbouring_node_rank);

        //     if (base_station->node_availabilities[neighbouring_node_rank] == 1)
        //     {
        //         int row = base_station->alert_messages[i].neighbouring_nodes_coord[j][0];
        //         int col = base_station->alert_messages[i].neighbouring_nodes_coord[j][1];

        //         get_nearby_nodes(base_station, row, col, &available_nodes);
        //     }
        // }

        // MPI_Isend(&available_nodes, 1, MPI_AVAILABLE_NODES, base_station->alert_messages[i].reporting_node + 1, REPORT_TAG, base_station->world_comm, &report_request[i]);

        // char received_message_buf[2000];
        // sprintf(received_message_buf, "REPORTING NODE %d: REPORT MESSAGE: { timestamp: %s, node size: %d, available nearby nodes: [ ",
        //         base_station->alert_messages[i].reporting_node, available_nodes.timestamp, available_nodes.size);

        // int first_entry = 1;
        // for (int i = 0; i < available_nodes.size; i++)
        // {

        //     char node_info[2000];

        //     if (!first_entry)
        //     {
        //         strcat(received_message_buf, ", ");
        //     }
        //     else
        //     {
        //         first_entry = 0;
        //     }

        //     sprintf(node_info, "node %d", available_nodes.nodes[i]);
        //     strcat(received_message_buf, node_info);
        // }
        
        // strcat(received_message_buf, "]} ");
        // log_base_station_event(base_station, received_message_buf);
    }

    MPI_Waitall(base_station->num_alert_messages, report_request, report_status);

    strcat(reporting_node_buf, "]");
    log_base_station_event(base_station, reporting_node_buf);
    log_base_station_event(base_station, "sent report messages to all nodes \n");
}

void send_to_reporting_node(struct BaseStation *base_station, int i, MPI_Request *report_request)
{
    int num_neighbours = base_station->alert_messages[i].num_neighbours;

    char currentTimestamp[TIMESTAMP_LEN];
    get_timestamp(currentTimestamp);

    struct AvailableNodes available_nodes;
    strcpy(available_nodes.timestamp, currentTimestamp);
    available_nodes.size = 0;
    available_nodes.nodes[MAX_NUM_NEIGHBOURS * MAX_NUM_NEIGHBOURS];

    for (int j = 0; j < num_neighbours; j++)
    {

        int neighbouring_node_rank = base_station->alert_messages[i].neighbouring_nodes[j];
        // printf("neighbouring node %d \n", neighbouring_node_rank);

        if (base_station->node_availabilities[neighbouring_node_rank] == 1)
        {
            int row = base_station->alert_messages[i].neighbouring_nodes_coord[j][0];
            int col = base_station->alert_messages[i].neighbouring_nodes_coord[j][1];

            get_nearby_nodes(base_station, row, col, &available_nodes);
        }
    }

    MPI_Isend(&available_nodes, 1, MPI_AVAILABLE_NODES, base_station->alert_messages[i].reporting_node + 1, REPORT_TAG, base_station->world_comm, &report_request[i]);

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

void add_to_available_nodes(struct AvailableNodes *available_nodes, int node)
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
    }
}

void get_nearby_nodes(struct BaseStation *base_station, int x_row, int y_col, struct AvailableNodes *available_nodes)
{
    int top_rank = (x_row)*n + (y_col - 1);
    int bottom_rank = (x_row)*n + (y_col + 1);
    int right_rank = (x_row + 1) * n + (y_col);
    int left_rank = (x_row - 1) * n + (y_col);

    int neighbour_nodes[MAX_NUM_NEIGHBOURS] = {top_rank, bottom_rank, left_rank, right_rank};

    for (int i = 0; i < MAX_NUM_NEIGHBOURS; i++)
    {
        if (neighbour_nodes[i] >= 0 && neighbour_nodes[i] < base_station->grid_size && base_station->node_availabilities[neighbour_nodes[i]] == 1)
        {
            add_to_available_nodes(available_nodes, neighbour_nodes[i]);
        }
    }
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