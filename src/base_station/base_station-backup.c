// #include "base_station.h"

// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
// #include <time.h>

// #include "mpi.h"

// time_t lastResetTime = 0;

// int get_sig_term();
// int get_world_rank(int i);
// int get_index(int world_rank);
// int get_neighbours(int rank);
// int write_to_logs(struct BaseStation *base_station, struct Log *log);
// void resetNodeAvailabilities(struct BaseStation *base_station);
// void checkResetTimer(struct BaseStation *base_station);

// void log_base_station_event(struct BaseStation *base_station, const char *message)
// {
// 	printf("[Base Station] %s\n", message);
// 	FILE *f;
// 	f = fopen("logs_base_station.txt", "a");
// 	fprintf(f, "[Base Station] %s\n", message); // Log the message
// 	fclose(f);
// }

// struct BaseStation *new_base_station(MPI_Comm world_comm, int grid_size, float cycle_interval, FILE *log_file_handler)
// {
// 	struct BaseStation *base_station = malloc(sizeof(struct BaseStation));
// 	base_station->world_comm = world_comm;
// 	// base_station->grid_comm_cart = grid_comm_cart;
// 	base_station->cycle_interval = cycle_interval;
// 	base_station->log_file_handler = log_file_handler;
// 	base_station->grid_size = grid_size;

// 	base_station->node_availabilities = malloc(sizeof(int) * base_station->grid_size);
// 	for (int i = 0; i < base_station->grid_size; i++)
// 	{
// 		base_station->node_availabilities[i] = 1;
// 	}

// 	// MPI_Comm_size(grid_comm_cart, &base_station->grid_size);
// 	return base_station;
// }

// void start_base_station(struct BaseStation *base_station)
// {
// 	printf("starting base station \n");

// 	MPI_Status probe_status, status;
// 	int has_alert = 0;
// 	int sig_term;

// 	MPI_Datatype MPI_ALERT_MESSAGE;
// 	define_mpi_alert_message(&MPI_ALERT_MESSAGE);

// 	MPI_Datatype MPI_AVAILABLE_NODES;
// 	define_mpi_available_nodes(&MPI_AVAILABLE_NODES);

// 	// TODO: (2f)  send or receive MPI messages using POSIX thread
// 	while (1)
// 	{
// 		int total_alerts, total_sent_messages = 0;

// 		int sig_term = get_sig_term();
// 		if (sig_term == 1)
// 		{
// 			break;
// 		}

// 		checkResetTimer(base_station);

// 		MPI_Iprobe(MPI_ANY_SOURCE, ALERT_TAG, base_station->world_comm, &has_alert, &probe_status);
// 		if (has_alert == 1)
// 		{
// 			int alert_source_rank = probe_status.MPI_SOURCE;

// 			char rev_message_buf[1024];
// 			sprintf(rev_message_buf, "receiving alert message from charging node %d", alert_source_rank - 1);
// 			log_base_station_event(base_station, rev_message_buf);

// 			struct AlertMessage *alert_message;
// 			MPI_Recv(alert_message, 1, MPI_ALERT_MESSAGE, alert_source_rank, ALERT_TAG, base_station->world_comm, &status);

// 			sprintf(rev_message_buf, "received alert message from charging node %d", alert_source_rank - 1);
// 			log_base_station_event(base_station, rev_message_buf);

// 			// TODO: format the receive message

// 			char alert_timestamp = alert_message->timestamp;
// 			int reporting_node = alert_message->reporting_node;

// 			int reporting_node_coord[N_DIMS];

// 			reporting_node_coord[0] = alert_message->reporting_node_coord[0];
// 			reporting_node_coord[1] = alert_message->reporting_node_coord[1];

// 			int num_neighbours = alert_message->num_neighbours;

// 			int neighbouring_nodes[num_neighbours];
// 			int neighbouring_nodes_coord[num_neighbours][N_DIMS];

// 			for (int i = 0; i < num_neighbours; i++)
// 			{
// 				neighbouring_nodes[i] = alert_message->neighbouring_nodes[i];
// 				neighbouring_nodes_coord[i][0] = alert_message->neighbouring_nodes_coord[i][0];
// 				neighbouring_nodes_coord[i][1] = alert_message->neighbouring_nodes_coord[i][1];
// 			};

// 			char alert_message_buf[1024];
// 			sprintf(alert_message_buf, "ALERT MESSAGE = { timestamp: %s, reporting node: %d (%d, %d), neighbouring nodes: [",
// 					alert_message->timestamp, reporting_node, reporting_node_coord[0], reporting_node_coord[1]);

// 			int first_entry = 1;

// 			for (int i = 0; i < num_neighbours; i++)
// 			{
// 				char neighbour_info[516];
// 				if (!first_entry)
// 					strcat(alert_message_buf, ", ");
// 				else
// 					first_entry = 0;
// 				sprintf(neighbour_info, "node %d (%d, %d)", neighbouring_nodes[i], neighbouring_nodes_coord[i][0],
// 						neighbouring_nodes_coord[i][1]);
// 				strcat(alert_message_buf, neighbour_info);
// 			}

// 			strcat(alert_message_buf, "] }");
// 			log_base_station_event(base_station, alert_message_buf);

// 			total_alerts++;

// 			// TODO (2c): report nearest available node
// 			base_station->node_availabilities[reporting_node.id] = 0;

// 			struct AvailableNodes available_nodes;
// 			available_nodes.size = 0;
// 			available_nodes.nodes[base_station->grid_size];

// 			for (int i = 0; i < num_neighbours; i++)
// 			{
// 				if (base_station->node_availabilities[i] == 0)
// 				{
// 					// nearest_neighbours_node[available_nodes.size] = i;
// 					// available_nodes.size += 1;

// 					// report no nearby available nodes
// 				}
// 				else
// 				{
// 					// get neighbours
// 					// check availability
// 					// append nodes
// 				}
// 			}

// 			// Send
// 		}

// 		for (int i = 0; i < base_station->grid_size; i++)
// 		{
// 			int node_availability = base_station->node_availabilities[i];
// 			int alert_source_rank = probe_status.MPI_SOURCE - 1;

// 			// struct AvailableNodes available_nodes;
// 			// available_nodes.size = 0;
// 			// available_nodes.nodes[0] = 1;

// 			// TODO (2c): send list of available nodes to reporting nodes

// 			// 1. checks for the nearest neighbour nodes

// 			/* Upon receiving a report from an EV charging node, the base station checks for
// 			the nearest neighbour nodes that are available based on the neighbouring nodes of
// 			the reporting node.For instance, if the reporting node is Node 0, the neighbour
// 			nodes are Node 1 and 3. The base station will check	whether there’re any reports
// 			received from Node 1 and 3(i.e., Node 2, 4 and 6) :
// 			-If no report is received from
// 			the nodes within a predefined period, the base station will suggest the available
// 			nodes to the reporting node.
// 			- If there are reports received from all of the nodes within a predefined period
// 			the base station will send a message notifying the reporting node that there are
// 			no available nodes nearby.

// 			MPI_Send(base_station->node_availabilities, 1, MPI_AVAILABLE_NODES,
// 			get_world_rank(i), 0, base_station->world_comm);
// 			*/

// 			if (node_availability == 0)
// 			{

// 				// char message[100];
// 				// sprintf(message, "sending node availabilities to node %d", alert_source_rank);
// 				// log_base_station_event(base_station, message);

// 				// MPI_Send(&available_nodes, 1, MPI_AVAILABLE_NODES, get_world_rank(i), 0, base_station->world_comm);
// 				// sprintf(message, "sent node availabilities to node %d", alert_source_rank);
// 				// log_base_station_event(base_station, message);

// 				total_sent_messages++;
// 			}
// 		}

// 		// TODO (2d): write more stats to output file
// 		struct Log *log = malloc(sizeof(struct Log));

// 		char currentTimestamp[TIMESTAMP_LEN];
// 		get_timestamp(currentTimestamp);

// 		strcpy(log->timestamp, currentTimestamp);
// 		log->grid_size = base_station->grid_size;
// 		log->total_alerts = total_alerts;
// 		log->total_sent_messages = total_sent_messages;

// 		write_to_logs(base_station, log);
// 		log_base_station_event(base_station, "listening to charging grid");
// 		sleep(base_station->cycle_interval);
// 	}

// 	MPI_Type_free(&MPI_ALERT_MESSAGE);
// 	MPI_Type_free(&MPI_AVAILABLE_NODES);

// 	printf("closing base station \n");
// 	close_base_station(base_station);
// 	printf("base station closed \n");
// }

// void close_base_station(struct BaseStation *base_station)
// {
// 	MPI_Request send_request[base_station->grid_size];
// 	MPI_Status send_status[base_station->grid_size];

// 	printf("sending termination messages to charging grid \n");

// 	int sig_term = 1;
// 	for (int world_rank = 0; world_rank < base_station->grid_size + 1; world_rank++)
// 	{
// 		int i = get_index(world_rank);
// 		MPI_Isend(&sig_term, 1, MPI_INT, world_rank, TERMINATION_TAG, base_station->world_comm, &send_request[i]);
// 	}

// 	MPI_Waitall(base_station->grid_size, send_request, send_status);
// 	printf("sent termination messages to charging grid \n");

// 	free(base_station->node_availabilities);
// 	free(base_station);
// }

// int get_world_rank(int i)
// {
// 	int world_rank = i;
// 	if (i >= BASE_STATION_RANK)
// 	{
// 		world_rank++;
// 	}
// 	return world_rank;
// }

// int get_index(int world_rank)
// {
// 	int i = world_rank;
// 	if (world_rank == BASE_STATION_RANK)
// 	{
// 		return -1;
// 	}
// 	if (world_rank > BASE_STATION_RANK)
// 	{
// 		i = world_rank - 1;
// 	}
// 	return i;
// }

// // NIT: reuse file handler
// int get_sig_term()
// {
// 	FILE *f;
// 	f = fopen("sig_term.txt", "r");
// 	if (!f)
// 		return -1;
// 	char s[2];
// 	fgets(s, 2, f);
// 	fclose(f);
// 	if (strcmp(s, "0") == 0)
// 		return 1;
// 	else
// 		return 0;
// }

// int get_neighbours(struct BaseStation *base_station, int x_row, int y_col)
// {
// 	int top_rank = (x_row)*n + (y_col - 1);
// 	int bottom_rank = (x_row)*n + (y_col + 1);
// 	int right_rank = (x_row + 1) * n + (y_col);
// 	int left_rank = (x_row - 1) * n + (y_col);

// 	int neighbour_nodes[MAX_NUM_NEIGHBOURS] = {top_rank, bottom_rank, right_rank, left_rank};
// 	int valid_nodes[];

// 	for (int i = 0; i < MAX_NUM_NEIGHBOURS; i++) {
// 		if (neighbour_nodes < base_station->grid_size && neighbour_nodes >= 0) {
// 			valid_nodes.append(neighbour_nodes);
// 		}
// 	}


// 	return neighbour_nodes;
// }

// /**
//  * return value: 0 is success, 1 is failure
//  */
// // TODO: add more stats to logs
// int write_to_logs(struct BaseStation *base_station, struct Log *log)
// {
// 	printf("writing to logs \n");
// 	if (base_station->log_file_handler == NULL)
// 	{
// 		return 1;
// 	}

// 	printf("[%ld] grid size=%d, total alerts=%d, total sent messages=%d\n",
// 		   log->timestamp, log->grid_size, log->total_alerts, log->total_sent_messages);

// 	fprintf(base_station->log_file_handler, "[%ld] grid size=%d, total alerts=%d, total sent messages=%d\n",
// 			log->timestamp, log->grid_size, log->total_alerts, log->total_sent_messages);

// 	printf("wrote to logs \n");

// 	return 0;
// }

// void resetNodeAvailabilities(struct BaseStation *base_station)
// {
// 	for (int i = 0; i < base_station->grid_size; i++)
// 	{
// 		base_station->node_availabilities[i] = 1;
// 	}
// }

// void checkResetTimer(struct BaseStation *base_station)
// {
// 	time_t currentTime = time(NULL);

// 	if (currentTime - lastResetTime >= RESET_INTERVAL_S)
// 	{
// 		resetNodeAvailabilities(base_station);
// 		lastResetTime = currentTime;
// 	}
// }