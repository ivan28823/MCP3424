/*
*  MCP3424 Driver for cpp
*  Author : Ivan Moreno
*
*  Based in ABE_ADCDifferentialPi from ABE Electronics
*
*  Required package{
*    apt-get install libi2c-dev}
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdexcept>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include "MCP3424.h"

#define fileName "/dev/i2c-1" // change to /dev/i2c-0 if you are using a revision 0002 or 0003 model B

// Constructors
MCP3424::MCP3424(){
	i2cbus = 0;           // enumerate i2c bus
	i2caddress = 0;      // addres of i2c
	config = 0;          // config byte
	currentchannel = 1;  // actual channel
	bitrate = 18;         // bitrate of device
	conversionmode = 1;  // mode of conversion mode
	pga = 0.5;           // gain of device
	lsb = 0.000007812;           // least significative bit
	char signbit = 0;         // sign bit
}
MCP3424::MCP3424(char address, char rate){
	signbit = 0;
	i2caddress = address;
	config = 0x9C;		 	// PGAx1, 18 bit, continuous conversion, channel 1
	currentchannel = 1; // channel variable for adc 1
	bitrate = 18;		 		// current bitrate
	conversionmode = 1; // Conversion Mode
	pga = 0.5;			 		// current pga setting
	lsb = 0.000007812;  // default lsb value for 18 bit

	set_bit_rate(rate);
}

//Private Methods
void MCP3424::write_byte(char address, char value)
{
	/**
	* private method for writing a byte to the I2C port
	*/
	if ((i2cbus = open(fileName, O_RDWR)) < 0){	// open the i2c bus
		printf("Failed to open i2c port for read %s \n", strerror(errno));
		exit(1);
	}
	if (ioctl(i2cbus, I2C_SLAVE, address) < 0){
		throw std::runtime_error("Failed to write to i2c port for write");
	}
	writebuffer[0] = value;
	if ((write(i2cbus, writebuffer, 1)) != 1)
	{
		throw std::runtime_error("Failed to write to i2c device for write");
	}
	close(i2cbus);
}
void MCP3424::read_byte_array(char address, char reg, char length){
	/**
	* private method for reading bytes from the I2C port
	*/
	if (ioctl(i2cbus, I2C_SLAVE, address) < 0){
		throw std::runtime_error("Failed to write to i2c port for read");
	}
	writebuffer[0] = reg;
	if ((write(i2cbus, writebuffer, 1)) != 1){
		throw std::runtime_error("Failed to write to i2c device for read");
	}
	read(i2cbus, readbuffer, 4);
}
char MCP3424::update_byte(char byte, char bit, char value){
	/**
	* private method for setting the value of a single bit within a byte
	*/
	if (value == 0){
		return (byte &= ~(1 << bit));
	}
	else if (value == 1){
		return (byte |= 1 << bit);
	}
	else{
		throw std::out_of_range("update_byte() value out of range: 0 or 1");
	}
}
void MCP3424::set_channel(char channel){
	/**
	* private method for setting the channel
	*/
	if (channel >= 1 && channel <= 4){
		if (channel != currentchannel){
			switch (channel){
			case 1:
				config = update_byte(config, 5, 0);
				config = update_byte(config, 6, 0);
				currentchannel = 1;
				break;
			case 2:
				config = update_byte(config, 5, 1);
				config = update_byte(config, 6, 0);
				currentchannel = 2;
				break;
			case 3:
				config = update_byte(config, 5, 0);
				config = update_byte(config, 6, 1);
				currentchannel = 3;
				break;
			case 4:
				config = update_byte(config, 5, 1);
				config = update_byte(config, 6, 1);
				currentchannel = 4;
				break;
			}
		}
	}
	else{
		throw std::out_of_range("set_channel() channel out of range: 1 to 4");
	}
}

// Public Methods
void MCP3424::set_config_values(char address,char channel,char rate,char pg,char conv_mode){
	/**
	*	Function that set the parameters outside of the constructor
	*/
	i2caddress = address;
	set_pga(pg);
	set_channel(channel);
	set_bit_rate(rate);
	set_conversion_mode(conv_mode);
}
int MCP3424::read_raw(char channel){
	/**
	* Reads the raw value from the selected ADC channel
	* @param channel - 1 to 4
	* @returns - raw long value from ADC buffer
	*/
	// variables for storing the raw bytes from the ADC
	char h = 0;
	char l = 0;
	char m = 0;
	char s = 0;
	char config = 0;
	char address = 0;
	long t = 0;
	signbit = 0;

	// get the config and i2c address for the selected channel
	set_channel(channel);

	if (channel < 5){
		config = this->config;
		address = i2caddress;
	}
	// if the conversion mode is set to one-shot update the ready bit to 1
	if (conversionmode == 0){
		config = update_byte(config, 7, 1);
		write_byte(address, config);
		config = update_byte(config, 7, 0);
	}
	// keep reading the ADC data until the conversion result is ready
	int timeout = 1000; // number of reads before a timeout occurs
	int x = 0;
	// open the i2c bus
	if ((i2cbus = open(fileName, O_RDWR)) < 0){
		printf("Failed to open i2c port for read %s \n", strerror(errno));
		exit(1);
	}
	do{
		if (bitrate == 18){
			read_byte_array(address, config, 3);
			h = readbuffer[0];
			m = readbuffer[1];
			l = readbuffer[2];
			s = readbuffer[3];
		}
		else{
			read_byte_array(address, config, 2);
			h = readbuffer[0];
			m = readbuffer[1];
			s = readbuffer[2];
		}
		// check bit 7 of s to see if the conversion result is ready
		if (!(s & (1 << 7))){
			break;
		}
		if (x > timeout){
			// timeout occurred
			return (0);
		}
		x++;
	} while (1);
	// close the i2c bus
	close(i2cbus);
	// extract the returned bytes and combine in the correct order
	switch (bitrate){
	case 18:
		t = ((h & 3) << 16) | (m << 8) | l;
		if ((t >> 17) & 1){
			signbit = 1;
			t &= ~(1 << 17);
		}
		break;
	case 16:
		t = (h << 8) | m;
		if ((t >> 15) & 1){
			signbit = 1;
			t &= ~(1 << 15);
		}
		break;
	case 14:
		t = ((h & 63) << 8) | m;
		if ((t >> 13) & 1){
			signbit = 1;
			t &= ~(1 << 13);
		}
		break;
	case 12:
		t = ((h & 15) << 8) | m;
		if ((t >> 11) & 1){
			signbit = 1;
			t &= ~(1 << 11);
		}
		break;
	default:
		throw std::runtime_error("read_raw() bitrate out of range");
		break;
	}
	return (t);
}
double MCP3424::read_voltage(char channel){
	/**
	* Returns the voltage from the selected ADC channel
	* @param channel - 1 to 8
	* @returns - double voltage value from ADC
	*/
	int raw = read_raw(channel); // get the raw value
	if (signbit == 1){ // if the signbit is 1 the value is negative and most likely noise so it can be ignored.
		double voltage = ((double)raw * (lsb / pga)) - (2.048 / (pga * 2)); // calculate the voltage and return it
		return (voltage);
	}
	else
	{
		double voltage = ((double)raw * (lsb / pga)); // calculate the voltage and return it
		return (voltage);
	}
}
void MCP3424::set_pga(char gain){
	/**
	* Programmable Gain Amplifier gain selection
	* @param gain - Set to 1, 2, 4 or 8
	*/
	switch (gain)
	{
	case 1:
		config = update_byte(config, 0, 0);
		config = update_byte(config, 1, 0);
		pga = 0.5;
		break;
	case 2:
		config = update_byte(config, 0, 1);
		config = update_byte(config, 1, 0);
		pga = 1;
		break;
	case 4:
		config = update_byte(config, 0, 0);
		config = update_byte(config, 1, 1);
		pga = 2;
		break;
	case 8:
		config = update_byte(config, 0, 1);
		config = update_byte(config, 1, 1);
		pga = 4;
		break;
	default:
		throw std::out_of_range("set_pga() gain out of range: 1, 2, 4, 8");
	}
	write_byte(i2caddress, config);
}
void MCP3424::set_bit_rate(char rate){
	/**
	* Set the sample resolution
	* @param rate - 12 = 12 bit(240SPS max), 14 = 14 bit(60SPS max), 16 = 16 bit(15SPS max), 18 = 18 bit(3.75SPS max)
	*/
	switch (rate){
  	case 12:
  		config = update_byte(config, 2, 0);
  		config = update_byte(config, 3, 0);
  		bitrate = 12;
  		lsb = 0.0005;
  		break;
  	case 14:
  		config = update_byte(config, 2, 1);
  		config = update_byte(config, 3, 0);
  		bitrate = 14;
  		lsb = 0.000125;
  		break;
  	case 16:
  		config = update_byte(config, 2, 0);
  		config = update_byte(config, 3, 1);
  		bitrate = 16;
  		lsb = 0.00003125;
  		break;
  	case 18:
  		config = update_byte(config, 2, 1);
  		config = update_byte(config, 3, 1);
  		bitrate = 18;
  		lsb = 0.0000078125;
  		break;
  	default:
  		throw std::out_of_range("set_bit_rate() rate out of range: 12, 14, 16, 18");
	}
  write_byte(i2caddress, config);
}
void MCP3424::set_conversion_mode(char mode){
	/**
	* Set the conversion mode for ADC
	* @param mode - 0 = One shot conversion mode, 1 = Continuous conversion mode
	*/
	if (mode == 0){
		config = update_byte(config, 4, 0);
		conversionmode = 0;
	}
	else if (mode == 1){
		config = update_byte(config, 4, 1);
		conversionmode = 1;
	}
	else{
		throw std::out_of_range("set_conversion_mode() mode out of range: 0 or 1");
	}
}
