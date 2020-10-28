/*
 * DS18b20.cpp
 *
 * Created: 27.09.2020 3:24:41
 *  Author: 123
 */ 

#include "DS18b20.h"

DS18B20::DS18B20 (unsigned char pin) {
	this -> DQ = pin;
}

unsigned char DS18B20::reset( void ) {
	unsigned char response;
	
	pinMode(this -> DQ, OUTPUT);
	digitalWrite(this -> DQ, LOW);
	_delay_us(480);
	pinMode(this -> DQ, INPUT);
	_delay_us(60);
	
	response = digitalRead(this -> DQ);
	_delay_us(410);
	
	return response;
}

void DS18B20::send_bit(char bit) {
	pinMode(this -> DQ, OUTPUT);
	digitalWrite(this -> DQ, LOW);
	if (bit) {
		asm("CLI");
		_delay_us(5);
		pinMode(this -> DQ, INPUT);
		_delay_us(80);
		asm("SEI");
	} else {
		asm("CLI");
		_delay_us(80);
		pinMode(this -> DQ, INPUT);
		_delay_us(5);
		asm("SEI");
	}
}

unsigned char DS18B20::read_bit ( void ) {
	unsigned char bit = 0;
	asm("CLI");
	pinMode(this -> DQ, OUTPUT);
	digitalWrite(this -> DQ, LOW);
	
	_delay_us(2);
	pinMode(this -> DQ, INPUT);
	_delay_us(8);
	bit = digitalRead(this -> DQ);
	
	asm("SEI");
	_delay_us(50);
	return bit;
}

unsigned char DS18B20::read_packet( void ) {
	unsigned char r = 0;
	unsigned char i;
	for (i = 8; i; i--) {
		r = r >> 1;
		if (read_bit()) r |= 0x80;
	}
	return r;	
}

void DS18B20::write_packet( unsigned char byte ) {
	unsigned char i;
	for (i = 0; i < 8; i++) this -> send_bit(byte & (1 << i));	
}

void DS18B20::skipROM() {
	reset();
	this -> write_packet(SKIPROM_COMMAND);
}

void DS18B20::beginConversion( void ) {
	skipROM();
	this -> write_packet(CONVERT_COMMAND);
}

float DS18B20::readTemp( void ) {
	unsigned char tempL;
	unsigned char tempH;
	
	this -> skipROM();
	this -> write_packet(CMD_RSCRATCHPAD);
	
	tempL = this -> read_packet();
	tempH = this -> read_packet();
	
	return ((tempH << 8) + tempL) * 0.0625F; // Преобразование в цельсии
}