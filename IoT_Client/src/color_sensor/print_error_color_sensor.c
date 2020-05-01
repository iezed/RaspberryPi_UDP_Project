/*
 * print_error_color_sensor.c
 *
 *  Created on: Apr 2020
 *      Author: Nicolas Villanueva
 */

#include <stdio.h>

#include "print_error_color_sensor.h"



void print_error_color_sensor(int error_code) {

	printf("ERROR COLOR_SENSOR: Code %d\n", error_code);
	switch(error_code) {
		case 1:
			printf(">> Could not open file descriptor");
			break;
		case 2:
			printf(">> Could not find the I2C slave");
			break;
		case 3:
			printf(">> Data write to I2C slave failed");
			break;
		case 4:
			printf(">> Bytes sent do not match length expected.\n");
			break;
		case 5:
			printf(">> Input/Output error at data read");
			break;
		case 6:
			printf(">> Data headers are incorrect (Function code & Number of bytes queried)");
			break;
	}
}
