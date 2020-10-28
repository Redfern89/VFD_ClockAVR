/*
 * ds3231.cpp
 *
 * Created: 24.09.2020 3:02:37
 *  Author: user
 */ 

#include "ds3231.h"
I2C wire;

DS3231::DS3231 () {
	
}

int DS3231::bcd2dec(int bcd) {
	return ((bcd & 0x0f) + ((bcd >> 4) * 10));
}

int DS3231::dec2bcd(int dec) {
	return ((dec / 10) << 4) | (dec % 10);
}

unsigned char DS3231::read_packet(unsigned char addr) {
	wire.Begin();
	wire.Write(0xD0);
	wire.Write(addr);
	wire.Begin();
	wire.Write(0xD1);
	return wire.Read(0);
	wire.Stop();
}

void DS3231::write_packet(unsigned char addr, unsigned char data) {
	wire.Begin();
	wire.Write(0xD0);
	wire.Write(addr);
	wire.Write(data);
	wire.Stop();
}

void DS3231::ReadDateTime ( TDateTime *dt ) {
	char tempL, tempH;
	
	dt->Sec = this -> bcd2dec(read_packet(0x00));
	dt->Min = this -> bcd2dec(read_packet(0x01));
	dt->Hour = this -> bcd2dec(read_packet(0x02));
	
	dt->WeekDay = this -> bcd2dec(read_packet(0x03));
	dt->Day = this -> bcd2dec(read_packet(0x04));
	dt->Month = this -> bcd2dec(read_packet(0x05));
	dt->Year = this -> bcd2dec(read_packet(0x06));
	
	tempH = this -> read_packet(0x11);
	tempL = this -> read_packet(0x12);
	dt->temp = tempH + ((tempL >> 6) * 0.25F);
	
	wire.Stop();
}

void DS3231::SetDateTime( TDateTime dt ) {
	write_packet(0x00, this -> dec2bcd(dt.Sec));
	write_packet(0x01, this -> dec2bcd(dt.Min));
	write_packet(0x02, this -> dec2bcd(dt.Hour));
	
	write_packet(0x03, this -> dec2bcd(dt.WeekDay));
	write_packet(0x04, this -> dec2bcd(dt.Day));
	write_packet(0x05, this -> dec2bcd(dt.Month));
	write_packet(0x06, this -> dec2bcd(dt.Year));
}

void DS3231::SetSQWOutState( _sqwout OutFreq ) {
	if (OutFreq == _1HZ) 
		write_packet(0x0E, 0b00000010);
}