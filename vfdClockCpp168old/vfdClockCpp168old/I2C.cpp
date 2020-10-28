/*
 * I2C.cpp
 *
 * Created: 24.09.2020 0:06:46
 *  Author: user
 */ 

#include "i2c.h"

I2C::I2C () {
	TWBR = 0x20;
	TWSR = ( 1 << TWPS1) | (1 << TWPS0);
	TWCR = (1 << TWEN);
}

void I2C::Begin( void ) {
	TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTA);
	while ( !(TWCR & (1 << TWINT))){}
}

void I2C::Stop( void ) {
	TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
	while (TWCR & (1 << TWSTO)){}
}

void I2C::Write ( unsigned char data ) {
	TWDR = data;
	TWCR = (1 << TWEN) | (1 << TWINT);
	while (!(TWCR & (1 << TWINT))){}

}

unsigned char I2C::Read ( unsigned char ack ) {
	if (ack) TWCR |= (1 << TWEA);
	else TWCR &= ~(1 << TWEA);
	
	TWCR |= (1 << TWINT) | (1 << TWEN);
	while (!(TWCR & (1 << TWINT))){}

	return TWDR;
}