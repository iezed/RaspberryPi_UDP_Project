/*
 ============================================================================
 Name        : color_sensor.c
 Author      : Nicolas Villanueva
 Version     : 1.0.0 (March 2020)
 Description : Small program to detect colors with the TCS34725 RGB sensor
 ============================================================================
 */

#include <stdio.h>			// For printf() and basic C utilities
#include <stdlib.h>			// For exit code
#include <stdint.h>			// For register types (e.g. uint8_t)
#include <unistd.h>			// for usleep() and sleep()
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "color_sensor.h"
#include "color_sensor_interface.h"



float integration_time_secs;	// Integration Time = 2.4 ms × (256 − ATIME)
float waiting_time_secs;		// Waiting Time 	= 2.4 ms × (256 − WTIME)



int tcs34725_setup(tcs34725_setup_params* setup) {
	int fd_i2c;
	uint8_t ena_reg_byte;

	/* Step 1: Create instance of I2C interface's descriptor */

	fd_i2c = get_i2c_descriptor();



	/* Step 2: Write to ATIME Register (0x01) to set Integration Time (700ms - 0x00) */

	printf("COLOR_SENSOR: Writing ATIME register\n");
	write_config_byte(fd_i2c, TCS_REG_ATIME, setup->a_time);

	integration_time_secs = (2.4 * (256 - (int) setup->a_time));
	printf("COLOR_SENSOR: Integration time: %f ms\n", integration_time_secs);



	/* Step 3: Write to WTIME Register (0x03) to set Waiting Time (2.4ms - 0xFF) */

	if (setup->w_enable == true) {
		printf("COLOR_SENSOR: Writing WTIME register\n");
		write_config_byte(fd_i2c, TCS_REG_WTIME, setup->w_time);

		waiting_time_secs = (2.4 * (256 - (int) setup->w_time));
		printf("COLOR_SENSOR: Waiting time: %f ms\n", waiting_time_secs);
	} else {
		waiting_time_secs = 0;
		printf("COLOR_SENSOR: No waiting state configuration.\n");
	}



	/* Step 4: Write to AGAIN bits of Control Register (0x0F) to set AGAIN Value (x1 Gain RGBC Control value: 0x00) */

	printf("COLOR_SENSOR: Writing Control register\n");
	write_config_byte(fd_i2c, TCS_REG_CONTROL, setup->ctrl_reg);



	/* Step 5: Write to Enable Register (0x00) to set PON bit: sets sensor to "idle" state */

	printf("COLOR_SENSOR: Writing Enable register: PON bits \n");
	write_config_byte(fd_i2c, TCS_REG_ENABLE, 0x01);

	printf("COLOR_SENSOR: Wait for 2.4 ms\n");		// There is a 2.4 ms warm-up delay when PON is enabled.
	usleep(2400);



	/* Step 6: Write to Enable Register (0x00) to set AEN bit (without WEN bit) to start sensor cycles */

	ena_reg_byte = (setup->w_enable == true) ? 0x0C : 0x03;
	printf("COLOR_SENSOR: Writing Enable register\n");
	write_config_byte(fd_i2c, TCS_REG_ENABLE, ena_reg_byte);		// NOTE: to set PON, AEN and WEN bits: 0, 1 and 3 respectively (0x0C)


	return fd_i2c;
}





void tcs34725_read(int fd_i2c, uint8_t* data) {
	uint8_t read_ptr_reg;
	uint8_t data_byte;


	// printf("\nCOLOR_SENSOR: Integration and Waiting time hold-up (%.1f ms)\n", integration_time_secs + waiting_time_secs);
	// usleep((integration_time_secs + waiting_time_secs) * 1000);		// We are to wait the amount of time defined as integration time + waiting time


	// printf("COLOR_SENSOR: TCS - read sequence begun.\n");
	read_ptr_reg = TCS_CMD_BYTE | TCS_REG_DATA_C_LOW; // TCS_CMD_AUTOINC | TCS_REG_DATA_C_LOW;
	int i;
	for (i = 0; i < TCS34725_SAMPLE_SIZE; i++) {
		// Write request to perform read operation
		i2c_write(fd_i2c, &read_ptr_reg, 1);

		// Read from queried register
		data_byte = i2c_read(fd_i2c);
		if (data_byte != -1) {
			data[i] = data_byte;
		}

		// Increase read pointer register
		read_ptr_reg++;
	}
	// printf("COLOR_SENSOR: TCS - read sequence done:\n");

}




void tcs34725_print(uint8_t* data_in) {
	uint16_t conversions_16[4];
	float conversions[4];


	// Merge 8-bit register sensor data into 16-bit structures
	conversions_16[0] = (uint16_t) (data_in[1] << 8 | data_in[0]);
	conversions_16[1] = (uint16_t) (data_in[3] << 8 | data_in[2]);
	conversions_16[2] = (uint16_t) (data_in[5] << 8 | data_in[4]);
	conversions_16[3] = (uint16_t) (data_in[7] << 8 | data_in[6]);

	// Convert into percentage-based floating-point numbers.
	conversions[0] = (float) conversions_16[0] / 655.35;
	conversions[1] = (float) conversions_16[1] / 655.35;
	conversions[2] = (float) conversions_16[2] / 655.35;
	conversions[3] = (float) conversions_16[3] / 655.35;

	printf("COLOR_SENSOR: Clarity: %.2f %% - Red: %.2f %% - Green: %.2f %% - Blue: %.2f %%\n", conversions[0], conversions[1], conversions[2], conversions[3]);

	// Check for color predominance
	/*
	if ((conversions[1] > conversions[2]*1.5) && (conversions[1] > conversions[3]*1.5)) {
		printf("COLOR_SENSOR: Predominant color: RED \n");
	} else if ((conversions[2] > conversions[1]*1.5) && (conversions[2] > conversions[3]*1.5)) {
		printf("COLOR_SENSOR: Predominant color: GREEN \n");
	} else if ((conversions[3] > conversions[2]*1.5) && (conversions[3] > conversions[1]*1.5)) {
		printf("COLOR_SENSOR: Predominant color: BLUE \n");
	} else {
		printf("COLOR_SENSOR: No clear predominant color.\n");
	}
	*/

}


