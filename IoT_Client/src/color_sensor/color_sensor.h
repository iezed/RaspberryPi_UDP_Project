/*
 ============================================================================
 Name        : color_sensor.h
 Author      : Nicolas Villanueva
 Version     : 1.0.0 (March 2020)
 Description : Small program to detect colors with the TCS34725 RGB sensor
 ============================================================================
 */

#include <stdbool.h>
#include <stdint.h>			// For register types (e.g. uint8_t)


/* CONSTANTS AND MACROS */

#define TCS34725_SAMPLE_SIZE	8


/* TYPE DEFINITIONS */

typedef struct {
	uint8_t a_time;
	uint8_t w_time;
	uint8_t ctrl_reg;
	bool	w_enable;
} tcs34725_setup_params;



/* FUNCTION DECLARATIONS */

int		tcs34725_setup				(tcs34725_setup_params* setup); // returns file descriptor to I2C interface
void 	tcs34725_read				(int fd_i2c, uint8_t* data);
void 	tcs34725_print				(uint8_t* data_in);

