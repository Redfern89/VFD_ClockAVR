/*
 * SPI.cpp
 *
 * Created: 23.09.2020 21:38:35
 *  Author: user
 */ 

#include "SPI.h"

SPI::SPI (char SCLK, char RCLK, char DATA, bo bitOrder) {
	pinMode(SCLK, OUTPUT);
	pinMode(RCLK, OUTPUT);
	pinMode(DATA, OUTPUT);
	
	this -> sclkPin = SCLK;
	this -> rclkPin = RCLK;
	this -> dataPin = DATA;
	this -> bitorder = bitOrder;
}

void SPI::send(unsigned char _data) {
	unsigned char i;
	
	for (i = 0; i < 8; i++) {
		if (this -> bitorder == LSBFIRST)
			digitalWrite(this -> dataPin, !!(_data & (1 << (7 - i))));
		digitalWrite(this -> sclkPin, LOW);
		digitalWrite(this -> sclkPin, HIGH);
	}
}

void SPI::CSLatch( void ) {
	digitalWrite(this -> rclkPin, HIGH);
	digitalWrite(this -> rclkPin, LOW);
}