/*
 * ds3231.h
 *
 * Created: 24.09.2020 3:02:22
 *  Author: user
 */ 


#ifndef DS3231_H_
#define DS3231_H_

#include "main.h"

typedef struct {
	int Sec;
	int Min;
	int Hour;
	
	int Day;
	int Month;
	int Year;
	
	int WeekDay;
	float temp;
} TDateTime;

enum _sqwout {
	disabled,
	_1HZ,
	_1_024kHz,
	_4_096kHz,
	_8_192kHz	
};

class DS3231 {
	private:
		int bcd2dec (int bcd);
		int dec2bcd (int dec);
		
		unsigned char read_packet(unsigned char addr);
		void write_packet(unsigned char addr, unsigned char data);
	public:
		DS3231();
		
		void ReadDateTime( TDateTime *dt );
		void SetDateTime( TDateTime dt );
		
		void SetSQWOutState( _sqwout OutFreq );
};


#endif /* DS3231_H_ */