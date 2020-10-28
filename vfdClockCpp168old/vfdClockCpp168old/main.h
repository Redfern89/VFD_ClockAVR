/*
 * main.h
 *
 * Created: 16.10.2020 9:20:32
 *  Author: user
 */ 


#ifndef MAIN_H_
#define MAIN_H_

typedef unsigned long int DWORD;

#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define bytesJoin(value, b1, b2, b3, b4) value = b1; value <<= 8UL; value |= b2; value <<= 8UL; value |= b3; value <<= 8UL; value |= b4;

#include "WFast.h"
#include "i2c.h"
#include "SPI.h"
#include "DS18b20.h"
#include "ds3231.h"
#include "main_data.h"
#include "button.h"
#include "EEPROM.h"

#define INTERRUPT ISR
#define IRQ0_isr INT0_vect
#define IRQ1_isr INT1_vect
#define TIM0_OVF_isr TIMER0_OVF_vect
#define TIM1_OVF_isr TIMER1_OVF_vect
#define TIM2_OVF_isr TIMER2_OVF_vect
#define USART_RX_isr USART_RX_vect

#define OK_BTN B0
#define DOWN_BTN D7
#define UP_BTN D6
#define MENU_BTN D5

#endif /* MAIN_H_ */