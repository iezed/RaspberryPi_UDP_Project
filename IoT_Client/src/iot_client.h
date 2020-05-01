
#ifndef IOT_CLIENT_H_
#define IOT_CLIENT_H_


/* Libraries */

#include "iot_lib.h"
#include "color_sensor/color_sensor.h"



/* FUNCTION DECLARATION */

int 		client_socket_init			(struct sockaddr_in* server_addr);
void 		client_socket_print_info	(struct sockaddr_in* sockaddr);
void 		client_send_data			(int client_socket, struct sockaddr_in* server_addr, uint8_t* buffer_send, uint8_t* buffer_recv);
void		client_parse_timing_params	(timing_rates* timings, uint8_t* buffer_recv);
void		client_push_server_buffer	(int timestamp, int server_buffer_index, uint8_t* sensor_data, uint8_t server_buffer[MAX_SAMPLING_RATIO][DATAGRAM_SAMPLE_SIZE]);
void 		client_tcs34725_build_data	(uint8_t request_type, int n_samples, uint8_t server_buffer[MAX_SAMPLING_RATIO][DATAGRAM_SAMPLE_SIZE], uint8_t* buffer_send);
void		print_error_client			(int error_code);



#endif /* IOT_CLIENT_H_ */
