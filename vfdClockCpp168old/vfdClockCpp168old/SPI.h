/*
 * SPI.h
 *
 * Created: 23.09.2020 21:38:19
 *  Author: user
 */ 

#include "main.h"

#ifndef SPI_H_
#define SPI_H_

enum bo {
	MSBFIRST,
	LSBFIRST
};

class SPI {
	private:
		char sclkPin;
		char rclkPin;
		char dataPin;
		bo bitorder;
	public:
		SPI (char SCLK, char RCLK, char DATA, bo bitOrder);
		
		void send(unsigned char _data);
		void CSLatch ( void );
	
};

#endif /* SPI_H_ */