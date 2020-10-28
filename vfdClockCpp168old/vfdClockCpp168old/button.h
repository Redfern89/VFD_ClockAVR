/*
 * button.h
 *
 * Created: 19.10.2020 21:40:29
 *  Author: 123
 */ 


#ifndef BUTTON_H_
#define BUTTON_H_

#include "main.h"

class BUTTON {
	private:
		DWORD int_cnt_short;
		DWORD int_cnt_long;
		DWORD int_cnt_press;
		unsigned char button_pin;
		unsigned char button_down_flag;
		unsigned char button_down_flag_global;
	public:
		BUTTON (unsigned char pin) {
			this -> int_cnt_short = 0;
			this -> int_cnt_long = 0;
			this -> int_cnt_press = 0;
			this -> button_down_flag_global = 0;
			this -> button_down_flag = 0;
			this -> button_pin = pin;
			pinMode(this -> button_pin, INPUT);
		}
		unsigned char down() {
			unsigned char res = 0;
			
			if (!digitalRead(this -> button_pin)) {
				this -> int_cnt_short++;
				if (this -> int_cnt_short == 1) {
					res = 1;
					this -> button_down_flag_global = 1;
					this -> button_down_flag = 1;
				}
			} else {
				if (this -> button_down_flag) {
					_delay_ms(5);
				}
				this -> button_down_flag = 0;
				this -> int_cnt_short = 0;
				res = 0;
			}
			
			return res;
		}
		
		unsigned char press(unsigned int skipTitcks=1) {
			unsigned char res = 0;
			
			if (!digitalRead(this -> button_pin)) {
				this -> int_cnt_press++;
				if (this -> int_cnt_press == 10000) {
					this -> button_down_flag_global = 1;
					res = 1;
					this -> int_cnt_press = 10000 - skipTitcks;
				}
			} else {
				//_delay_ms(5);
				this -> int_cnt_press = 0;
				res = 0;
			}
			
			return res;
		}
		
		unsigned char downLong(DWORD ticks=100) {
			unsigned char res = 0;
			
			if (!digitalRead(this -> button_pin)) {
				this -> int_cnt_long++;
				if (this -> int_cnt_long == ticks) {
					res = 1;
					this -> button_down_flag_global = 1;
				}
			} else {
				//_delay_ms(5);
				this -> int_cnt_long = 0;
				res = 0;
			}
			
			return res;
		}
		
		unsigned char up() {
			unsigned char res = 0;
			
			if (this -> button_down_flag_global) {
				if (digitalRead(this -> button_pin)) {
					res = 1;
					this -> button_down_flag_global = 0;
				}
			} else res = 0;
			
			return res;
		}

};


#endif /* BUTTON_H_ */