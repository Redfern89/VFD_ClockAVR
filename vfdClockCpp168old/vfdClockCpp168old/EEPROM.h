/*
 * EEPROM.h
 *
 * Created: 23.10.2020 1:09:39
 *  Author: 123
 */ 


#ifndef EEPROM_H_
#define EEPROM_H_


#include "main.h"

class EEPROM {
	
	public:
		EEPROM(void);
		void WriteByte(unsigned int addr, unsigned char data);
		unsigned char ReadByte(unsigned int addr);
		
		void WriteDWORD(unsigned int addr, DWORD data);
		DWORD ReadDWORD(unsigned int addr);
		
};


#endif /* EEPROM_H_ */