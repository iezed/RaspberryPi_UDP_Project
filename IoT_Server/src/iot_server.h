
#ifndef IOT_SERVER_H_
#define IOT_SERVER_H_


#include <netinet/in.h>		// For socket and bind()'s sockaddr_in struct
#include <stdint.h>			// For register types (e.g. uint8_t)

#include "iot_lib.h"



/* TYPE DEFINITIONS */

typedef struct {
	long int timestamp;
	float clarity;
	float red;
	float green;
	float blue;
} sample_data;


typedef struct {
    float minimum;
    float mean;
    float maximum;
} server_stats;



/* FUNCTION DECLARATIONS */

// IoT Server Module
void		parse_param_rates			(timing_rates* rates, int argc, char* argv[]);
int			server_socket_init			(struct sockaddr_in* server_addr, struct timeval *intervals);
void		server_socket_print_info	(struct sockaddr_in* sockaddr);
int			server_socket_listen		(int server_socket, struct sockaddr_in *client_addr, int comm_established_flag, int server_stats_timeout, int *stats_secs, uint8_t* buffer_recv);
void 		server_socket_reply			(int server_socket, struct sockaddr_in *client_addr, uint8_t* buffer_recv, timing_rates* timings);
void 		server_build_reply			(int server_socket, uint8_t* buffer_recv, uint8_t* buffer_reply, timing_rates* timings);
int 		server_datagram_parsing		(uint8_t* data_in, sample_data* data_out);
void		server_save_samples			(sample_data* samples_stream, int n_samples, sample_data* samples_all, int* samples_all_index);
void		server_compute_stats		(sample_data* samples_all, int* samples_all_index, server_stats* stats);


// Error Control
void 		print_error_server	(int error_code);
// static void	sig_handler			(int _);



#endif /* IOT_CLIENT_H_ */
