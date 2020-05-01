/*
 * color_sensor_interface.h
 *
 *  Created on: Apr 2020
 *      Author: Nicolas Villanueva
 */


#ifndef COLOR_SENSOR_INTERFACE_H_
#define COLOR_SENSOR_INTERFACE_H_


#include <stdint.h>			// For register types (e.g. uint8_t)


/* MACROS AND CONSTANTS */

#define I2C_INTERFACE			"/dev/i2c-1"
#define TCS34725_ADDR			0x29

#define TCS_CMD_BYTE			0x80
#define TCS_CMD_AUTOINC			0xa0
#define TCS_REG_ENABLE			0x00
#define TCS_REG_ATIME			0x01
#define TCS_REG_WTIME			0x03
#define TCS_REG_CONTROL			0x0F
#define TCS_REG_DATA_C_LOW		0x14



/* FUNCTION DECLARATION */

int		get_i2c_descriptor	();
void 	write_config_byte	(int fd_i2c, uint8_t reg, uint8_t byte);
void 	i2c_write			(int fd_i2c, uint8_t* registers, int length);
int 	i2c_read			(int fd_i2c);



#endif /* COLOR_SENSOR_INTERFACE_H_ */
