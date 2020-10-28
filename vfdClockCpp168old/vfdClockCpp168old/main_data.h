/*
 * main_data.h
 *
 * Created: 23.09.2020 22:14:13
 *  Author: user
 */ 


#ifndef MAIN_DATA_H_
#define MAIN_DATA_H_

const unsigned char grids[8] = {
	0b00100000,
	0b00000100,
	0b00001000,
	0b00010000,
	0b00000001,
	0b10000000,
	0b00000010,
	0b01000000
};

const unsigned char digits[10] = {
	0b10111101,
	0b00001100,
	0b10011011,
	0b10011110,
	0b00101110,
	0b10110110,
	0b10110111,
	0b10001100,
	0b10111111,
	0b10111110
};

const unsigned char divider_horizontal = 0b00000010;
const unsigned char cel_symbol = 0b10101010;

#endif /* MAIN_DATA_H_ */