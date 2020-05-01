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

#include "iot_lib.h"
#include "iot_server.h"



sensor_statistics stats = { -1, -1, -1 };

int main(int argc, char* argv[]) {

	/* STEP 1 - Parse command line parameters: sampling rate and server streaming rate */
	timing_rates timings;
	parse_param_rates(&timings, argc, argv);


	/* STEP 2 - Initialize UDP communication socket */
	struct sockaddr_in server_addr;
	int server_socket = server_socket_init(&server_addr);


	while(1) {
		/* STEP 3 - Process incoming datagrams from client */
		struct sockaddr_in client_addr;
		uint8_t buffer_recv[DATAGRAM_SIZE] = {'\0'};
		server_socket_listen(server_socket, &client_addr, buffer_recv, &timings);


		/* STEP 4 - Parse data from received datagram */
		// float sensor_data_converted[n_samples][4];
		server_datagram_parsing(buffer_recv);
	}


	close(server_socket);
	return EXIT_SUCCESS;
}





/* parse_param_rates
 * return 0 for successful parsing, 1 for incorrect format
 */
void parse_param_rates (timing_rates* timings, int argc, char* argv[]) {

	int sampling, server_data;
	switch(argc) {
		// No parameters provided: default values
		case 1:
			timings->sampling = DEFAULT_RATE_SAMPLING;
			timings->server_stream = DEFAULT_RATE_SERVER_STREAM;
			break;

		// Parameters provided: parse and save for subsequent datagram to client
		case 3:
			sampling = atoi(argv[1]);
			server_data = atoi(argv[2]);
			if (sampling && server_data && (sampling <= server_data) && ((server_data / sampling) <= MAX_SAMPLING_RATIO)) {
				timings->sampling = sampling;
				timings->server_stream = server_data;
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
int server_socket_init(struct sockaddr_in* server_addr) {

	/* Create UDP/IP socket */
	int server_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (server_socket < 0) {
		print_error_server(1);
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
 */
void server_socket_listen(int server_socket, struct sockaddr_in *client_addr, uint8_t* buffer_recv, timing_rates* timings) {

	/* Clear reception buffer and client address structure */
	memset(buffer_recv, 0, DATAGRAM_SIZE);
	memset(client_addr, 0, sizeof(*client_addr));
	socklen_t client_addr_len = sizeof(*client_addr);

	/* Waits for datagram */
	printf("IOT_SERVER: Waiting to receive datagram...\n");
	ssize_t recv_len = recvfrom(server_socket, buffer_recv, DATAGRAM_SIZE, 0, (struct sockaddr *) client_addr, &client_addr_len);

	// Parse message and print buffer information
	printf("IOT_SERVER: Received %d-byte datagram from %s:%d\n", (int) recv_len, inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
	// printf("received data: %u %u %u %u %u %u %u %u %u %u\n", buffer_recv[0], buffer_recv[1], buffer_recv[2], buffer_recv[3], buffer_recv[4], buffer_recv[5], buffer_recv[6], buffer_recv[7], buffer_recv[8], buffer_recv[9]);
	int i;
	for (i = 0; i < recv_len; i++)
		printf("	>> %u", buffer_recv[i]);
	printf("\n");

	/* Build and send UDP reply to client */
	uint8_t buffer_reply[DATAGRAM_SIZE] = {"\0"};
	server_build_reply(server_socket, client_addr, buffer_recv, buffer_reply, timings);


	ssize_t send_len = sendto(server_socket, buffer_reply, (((int) (buffer_reply[2] << 8) | (buffer_reply[1])) + DATAGRAM_HEADER_SIZE + 1), 0, (struct sockaddr *) client_addr, client_addr_len);
	printf("Sent %d-byte response\n", (int) send_len);
}





/**
 * server_build_reply
 * Build reply to client according to request type received
 */
void server_build_reply(int server_socket, struct sockaddr_in* client_addr, uint8_t* buffer_recv, uint8_t* buffer_reply, timing_rates* timings) {

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
 */
// float data_out[][4]
void server_datagram_parsing(uint8_t* buffer_recv) {

	int n_samples = (int) ((buffer_recv[2] << 8) | (buffer_recv[1])) / 8;

	uint8_t		samples_raw[n_samples][DATAGRAM_SAMPLE_SIZE];
	sample_data samples_parsed[n_samples];

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
		samples_parsed[sample].timestamp = (int) conversions_16[0];
		samples_parsed[sample].clarity = (float) conversions_16[1] / 655.35;
		samples_parsed[sample].red = (float) conversions_16[2] / 655.35;
		samples_parsed[sample].green = (float) conversions_16[3] / 655.35;
		samples_parsed[sample].blue = (float) conversions_16[4] / 655.35;

		printf("IOT_SERVER: Sample %d from sensor at %d seconds: Clarity %.2f %% - Red: %.2f %% - Green: %.2f %% - Blue: %.2f %% \n",
				sample, samples_parsed[sample].timestamp, samples_parsed[sample].clarity, samples_parsed[sample].red, samples_parsed[sample].green, samples_parsed[sample].blue);
	}
	printf("\n");

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
		case 2:
			printf(">> Could not bind address to socket.\n\n");
			break;
		case 3:
			printf(">> Could not...\n\n");
			break;
		case 4:
			printf(">> Incorrect arguments provided:\n 1.- Sampling rate for sensor data\n 2.- Transmission streaming rate to server\n");
			printf(" (Must satisfy sampling_rate <= transmission_rate)\n (sampling/streaming) ratio must not exceed maximum sampling ratio: %d\n\n", MAX_SAMPLING_RATIO);
			break;
	}

}
