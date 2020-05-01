/*
 ============================================================================
 Name        : iot_client.c
 Author      : Nicolas Villanueva
 Version     : 1.0.0 (April 2020)
 Description : IoT client program to run in Raspberry Pi.
 ============================================================================
 */

#include <stdio.h>			// For printf() and basic C utilities
#include <stdlib.h>			// For exit code
#include <stdint.h>			// For register types (e.g. uint8_t)
#include <string.h>			// For memset()
#include <sys/socket.h>		// For socket()
#include <netinet/in.h> 	// For socket() and sockaddr_in struct */
#include <netinet/udp.h>	// For socket()
#include <unistd.h>			// For close()
#include <arpa/inet.h>		// For inet_aton()

#include "iot_lib.h"
#include "iot_client.h"




int main(void) {

	/* STEP 1 - Initialize both sensor and communication socket */

	struct sockaddr_in server_addr;
	tcs34725_setup_params sensor_setup = {0x00, 0xFF, 0x01, false};

	int fd_sensor = tcs34725_setup(&sensor_setup);
	int client_socket = client_socket_init(&server_addr);


	/* STEP 2 - Send communication request to server to ensure communication and parse timing parameters */

	timing_rates timings;
	uint8_t buffer_send[DATAGRAM_SIZE] = {'\0'};
	uint8_t buffer_recv[DATAGRAM_SIZE] = {'\0'};

	client_tcs34725_build_data(DATAGRAM_REQ_COMM, 0, NULL, buffer_send);
	client_send_data(client_socket, &server_addr, buffer_send, buffer_recv);
	client_parse_timing_params(&timings, buffer_recv);

	// Set server stream buffer to twice the sampling ratio in case of sending datagram failure
	// int sampling_ratio = timings.server_stream / timings.sampling + 1;
	uint8_t server_buffer[MAX_SAMPLING_RATIO][DATAGRAM_SAMPLE_SIZE] = {{'\0'}};


	long int seconds = 0; int server_buffer_index = 0;
	while(1) {
		uint8_t sensor_data[TCS34725_SAMPLE_SIZE];

		/* STEP 3 - Read data from sensor */

		if (seconds % timings.sampling == 0) {
			printf("\nIOT_CLIENT: Sampling sensor at %ld seconds\n", seconds);
			tcs34725_read(fd_sensor, sensor_data);
			tcs34725_print(sensor_data);

			// Push sensor reading into server buffer for subsequent streaming
			client_push_server_buffer(seconds, server_buffer_index, sensor_data, server_buffer);
			server_buffer_index++;
		}


		/* STEP 4 - Build message from sensor data and send to server */

		if ((seconds > 0) && (seconds % timings.server_stream == 0)) {
			printf("\nIOT_CLIENT: Sending data to server at %ld seconds\n", seconds);
			client_tcs34725_build_data(DATAGRAM_REQ_SEND_DATA, server_buffer_index, server_buffer, buffer_send);
			client_send_data(client_socket, &server_addr, buffer_send, buffer_recv);

			memset(buffer_send, 0, DATAGRAM_SIZE); // sizeof(*message) ??
			memset(server_buffer, 0, (DATAGRAM_SAMPLE_SIZE * MAX_SAMPLING_RATIO));
			server_buffer_index = 0;
		}

		sleep(1);
		seconds++;
	}

	close(client_socket);

	return EXIT_SUCCESS;

}





/**
 * client_socket_init
 * returns socket descriptor to communicate with server
 */
int client_socket_init(struct sockaddr_in* server_addr) {

	/* Create UDP/IP socket */

	int client_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (client_socket < 0) {
		print_error_client(1);
		exit(EXIT_FAILURE);
	}
	printf("IOT_CLIENT: Initialized client UDP socket (descriptor %d)\n", client_socket);


	/* Configure address structure to send datagrams to server */

	// Fill sockaddr structure
	memset(server_addr, 0, sizeof(*server_addr));
	server_addr->sin_family = AF_INET;
	server_addr->sin_port =  htons(SERVER_PORT);
	inet_aton(SERVER_ADDR, &server_addr->sin_addr); // server_addr.sin_addr.s_addr = htonl(SERVER_ADDR_32T);

	client_socket_print_info(server_addr);
	return client_socket;
}





/**
 * client_socket_print_info
 * shows own socket address and port values in console
 */
void client_socket_print_info(struct sockaddr_in* sockaddr) {

	/* Print socket address' IP and UDP port */

	short ip_1, ip_2, ip_3, ip_4;
	unsigned long int ip_unparsed;
	int port;

	port = (int) ntohs(sockaddr->sin_port);
	ip_unparsed = (unsigned long int) ntohl(sockaddr->sin_addr.s_addr);

	ip_4 = ip_unparsed % 256;
	ip_unparsed /= 256;
	ip_3 = ip_unparsed % 256;
	ip_unparsed /= 256;
	ip_2 = ip_unparsed % 256;
	ip_unparsed /= 256;
	ip_1 = ip_unparsed % 256;

	printf("IOT_CLIENT: Bound address to socket: Address %hd.%hd.%hd.%hd - port %d.\n", ip_1, ip_2, ip_3, ip_4, port);
}





/**
 * client_send_data
 * sends data to server and expects confirmation reply
 */
void client_send_data(int client_socket, struct sockaddr_in* server_addr, uint8_t* buffer_send, uint8_t* buffer_recv) {

	/* Send Packet to Server */
	size_t buffer_send_len = (size_t) ((buffer_send[2] << 8) + buffer_send[1]) + DATAGRAM_HEADER_SIZE + 1;
	ssize_t send_len = sendto(client_socket, buffer_send, buffer_send_len, 0,
							  (const struct sockaddr *) server_addr, sizeof(*server_addr));
	printf("IOT_CLIENT: Sent %d-byte datagram to server\n", (int) send_len);

	int i;
	for (i = 0; i < buffer_send_len; i++)
		printf("	>> %u", buffer_send[i]);
	printf("\n");
	memset(buffer_send, 0, DATAGRAM_SIZE);

	/* Receive Reply */

	struct sockaddr_in server_reply_addr;
	memset(&server_reply_addr, 0, sizeof(server_reply_addr));
	socklen_t server_reply_addr_len = sizeof(server_reply_addr);
	ssize_t recv_len = recvfrom(client_socket, buffer_recv, DATAGRAM_SIZE, 0, (struct sockaddr *) &server_reply_addr, &server_reply_addr_len);
	printf("IOT_CLIENT: Received %d-byte reply from server\n\n", (int) recv_len);

	// printf("IOT_CLIENT: Message sent to			: %lld\n", (unsigned long long int) ntohl(server_reply_addr.sin_addr.s_addr));
	// printf("IOT_CLIENT: Reply received from		: %lld\n", (unsigned long long int) ntohl(server_addr->sin_addr.s_addr));




}





/**
 * client_parse_timing_params
 * Detects sampling and server streaming rates from server's communication "acceptance"
 */
void client_parse_timing_params(timing_rates* timings, uint8_t* buffer_recv) {

	if ((buffer_recv[0] == DATAGRAM_REP_COMM_OK)) {
		int data_length = (int) ((buffer_recv[2] << 8) | (buffer_recv[1]));
		if (data_length == 2) {
			timings->sampling = (int) buffer_recv[3];
			timings->server_stream = (int) buffer_recv[4];
		} else {
			timings->sampling = DEFAULT_RATE_SAMPLING;
			timings->server_stream = DEFAULT_RATE_SERVER_STREAM;
		}
		printf("IOT CLIENT: sampling rate: %d - server streaming rate: %d\n", timings->sampling, timings->server_stream);
	}
}





/**
 *
 */
void client_push_server_buffer(int timestamp, int server_buffer_index, uint8_t* sensor_data, uint8_t server_buffer[MAX_SAMPLING_RATIO][DATAGRAM_SAMPLE_SIZE]) {

	// bytes 0-1: timestamp
	uint16_t seconds_16t = (uint16_t) timestamp;
	server_buffer[server_buffer_index][0] = (uint8_t) seconds_16t;			// LSB
	server_buffer[server_buffer_index][1] = (uint8_t) (seconds_16t >> 8);	// MSB

	// bytes 2-10: sensor data
	int reg_index;
	for (reg_index = 0; reg_index < TCS34725_SAMPLE_SIZE; reg_index++) {
		server_buffer[server_buffer_index][reg_index + 2] = sensor_data[reg_index];
	}
}





/**
 * client_tcs34725_build_data
 * Build data vector from server buffer matrix
 */
void client_tcs34725_build_data(uint8_t request_type, int n_samples, uint8_t server_buffer[MAX_SAMPLING_RATIO][DATAGRAM_SAMPLE_SIZE], uint8_t* buffer_send) {

	// First byte: Request type
	buffer_send[0] = request_type;

	// Second and Third bytes: message size (without headers and End-Of-Package)
	uint16_t message_size = (uint16_t) ((n_samples * DATAGRAM_SAMPLE_SIZE));
	buffer_send[1] = (uint8_t) message_size;		// LSB
	buffer_send[2] = (uint8_t) (message_size >> 8);	// MSB
	printf("IOT_CLIENT: size of message: %d\n", (int) ((buffer_send[2] * 256) + buffer_send[1]));

	// Fourth-to-last bytes: message data
	int sample;
	if (n_samples > 0) {
		for (sample = 0; sample < n_samples; sample++) {
			int byte;
			for (byte = 0; byte < DATAGRAM_SAMPLE_SIZE; byte++) {
				buffer_send[(sample * DATAGRAM_SAMPLE_SIZE) + byte + DATAGRAM_HEADER_SIZE] = server_buffer[sample][byte];
			}
		}
	}

}





void print_error_client(int error_code) {

	printf("ERROR IOT_CLIENT: Code %d\n", error_code);
	switch(error_code) {
		case 1:
			printf(">> Could not open file descriptor for socket.\n");
			break;
		case 2:
			printf(">> Could not bind address to socket.\n");
			break;
		case 3:
			printf(">> \n");
			break;
	}
}


