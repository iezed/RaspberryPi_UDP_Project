
#ifndef IOT_SERVER_H_
#define IOT_SERVER_H_


#include <netinet/in.h>		// For socket and bind()'s sockaddr_in struct
#include <stdint.h>			// For register types (e.g. uint8_t)

#include "iot_lib.h"


/* MACROS & CONSTANTS */

#define TCS34725_SAMPLE_SIZE	8





/* TYPE DEFINITIONS */

typedef struct {
	int timestamp;
	float clarity;
	float red;
	float green;
	float blue;
} sample_data;


typedef struct {
    int minimum;
    int mean;
    int maximum;
} sensor_statistics;



/* FUNCTION DECLARATIONS */

// IoT Server Module
void		parse_param_rates			(timing_rates* rates, int argc, char* argv[]);
int			server_socket_init			(struct sockaddr_in* server_addr);
void		server_socket_print_info	(struct sockaddr_in* sockaddr);
void		server_socket_listen		(int server_socket, struct sockaddr_in* client_addr, uint8_t* buffer_recv, timing_rates* timings);
void 		server_build_reply			(int server_socket, struct sockaddr_in* client_addr, uint8_t* buffer_recv, uint8_t* buffer_reply, timing_rates* timings);
void 		server_datagram_parsing		(uint8_t* data_in); // float data_out[][4]


// Error Control
void 		print_error_server	(int error_code);
// static void	sig_handler			(int _);



#endif /* IOT_CLIENT_H_ */
