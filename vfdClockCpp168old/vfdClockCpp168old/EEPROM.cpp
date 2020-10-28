/*
 * EEPROM.cpp
 *
 * Created: 23.10.2020 1:10:38
 *  Author: 123
 */ 

#include "EEPROM.h"

EEPROM::EEPROM (void) {
	
}

void EEPROM::WriteByte(unsigned int addr, unsigned char data) {
	while (EECR & (1 << EEPE));
	EEAR = addr;
	EEDR = data;
	EECR |= (1 << EEMPE);
	EECR |= (1 << EEPE);
}

unsigned char EEPROM::ReadByte(unsigned int addr) {
	while (EECR & (1 << EEPE));
	EEAR = addr;
	EECR |= (1 << EERE);

	return EEDR;
}

void EEPROM::WriteDWORD(unsigned int addr, DWORD data) {
	this -> WriteByte(addr, ((data >> 24UL) & 0xFF));
	this -> WriteByte(addr + 1, ((data >> 16UL) & 0xFF));
	this -> WriteByte(addr + 2, ((data >> 8UL) & 0xFF));
	this -> WriteByte(addr + 3, (data & 0xFF));
}

DWORD EEPROM::ReadDWORD(unsigned int addr) {
	unsigned char a = this -> ReadByte(addr);
	unsigned char b = this -> ReadByte(addr + 1);
	unsigned char c = this -> ReadByte(addr + 2);
	unsigned char d = this -> ReadByte(addr + 3);
	unsigned long int value = 0x00;
	
	value = a; value <<= 8UL; value |= b; value <<= 8UL; value |= c; value <<= 8UL; value |= d;
	return value;
}