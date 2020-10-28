/*
 * i2c.h
 *
 * Created: 24.09.2020 0:05:26
 *  Author: user
 */ 


#ifndef I2C_H_
#define I2C_H_

#include "main.h"

class I2C {
	public:
		I2C ();
		
		void Begin ( void );
		void Stop ( void );
		void Write ( unsigned char data );
		unsigned char Read ( unsigned char ack );
};



#endif /* I2C_H_ */