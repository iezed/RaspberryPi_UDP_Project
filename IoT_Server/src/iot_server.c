/*
 ============================================================================
 Name        : iot_server.c
 Author      : Nicolas Villanueva
 Version     : 1.0.0 (April 2020)
 Description : IoT server program to run in Ubuntu host.
 ============================================================================
 */


#include <stdio.h>			// For printf() and basic C utilities
#include <stdlib.h>			// For exit code
#include <string.h>			// For memset()
#include <stdbool.h>		// For bool

#include <sys/socket.h> 	// For socket()
#include <netinet/udp.h>	// For socket()
#include <sys/types.h>		// For bind()
#include <unistd.h>			// For close()
#include <arpa/inet.h>		// For inet_aton()
#include <sys/time.h>		// For timeval struct

#include "iot_lib.h"
#include "iot_server.h"




int main(int argc, char* argv[]) {

	/* STEP 1 - Parse command line parameters: sampling rate and server streaming rate */
	timing_rates timings;
	parse_param_rates(&timings, argc, argv);


	/* STEP 2 - Initialize UDP communication socket */
	struct sockaddr_in server_addr;
	struct timeval intervals;
	intervals.tv_sec = 1;
	intervals.tv_usec = 0;
	int server_socket = server_socket_init(&server_addr, &intervals);

	int samples_all_index = 0;
	sample_data samples_all		[MAX_RATE_SERVER_STATS_CALC];
	sample_data samples_stream	[MAX_SAMPLING_RATIO];


	server_stats stats = { -1, -1, -1 };
	int comm_established_flag = false;
	int stats_secs = 0;
	while(1) {
		/* STEP 3 - Process incoming datagrams from client */

		struct sockaddr_in client_addr;
		uint8_t buffer_recv[DATAGRAM_SIZE] = {'\0'};
		int recv_len = server_socket_listen(server_socket, &client_addr, comm_established_flag, timings.server_stats_calc, &stats_secs, buffer_recv);
		comm_established_flag = 1;

		/* STEP 4 - After datagram reception, reply to client, then parse and save data */

		if (recv_len > 0) {
			server_socket_reply(server_socket, &client_addr, buffer_recv, &timings);

			int n_samples = server_datagram_parsing(buffer_recv, samples_stream);
			server_save_samples(samples_stream, n_samples, samples_all, &samples_all_index);
			memset(samples_stream, 0, MAX_SAMPLING_RATIO);
		}

		/* STEP 5 - For stats timeout, compute statistics for current data */

		else {
			stats_secs = 0;
			server_compute_stats(samples_all, &samples_all_index, &stats);
		}


	}


	close(server_socket);
	return EXIT_SUCCESS;
}





/* parse_param_rates
 * return 0 for successful parsing, 1 for incorrect format
 */
void parse_param_rates (timing_rates* timings, int argc, char* argv[]) {

	int sampling, server_data, server_stats_calc;
	switch(argc) {
		// No parameters provided: default values
		case 1:
			timings->sampling = DEFAULT_RATE_SAMPLING;
			timings->server_stream = DEFAULT_RATE_SERVER_STREAM;
			timings->server_stats_calc = DEFAULT_RATE_SERVER_STATS_CALC;
			break;

		// Parameters provided: parse and save for subsequent datagram to client
		case 3:
			sampling = atoi(argv[1]);
			server_data = atoi(argv[2]);
			if (sampling && server_data && (sampling <= server_data) && ((server_data / sampling) <= MAX_SAMPLING_RATIO)) {
				timings->sampling = sampling;
				timings->server_stream = server_data;
				timings->server_stats_calc = DEFAULT_RATE_SERVER_STATS_CALC;
			} else {
				print_error_server(4);
				exit(EXIT_FAILURE);
			}
			break;

		case 4:
			sampling = atoi(argv[1]);
			server_data = atoi(argv[2]);
			server_stats_calc = atoi(argv[3]);
			if (sampling && server_data && (sampling <= server_data) && ((server_data / sampling) <= MAX_SAMPLING_RATIO) && ((server_data*2) <= server_stats_calc) && (server_stats_calc <= 600)) {
				timings->sampling = sampling;
				timings->server_stream = server_data;
				timings->server_stats_calc = server_stats_calc;
			} else {
				print_error_server(4);
				exit(EXIT_FAILURE);
			}
			break;

		// Wrong number of parameters
		default:
			print_error_server(4);
			exit(EXIT_FAILURE);
	}
}





/**
 * server_socket_init
 * returns socket descriptor bound to UDP server
 */
int server_socket_init(struct sockaddr_in* server_addr, struct timeval *intervals) {

	/* Create UDP/IP socket */
	int server_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (server_socket < 0) {
		print_error_server(1);
		exit(EXIT_FAILURE);
	}

	/* Timeout socket every 1 second */
	if (setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, intervals, sizeof(*intervals)) < 0) {
		print_error_server(5);
		exit(EXIT_FAILURE);
	}
	printf("IOT_SERVER: Initialized server UDP socket (descriptor %d)\n", server_socket);


	/* Fill socket address structure */
	memset(server_addr, 0, sizeof(*server_addr));
	server_addr->sin_family = AF_INET;
	server_addr->sin_port = htons(SERVER_PORT);
	server_addr->sin_addr.s_addr = htonl(INADDR_ANY);		// int ia = inet_aton(SERVER_ADDR, &server_addr.sin_addr);


	/* Bind/Assign address to server */
	if (bind(server_socket, (struct sockaddr *) server_addr, sizeof(*server_addr)) < 0) {
		print_error_server(2);
		exit(EXIT_FAILURE);
	}
	server_socket_print_info(server_addr);

	return server_socket;
}





/**
 * server_socket_print_info
 * shows socket address and port values in console
 */
void server_socket_print_info(struct sockaddr_in* sockaddr) {

	/* Print socket address' IP and UDP port */
	int port = (int) ntohs(sockaddr->sin_port);
	unsigned long int ip_unparsed = (unsigned long int) ntohl(sockaddr->sin_addr.s_addr);

	short ip_1, ip_2, ip_3, ip_4;
	ip_4 = ip_unparsed % 256;
	ip_unparsed /= 256;
	ip_3 = ip_unparsed % 256;
	ip_unparsed /= 256;
	ip_2 = ip_unparsed % 256;
	ip_unparsed /= 256;
	ip_1 = ip_unparsed % 256;

	printf("IOT_SERVER: Bound address to socket: Address %hd.%hd.%hd.%hd - port %d.\n", ip_1, ip_2, ip_3, ip_4, port);
}





/**
 * server_socket_listen
 * Halts execution until datagram arrives from client
 * return length of received data (-1 if no data is received: stats_flag triggered)
 */
int server_socket_listen(int server_socket, struct sockaddr_in *client_addr, int comm_established_flag, int server_stats_timeout, int *stats_secs, uint8_t* buffer_recv) {

	/* Clear reception buffer and client address structure */
	memset(buffer_recv, 0, DATAGRAM_SIZE);
	memset(client_addr, 0, sizeof(*client_addr));
	socklen_t client_addr_len = sizeof(*client_addr);

	/* Waits for datagram */
	int stats_flag = 0;
	ssize_t recv_len = -1;

	printf("IOT_SERVER: Waiting to receive datagram...\n");
	while ((recv_len < 0) && (stats_flag == 0)) {
		recv_len = recvfrom(server_socket, buffer_recv, DATAGRAM_SIZE, 0, (struct sockaddr *) client_addr, &client_addr_len);

		if (comm_established_flag == 1) {
			*stats_secs += 1;
			if (*stats_secs >= server_stats_timeout) {
				stats_flag = 1;
			}
		}
	}

	// Parse message and print buffer information
	if (stats_flag == 0) {
		printf("IOT_SERVER: Received %d-byte datagram from %s:%d\n", (int) recv_len, inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
	} else {
		printf("IOT_SERVER: statistics calculation flag triggered after %d seconds\n", *stats_secs);
	}

	return (int) recv_len;

}





/**
 * server_socket_reply
 * Parses received datagram, and builds and sends response
 */
void server_socket_reply(int server_socket, struct sockaddr_in *client_addr, uint8_t* buffer_recv, timing_rates* timings) {

	/* Build and send UDP reply to client */
	uint8_t buffer_reply[DATAGRAM_SIZE] = {"\0"};
	server_build_reply(server_socket, buffer_recv, buffer_reply, timings);

	ssize_t send_len = sendto(server_socket, buffer_reply, (((int) (buffer_reply[2] << 8) | (buffer_reply[1])) + DATAGRAM_HEADER_SIZE + 1), 0, (struct sockaddr *) client_addr, sizeof(*client_addr));
	printf("Sent %d-byte response\n", (int) send_len);

}




/**
 * server_build_reply
 * Build reply to client according to request type received
 */
void server_build_reply(int server_socket, uint8_t* buffer_recv, uint8_t* buffer_reply, timing_rates* timings) {

	uint8_t request_type = buffer_recv[0];

	switch(request_type) {
		case DATAGRAM_REQ_COMM:
			buffer_reply[0] = DATAGRAM_REP_COMM_OK;
			buffer_reply[1] = 0x02;
			buffer_reply[2] = 0x00;
			buffer_reply[3] = timings->sampling;
			buffer_reply[4] = timings->server_stream;
			break;

		case DATAGRAM_REQ_SEND_DATA:
			buffer_reply[0] = DATAGRAM_REP_SEND_DATA_OK;
			buffer_reply[1] = 0x00;
			buffer_reply[2] = 0x00;
			break;

		default:
			buffer_reply[0] = DATAGRAM_REP_ERROR;
			buffer_reply[1] = 0x00;
			buffer_reply[2] = 0x00;
			break;
	}
}





/**
 * server_datagram_parsing
 * parses datagram received from client
 * returns number of samples parsed
 */
// float data_out[][4]
int server_datagram_parsing(uint8_t* buffer_recv, sample_data* data_out) {

	int n_samples = (int) ((buffer_recv[2] << 8) | (buffer_recv[1])) / 8;

	uint8_t	samples_raw	[n_samples][DATAGRAM_SAMPLE_SIZE];

	int sample;
	for (sample = 0; sample < n_samples; sample++) {
		/* Parse samples into (row-per-sample)-matrix from raw buffer vector */
		int index;
		for (index = 0; index < DATAGRAM_SAMPLE_SIZE; index++) {
			samples_raw[sample][index] = buffer_recv[DATAGRAM_HEADER_SIZE + (sample * DATAGRAM_SAMPLE_SIZE) + index];
		}


		// Merge 8-bit register sensor data into 16-bit structures
		uint16_t conversions_16[5];
		conversions_16[0] = (uint16_t) (samples_raw[sample][1] << 8 | samples_raw[sample][0]);
		conversions_16[1] = (uint16_t) (samples_raw[sample][3] << 8 | samples_raw[sample][2]);
		conversions_16[2] = (uint16_t) (samples_raw[sample][5] << 8 | samples_raw[sample][4]);
		conversions_16[3] = (uint16_t) (samples_raw[sample][7] << 8 | samples_raw[sample][6]);
		conversions_16[4] = (uint16_t) (samples_raw[sample][9] << 8 | samples_raw[sample][8]);

		// Convert into percentage-based floating-point numbers.
		data_out[sample].timestamp = (long int) conversions_16[0];
		data_out[sample].clarity = (float) conversions_16[1] / 655.35;
		data_out[sample].red = (float) conversions_16[2] / 655.35;
		data_out[sample].green = (float) conversions_16[3] / 655.35;
		data_out[sample].blue = (float) conversions_16[4] / 655.35;

		printf("IOT_SERVER: Sample %d from sensor at %ld seconds: Clarity %.2f %% - Red: %.2f %% - Green: %.2f %% - Blue: %.2f %% \n",
				sample, data_out[sample].timestamp, data_out[sample].clarity, data_out[sample].red, data_out[sample].green, data_out[sample].blue);
	}
	printf("\n");

	return n_samples;
}





/**
 * server_save_samples
 * save sample_stream received from server into persistent array
 */
void server_save_samples(sample_data* samples_stream, int n_samples, sample_data* samples_all, int* samples_all_index) {

	int sample;
	for(sample = 0; sample < n_samples; sample++) {
		samples_all[(*samples_all_index) + sample] = samples_stream[sample];
	}
	*samples_all_index += n_samples;
}





/**
 * server_compute_stats
 * computes and prints statistics for time frame selected (default 60 secs)
 */
void server_compute_stats (sample_data* samples_all, int* samples_all_index, server_stats* stats) {

	server_stats acc_cla = { -1, -1, -1 };
	server_stats acc_red = acc_cla;
	server_stats acc_grn = acc_cla;
	server_stats acc_blu = acc_cla;

	int sample;
	for (sample = 0; sample < *samples_all_index; sample++) {
		acc_cla.mean += samples_all[sample].clarity;
		acc_red.mean += samples_all[sample].red;
		acc_grn.mean += samples_all[sample].green;
		acc_blu.mean += samples_all[sample].blue;

		if (samples_all[sample].clarity > acc_cla.maximum)
			acc_cla.maximum = samples_all[sample].clarity;
		if ((samples_all[sample].clarity < acc_cla.minimum) || (acc_cla.minimum < 0))
			acc_cla.minimum = samples_all[sample].clarity;

		if (samples_all[sample].red > acc_red.maximum)
			acc_red.maximum = samples_all[sample].red;
		if ((samples_all[sample].red < acc_red.minimum) || (acc_red.minimum < 0))
			acc_red.minimum = samples_all[sample].red;

		if (samples_all[sample].green > acc_grn.maximum)
			acc_grn.maximum = samples_all[sample].green;
		if ((samples_all[sample].green < acc_grn.minimum) || (acc_grn.minimum < 0))
			acc_grn.minimum = samples_all[sample].green;

		if (samples_all[sample].blue > acc_blu.maximum)
			acc_blu.maximum = samples_all[sample].blue;
		if ((samples_all[sample].blue < acc_blu.minimum) || (acc_blu.minimum < 0))
			acc_blu.minimum = samples_all[sample].blue;
	}

	acc_cla.mean = (acc_cla.mean + 1) / *samples_all_index;
	acc_red.mean = (acc_red.mean + 1) / *samples_all_index;
	acc_grn.mean = (acc_grn.mean + 1) / *samples_all_index;
	acc_blu.mean = (acc_blu.mean + 1) / *samples_all_index;

	/* Print values*/
	printf("\nIOT_SERVER: == Statistics Calculation ==\n");
	printf("IOT_SERVER: >> Clarity values 	- minimum: %.2f - mean: %.2f - maximum: %.2f\n", acc_cla.minimum, acc_cla.mean, acc_cla.maximum);
	printf("IOT_SERVER: >> Red values 	- minimum: %.2f - mean: %.2f - maximum: %.2f\n", acc_red.minimum, acc_red.mean, acc_red.maximum);
	printf("IOT_SERVER: >> Green values 	- minimum: %.2f - mean: %.2f - maximum: %.2f\n", acc_grn.minimum, acc_grn.mean, acc_grn.maximum);
	printf("IOT_SERVER: >> Blue values 	- minimum: %.2f - mean: %.2f - maximum: %.2f\n", acc_blu.minimum, acc_blu.mean, acc_blu.maximum);
	printf("\n");


	memset(samples_all, 0, MAX_RATE_SERVER_STATS_CALC);
	*samples_all_index = 0;
}





/**
 * print_error_server
 * Show error messages
 */
void print_error_server(int error_code) {

	printf("ERROR: Code %d\n", error_code);
	switch(error_code) {
		case 1:
			printf(">> Could not open file descriptor for socket.\n\n");
			break;
		case 5:
			printf(">> Could not set timeout for socket file descriptor.\n\n");
			break;
		case 2:
			printf(">> Could not bind address to socket.\n\n");
			break;
		case 3:
			printf(">> Could not...\n\n");
			break;
		case 4:
			printf(">> Incorrect arguments provided (all in seconds):\n 1.- Sampling rate for sensor data\n 2.- Transmission streaming rate to server\n 3.- Statistics calculation rate (Optional)\n");
			printf(" Must satisfy (sampling rate <= streaming rate)\n (sampling/streaming) ratio must not exceed maximum sampling ratio: %d\n\n", MAX_SAMPLING_RATIO);
			printf(" Statistics calculation rate must satisfy (>= 2*streaming rate) and not exceed %d\n\n", MAX_RATE_SERVER_STATS_CALC);
			break;
	}

}
