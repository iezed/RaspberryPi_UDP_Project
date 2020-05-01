/*
 * iot_shared.h
 *
 *  Created on: May 1, 2020
 *      Author: Nicolas Villanueva
 */

#ifndef IOT_LIB_H_
#define IOT_LIB_H_



/* MACROS AND CONSTANTS */

// Network parameters
#define SERVER_PORT 				8080
#define SERVER_PORT_STR 			"8080"
#define SERVER_ADDR 				"192.168.1.67"
#define SERVER_ADDR_32T				0xC0A80143 // 192.168.1.67

// Communication parameters
#define DATAGRAM_SIZE				1024
#define DATAGRAM_HEADER_SIZE		3	// Request Type (1B) + Message Size (2B)
#define DATAGRAM_SAMPLE_SIZE		10	// 2 timestamp bytes + 8 data bytes
#define MAX_SAMPLING_RATIO			DATAGRAM_SIZE / DATAGRAM_SAMPLE_SIZE
#define DEFAULT_RATE_SAMPLING		1
#define DEFAULT_RATE_SERVER_STREAM	10

// Protocol data codes
#define DATAGRAM_REQ_COMM				0x01
#define DATAGRAM_REP_COMM_OK			0x02
#define DATAGRAM_REQ_SEND_DATA			0x03
#define DATAGRAM_REP_SEND_DATA_OK		0x04
#define DATAGRAM_REP_ERROR				0x0F



/* TYPE DEFINITIONS */

typedef struct {
    int sampling;
    int server_stream;
} timing_rates;



#endif /* IOT_LIB_H_ */
