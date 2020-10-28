/*
 * DS18b20.h
 *
 * Created: 27.09.2020 3:24:00
 *  Author: 123
 */ 


#ifndef DS18B20_H_
#define DS18B20_H_

#include "main.h"

#define SKIPROM_COMMAND 0xCC
#define CONVERT_COMMAND 0x44
#define CMD_RSCRATCHPAD 0xBE

class DS18B20 {
	private:
		unsigned char DQ;
		unsigned char reset( void );
		void send_bit(char bit);
		unsigned char read_bit ( void );
		unsigned char read_packet( void );
		void write_packet( unsigned char byte );
	public:
		DS18B20 (unsigned char pin);
		void skipROM();
		void beginConversion( void );
		float readTemp( void );
};


#endif /* DS18B20_H_ */