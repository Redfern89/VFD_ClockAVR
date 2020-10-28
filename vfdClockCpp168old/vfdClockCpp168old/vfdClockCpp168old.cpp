/*
 * vfdClockCpp168old.cpp
 *
 * Created: 16.10.2020 9:05:02
 *  Author: user
 */ 

// � ���� ����� ��� �������� �����������
#include "main.h"

// ����������� ���������
SPI SPITransfer(C3, C0, B5, LSBFIRST);
DS3231 RTC;
TDateTime DateTime;
EEPROM Memory;
DS18B20 TempSensor(C2);
BUTTON menuButtton(D5);
BUTTON upButton(D7);
BUTTON downButton(D6);
BUTTON okButton(B0);


/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~ ���������� ���������� ~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */


/*
	HEADER,
	REPEAT,
	CLEAR BIT,
	SET BIT,
	MIN HANDLER, RESET COUNTERS CONDITION
*/
unsigned int NEC_Proto[10] = {
	108, 114,
	89, 94,
	9, 12,
	17, 20,
	5, 1200
};

/* NEC-���������� */
#define NEC_REFERENCE_MESSAGE_MIN 108 // ����������� ����� ���������
#define NEC_REFERENCE_MESSAGE_MAX 114 // ������������ ����� ���������
#define NEC_REPEAT_MESSAGE_MIN 89 // ����������� ����� ��������� �������
#define NEC_REPEAT_MESSAGE_MAX 94 // ������������ ����� ��������� �������
#define NEC_CLEAR_BIT_TICK_MIN 9 // ����������� ������������ ��������� ����
#define NEC_CLEAR_BIT_TICK_MAX 12 // ������������ ������������ ��������� ����
#define NEC_SET_BIT_TICK_MIN 17 // ����������� ������������ ��������� �������
#define NEC_SET_BIT_TICK_MAX 20 // ������������ ������������ ��������� �������
#define NEC_MAX_WAIT_TICKS 1200 // ���� � ������� ���� ����� ������ �� ������ - ���������� ��� �������� � ���������������
#define NEC_MIN_HANDLER_TICKS 5 // ����������� ������ ��������� �������
#define T2_EN TCCR2B |= (1 << CS21) // ������ ������� ������� 2
#define T2_STOP TCCR2B = 0x00 // ������ ��������� ������� 2

volatile unsigned char NEC_DONE_FLAG = 0; // ���� ���������� ��������� ������� NEC
volatile unsigned int NEC_SCLK = 0; // ������� ����� ������� ��������� � NEC. ��� ������� ��� �� 1 �������
volatile unsigned char NEC_RECV_CNT = 0; // ������� ������ �������
volatile unsigned char NEC_START_FLAG = 0; // ���� ������ ��������� ������������������
volatile unsigned char NEC_REPEAT_FLAG = 0; // ���� ����, ��� ������������������ �����������
volatile unsigned char NEC_REPEAT_WORK = 0; // ��� ������ �������. ��������� ��. � ���� ����������
volatile DWORD command = 0x00000000; // ���������� ���� �������
/*
	� NEC-������ ����� 4 �����:
		1. �����
		2. �������� ������ ��� ��� �����������
		3. �������
		4. �������� �������
*/
volatile unsigned char necAddr1 = 0x00;
volatile unsigned char necAddr2 = 0x00;
volatile unsigned char necCmd1 = 0x00;
volatile unsigned char necCmd2 = 0x00;

// ������� ����� ������ ���������� � �������
#define POWER_OFF 0
#define MENU 1
#define PLUS 2
#define MINUS 3
#define CLEAR 4
#define PLAY 5
#define FWND 6
#define RWND 7
#define RETURN 8

/* ������������ ��������� */
const int max_groups  = 8; // ����������� ����������� �����
volatile unsigned char display_pos = 0; // ������� ������� �����������
volatile unsigned char display_data[8]; // ������ ���������

/* ������ ���������� ��� ������ ������� */
volatile unsigned char SQW_Flag = 0x00;
unsigned long long int tmr0 = 0;
unsigned long long int tmr1 = 0;
unsigned long long int tmr2 = 0;
unsigned long long int tmr3 = 0;
volatile unsigned long long int millis = 0; // ����������� ����������� � ������� �������

/* �������� ������� */
#define setInterval(n, tmr, code) { if ((millis - tmr) >= n) { tmr = millis; code; }}

/* ���������������� ��� �������. ������ ��� ���� �� ����� :)) */
// void USORT_Init(unsigned char ubrr) {
// 	UBRR0H = (unsigned char)(ubrr>>8);
// 	UBRR0L = (unsigned char)ubrr;
// 	/*Enable receiver and transmitter */
// 	UCSR0B = (1<<RXEN0)|(1<<TXEN0);
// 	/* Set frame format: 8data, 2stop*/
// 	UCSR0C = (1<<USBS0)|(3<<UCSZ00);
// }
// 
// void USORT_Transmit( unsigned char data ) {
// 	/* Wait for empty transmit buffer */
// 	while ( !( UCSR0A & (1<<UDRE0)) );
// 	/* Put data into buffer, sends the data */
// 	UDR0 = data;
// }
// 
// // �������� ������ ����� UART
// void USORT_String(char *stringPtr){
// 	while(*stringPtr != 0) {
// 		USORT_Transmit(*stringPtr);
// 		stringPtr++;
// 	}
// }

// ������������ ���������
INTERRUPT (TIM0_OVF_isr) {
	TCNT0 = 180; // ��� ����� � �������� ������� �����, ���-�� �� �������, �� ��������� ������� � ��������� ����

	/* �������� ������� ������ � ��������� �������� */
	display_pos = (display_pos + 1) % max_groups; // ������� �� ������������� �������� � �������� ������
	SPITransfer.send(grids[display_pos]);  // �������� ������ �� �������
	SPITransfer.send(display_data[display_pos]); // �������� ������ �� ��������
	SPITransfer.CSLatch(); // ����������� ��������
}

// ���� � ���������� 2�� ������� �� ������������. ������ 100���
INTERRUPT (TIM2_OVF_isr) {
	TCNT2 = 0x38; // ��� �� ��� ��� ��� ���������
	if (++NEC_SCLK >= NEC_MAX_WAIT_TICKS) { // ����������� ���������� NEC_SCLK ������ 100���
		// � ��� ���������� ������ ���� ������ - ��� ����������
		T2_STOP;
		NEC_START_FLAG = 0;
		NEC_RECV_CNT = 0;
		NEC_REPEAT_WORK = 0;
		command = 0x00000000;
		necAddr1 = 0x00;
		necAddr2 = 0x00;
		necCmd1 = 0x00;
		necCmd2 = 0x00;
	}
}

// ���� �� ������� ����������
INTERRUPT (IRQ1_isr) {
	T2_EN; // ��� ������� ������ ��������� ����� - ��������� ������
	if (NEC_SCLK > NEC_MIN_HANDLER_TICKS) { // ���, ��� ������ ������-�� �������� - �� ������������
		// ��������� ������� ������ � ������� ������ ~13.2��
		if (NEC_SCLK >= NEC_REFERENCE_MESSAGE_MIN && NEC_SCLK <= NEC_REFERENCE_MESSAGE_MAX) {
			NEC_START_FLAG = 1; // ������ ��������� ���������
			NEC_RECV_CNT = 0; // ���������� ������� ������ ������
			/*
				 ��� ���� ����� �����, ��� �� ������������ ������ ��� ������ ���������� ��������, 
				 ���-�� ��� � �������� ��������� ����� ���� �������� ����������. � ���� NEC_REPEAT_FLAG
				 ����� ���� ��� ����� � ���������� �������� � �� �����, �� ��� ������������ � �������� ���������
			*/
			NEC_REPEAT_WORK = 0; // ���������� ���� ������ ������� �������
		}
		// ���� ������� ���������� ��������� ������� - ����� ������ ���������� ���������
		if (NEC_START_FLAG) {
			// ������� � NEC ������� ~11��
			if (NEC_SCLK >= NEC_REPEAT_MESSAGE_MIN && NEC_SCLK <= NEC_REPEAT_MESSAGE_MAX) {
				NEC_REPEAT_FLAG = 1; // ������ ������
				NEC_REPEAT_WORK = 1; // ���� ������ �������
			}
			/* ����, ��-��������, �� ����� ���� ���� ����������� */
			if (NEC_SCLK >= NEC_CLEAR_BIT_TICK_MIN && NEC_SCLK <= NEC_CLEAR_BIT_TICK_MAX) { // ������������ ���. 0 - ~1.1��
				NEC_RECV_CNT++; // ���������� ������� �������� ������
				/* ��� ������ ��� ����������� �� ���������� */
				if (NEC_RECV_CNT >= 0 && NEC_RECV_CNT < 9) bitClear(necAddr1, NEC_RECV_CNT);
				if (NEC_RECV_CNT >= 9 && NEC_RECV_CNT < 17) bitClear(necAddr2, (NEC_RECV_CNT - 9));
				if (NEC_RECV_CNT >= 17 && NEC_RECV_CNT < 25) bitClear(necCmd1, (NEC_RECV_CNT - 17));
				if (NEC_RECV_CNT >= 25 && NEC_RECV_CNT < 33) bitClear(necCmd2, (NEC_RECV_CNT - 25));
			}
		
			if (NEC_SCLK >= NEC_SET_BIT_TICK_MIN && NEC_SCLK <= NEC_SET_BIT_TICK_MAX) { // ������������ ���. 1 - ~2.2��
				/* ��� ��� ��-�� �����, ��� � � ������ */
				NEC_RECV_CNT++;
				if (NEC_RECV_CNT >= 0 && NEC_RECV_CNT < 9) bitSet(necAddr1, NEC_RECV_CNT);
				if (NEC_RECV_CNT >= 9 && NEC_RECV_CNT < 17) bitSet(necAddr2, (NEC_RECV_CNT - 9));
				if (NEC_RECV_CNT >= 17 && NEC_RECV_CNT < 25) bitSet(necCmd1, (NEC_RECV_CNT - 17));
				if (NEC_RECV_CNT >= 25 && NEC_RECV_CNT < 33) bitSet(necCmd2, (NEC_RECV_CNT - 25));
			}
			// ��� ������ ������ 32� ��� ���������
			if (NEC_RECV_CNT == 32) {
				// ��������� ����������� �������� ������
				// - � ������ ������ ��������� ��� ������ � ������� ���� ��� �����, ��� � ���������
				// - �� ������ ������ ����� �������� ��� ����� � �� ����� ��������
				// ������ ������ ������ � ��������� ����� � ������� �� ������ FFh - �� ��������, ��� ��� �����������
				if ((((necAddr1 + necAddr2) == 0xFF) && ((necCmd1 + necCmd2) == 0xff)) || ((necCmd1 + necCmd2) == 0xFF)) {
					NEC_DONE_FLAG = 1;
					NEC_RECV_CNT = 0;
					bytesJoin(command, necAddr1, necAddr2, necCmd1, necCmd2);// ��������� ��� � ���� �������
				}
				T2_STOP; // ������������� �������
			}
		}
	}
	NEC_SCLK = 0; // �������� ������� �������
}

// ������� ����������� �����������. ��������� ������������� ��������. � ����� 
// �������� ��� ����� ������ - ��� ���� �������������� ����������
void printTemperature(float temp) {
	// ���� ����� ������������� - ������ ������� ����� � �������� ��� � ������� ������ )
	if (temp < 0) {
		temp = fabs(temp);
		display_data[1] = divider_horizontal; // �� ��������� ����� �� ������ �������
	} else {
		display_data[1] = 0x00; // ���� ������������� ����� - ������� ������ ������
	}
	
	// ������� ��� ���������
	display_data[0] = 0x00;
	display_data[6] = 0x00;
	display_data[7] = 0x00;
	
	if (temp == 0.0F) { // ���� ������������ 0�, ��...
		display_data[2] = digits[0] | (1 << 6); // ���������� 0. � ������� �������
		display_data[3] = digits[0]; // ���� � ��������� �������
		display_data[4] = cel_symbol; // ������ � � ����� �������
		display_data[5] = 0x00; // ������� ������ ������, ���� ��� ���-�� ���� �� �����
	} else if (temp >= 0.1F && temp <= 9.9F) { // ���� ����������� ������ 0.1� � ������ 9.9�, ��...
		display_data[2] = digits[(unsigned char)temp] | (1 << 6); // ��������� �������� �� ���������� � ������ ����� ����� ����
		display_data[3] = digits[(unsigned char)(temp * 10.0F) % 10]; // �������� ����� �� 10 � ������� �� ������� �� 10 - ����� ������ ������ 
		display_data[4] = cel_symbol; // ������ ������ �������
		display_data[5] = 0x00; // ���� ��� ���-�� ���� �� �����, �� �������
	} else if (temp >= 10.0F && temp <= 99.9F) { // ���� ����������� ������ ��� ����� 10.0� � ������ ��� ����� 99.9�, ��...
		display_data[2] = digits[(unsigned char)(temp) % 100 / 10]; // ������ ������ ��� - ������� �� ������� �� 100, ����������� �� 10
		display_data[3] = digits[(unsigned char)(temp) % 10] | (1 << 6); // ������ ������ ��� ������� �� ������� �� 10. ���-�� ������ �����
		display_data[4] = digits[(unsigned char)(temp * 10.0F) % 10]; // ������ ������ - ��� ������� �� ������� �� ������, �� ����� ����������� �� ������
		display_data[5] = cel_symbol; // ���������� ������ �������
	}
}

/*
	 ������� ����������� ����
		����, �����, ��� - ��� �������� ���������
		dVisible, mVisible, yVisible - ��������� ����������� ���, ������ � ����. 1 - ������������, 0 - �� ������������
		dt - �������� ����������� �����-����������� ����� ����, ������� � �����
*/
void printDate(int day, int month, int year, int dVisible=1, int mVisible=1, int yVisible=1, unsigned char dt=1) {
	if (dVisible) { // ���� �� ����� ���������� ���, �� ..
		if (day == 0) { // ���� ���� ����� ���� (����� ������� �� ��������, �� � ���-�� �� � - ����-�� ������ �� ����� ���������� ����� ����������)
			display_data[0] = digits[0]; // � ������ ������ �������� ����
			display_data[1] = (dt) ? digits[0] | (1 << 6) : digits[0]; // � ���� ����. ��� ����������� ���������� �����-�����������
		} else if (day >= 1 && day <= 9) { // ��� �� 1 �� 9
			display_data[0] = digits[0]; // ���������� ������� ����
			display_data[1] = (dt) ? digits[day] | (1 << 6) : digits[day]; // ���������� ����
		} else if (day >= 10 && day <= 99) { // ��� �� 10 �� 99
			display_data[0] = digits[(unsigned char)((day % 100) / 10)]; // ��������� ��� �������� �������� ������ ������
			display_data[1] = (dt) ? digits[(unsigned char)(day % 10)] | (1 << 6) : digits[(unsigned char)(day % 10)]; // � ������. ��� ���������� ���������� �����-�����������
		}
	} else {
		// ���� �� ���������� - �� �� ��������. ��������� ������ �����-�����������
		display_data[0] = 0x00;
		display_data[1] = 0b01000000;
	}
	/*
		� ������� � ����� - ��� ���� �����, ��� ���� ��� ��� ����������� )))))
	*/
	if (mVisible) {
		if (month == 0) {
			display_data[2] = digits[0];
			display_data[3] = (dt) ? digits[0] | (1 << 6) : digits[0];
		} else if (month > 0 && month <= 9) {
			display_data[2] = digits[0];
			display_data[3] = (dt) ? digits[month] | (1 << 6) : digits[month];
		} else if (month >= 10 && month <= 99) {
			display_data[2] = digits[(unsigned char)((month % 100) / 10)];
			display_data[3] = (dt) ? digits[(unsigned char)(month % 10)] | (1 << 6) : digits[(unsigned char)(month % 10)];
		}
	} else {
		display_data[2] = 0x00;
		display_data[3] = 0b01000000;
	}
	if (yVisible) {
		if (year == 0) {
			display_data[4] = digits[2];
			display_data[5] = digits[0];
			display_data[6] = digits[0];
			display_data[7] = digits[0];
		} else if (year > 0 && year <= 9) {
			display_data[4] = digits[2];
			display_data[5] = digits[0];
			display_data[6] = digits[0];
			display_data[7] = digits[year];
		} else if (year >= 10 && year <= 99) {
			display_data[4] = digits[2];
			display_data[5] = digits[0];
			display_data[6] = digits[(unsigned char)((year % 100) / 10)];
			display_data[7] = digits[(unsigned char)(year % 10)];
		}
	} else {
		display_data[4] = 0x00;
		display_data[5] = 0x00;
		display_data[6] = 0x00;
		display_data[7] = 0x00;
	}
}

/*
	������� ����������� ������� �� �������
	hour, minutes, seconds - ����, ������ � �������
	hVisible, mVisible, sVisible - �����������. 0 - �� ����������, 1 - ����������
	
	������� ����� ��������� ������� printDate, ��� ����������� ������ �� ����.
*/
void printTime(int hour, int Minutes, int Seconds, int hVisible=1, int mVisible=1, int sVisible=1) {
	if (hVisible) {
		if (hour == 0) {
			display_data[0] = digits[0];
			display_data[1] = digits[0];
		} else if (hour > 0 && hour <= 9) {
			display_data[0] = digits[0];
			display_data[1] = digits[hour];
		} else if (hour >= 10 && hour <= 99) {
			display_data[0] = digits[(unsigned char)((hour % 100) / 10)];
			display_data[1] = digits[(unsigned char)(hour % 10)];
		}
	} else {
		display_data[0] = 0x00;
		display_data[1] = 0x00;
	}
	
	if (mVisible) {
		if (Minutes == 0) {
			display_data[3] = digits[0];
			display_data[4] = digits[0];
		} else if (Minutes > 0 && Minutes <= 9) {
			display_data[3] = digits[0];
			display_data[4] = digits[Minutes];
		} else if (Minutes >= 10 && Minutes <= 99) {
			display_data[3] = digits[(unsigned char)((Minutes % 100) / 10)];
			display_data[4] = digits[(unsigned char)(Minutes % 10)];
		}
	} else {
		display_data[3] = 0x00;
		display_data[4] = 0x00;
	}
	
	if (sVisible) {
		if (Seconds == 0) {
			display_data[6] = digits[0];
			display_data[7] = digits[0];
		} else if (Seconds > 0 && Seconds <= 9) {
			display_data[6] = digits[0];
			display_data[7] = digits[Seconds];
		} else if (Seconds >= 10 && Seconds <= 99) {
			display_data[6] = digits[(unsigned char)((Seconds % 100) / 10)];
			display_data[7] = digits[(unsigned char)(Seconds % 10)];
		}
	} else {
		display_data[6] = 0x00;
		display_data[7] = 0x00;
	}
}

// ������� ����������� ��������-������������
void printDividers (char print) {
	if (print) {
		display_data[2] = divider_horizontal;
		display_data[5] = divider_horizontal;
	} else {
		display_data[2] = 0x00;
		display_data[5] = 0x00;
	}
}

// ���� � ������� ����������. ��� SQW_Flag ������ ������� ��������������� � 1. ������ ������� - �����
INTERRUPT (IRQ0_isr) {
	SQW_Flag = 1;
}

// ���� � ���������� ������� �������. ���� ������� ������������ � ������� ������� �������
INTERRUPT (TIM1_OVF_isr) {
	TCNT1 = 0xFF06;
	millis++;	
}

// ������ ���������� ���� � ������ � ������������ ����������� ����. ����� ��� �������� ����/�������
int dayPerMonth(int month, int year) {
	int days = 0;
	int dayInFeb = 0;

	if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) dayInFeb = 28; 
	else dayInFeb = 29;

	if (month == 1) days = 31;
	if (month == 2) days = dayInFeb;
	if (month == 3) days = 31;
	if (month == 4) days = 30;
	if (month == 5) days = 31;
	if (month == 6) days = 30;
	if (month == 7) days = 31;
	if (month == 8) days = 31;
	if (month == 9) days = 30;
	if (month == 10) days = 31;
	if (month == 11) days = 30;
	if (month == 12) days = 31;
	
	return days;
}

// ������� ��� ������� �������
void fillDisplay(unsigned char character) {
	for (unsigned char i = 0; i < 8; i++) display_data[i] = character;
}

// ������� ��� ������� ������� � ���������� ������� ������ ������� � �����
void fillDisplay(unsigned char character, unsigned char pos, unsigned char length) {
	unsigned char i = 0;
	for (i = pos; i <= length; i++) {
		display_data[i] = character;
	}
}

// ������ ������������ �� ��������� �����
void EffectNextDisplay(void) {
	fillDisplay(0x00);
	for (int i = 0; i < 8; i++) {
		display_data[i] = divider_horizontal;
		_delay_ms(40);
	}
}

// ������ ������������ �� ���������� �����
void EffectPrevDisplay(void) {
	fillDisplay(0x00);
	for (int i = 8; i > 0; i--) {
		display_data[i] = divider_horizontal;
		_delay_ms(40);
	}
}

// ������� ����������� ����� �� �������
void printInt(unsigned char value) {
	if (value == 0) {
		display_data[0] = digits[0];
		display_data[1] = 0x00;
	} else if (value >= 1 && value <= 9) {
		display_data[0] = digits[value];
		display_data[1] = 0x00;
	} else if (value >= 10 && value <= 99) {
		display_data[0] = digits[(unsigned char)((value % 100) / 10)];
		display_data[1] = digits[(unsigned char)(value % 10)];		
	}
}


int main (void) {
	cli(); // ��������� ����������
	
	//USORT_Init(8); 
	RTC.SetSQWOutState(_1HZ);

	// ������������� �������/��������� 0 ��� ������������ ���������
	TCCR0B |= (1 << CS02);
	TIMSK0 |= (1 << TOIE0);
	
	T2_STOP; // ��������� ������� 2
	TIMSK2 |= (1 << TOIE2); // ����������� ������ 2 �� ������������
	
	// ������ 1 ��� �������� ������������ � ������� �������. ����� ��� �������� ��� ������
	TCCR1B |= (0 << CS12) | (1 << CS11) | (1 << CS10);
	TIMSK1 |= (1 << TOIE1);
	TCNT1 = 0xFF06;
	
	// ������������� ������� �����������
	EICRA |= (1 << ISC11) | (0 << ISC10); // ���������� 1 �� ���� ������ ��� ��������� NEC
	EICRA |= (0 << ISC01) | (1 << ISC00); // ���������� 0 �� ����� ��������� ������� ��� ��������� SQW � DS3231
	EIMSK |= (1 << INT1) | (1 << INT0); // ���������� ������� ����������
	
	sei(); // ��������� ��������� ����������
	
	char buttonFlag = 0; // ���� ������� �� ������ (����� ��� ����� � ����������)
	char buttonPressFlag = 0; // ���� ������� � ��������� ������ (����� ��� ����� � ����������)
	char menuFlag = 0; // ���� ����
	char enterMenuFlag = 0; // ���� ����� � ���� (����� ��� ������� ������� �������� ��� ���������)
	char irdaSettingsFlag = 0; // ���� �������� ��-������
	char enterIrdaSettingsFlag = 0; // ���� ����� � ��������� ��-������
	char menuItemBlinkFlag = 0; // ���� ������� �������������� ������ � ����
	char blinkDividersFlag = 0; // ���� ������� ������������ �� �������
	char blinkDotsFlag = 0; // ���� ������� �����-������������ � ����
	//char tempConversionEndFlag = 0; // ���� ��������� �������������� ����������� (������ �� �����, ��� ����������� ������� ����� � �����������)
	char irdaRecvCommandFlag = 0; // ���� ������ ������� � ������ (��� �������� ������)
	char irdaSaveCommandFlag = 0; // ���� ��������� ������ ������� � EEPROM
	
	int setHour = 0, setMinutes = 0, setSeconds = 0; // ������������� �������� (�����)
	int setDay = 0, setMonth = 0, setYear = 0; // ������������� �������� (����)
	int maxDaysPerMonth = 0; // ���� � ������
	int printMode = 0; // ����������� �� �������� ������� (0 - ����, 1 - ����, 2 - �����������)
	int tempRequetCNT = 0; // ������� ������ �������
	int irdaSettingButtonIndex = 0; // �������, ������������� ������ ��-������
	int irdaSettingButtonIndex_old = 0; // ����������, ������������� ������ �� ��-������
	int irdaSettingAnimationReadyPos = 0; // ������� ������� ������� (�������� ��� ��������� ��-������)
	int menuLevel = 0; // ������� ������� � ����
	int oldMenuLevel = 0; // ���������� ������� � ����
	
	DWORD irdaRecvCommand = 0x00000000; // �������� ������� � ������ ��� �������� ��-������
	DWORD irdaCommands[9]; // ������ ����� ������ � ��-������
	
	float tempC = 0.0F; // ����������� �� �������� �������
	
	// ������ ��� �������� ����� ������ ��-������
	for (unsigned char i = 0; i < 9; i++) {
		// �������� �� 4 - ��� DWORD ������� �� 4� ����
		irdaCommands[i] = Memory.ReadDWORD(i*4);
	}

	while (1) {
		
		/*
			����� ������� ����������� DS18b20
			0 - ������ �� ������. 
			1 - ������ �� ��������������. 
			2 - �������. 
			3 - ����� ������ �����������.
		*/
		setInterval(1000, tmr1, {
			if (tempRequetCNT == 1) TempSensor.beginConversion(); // ������ �� ��������������
			//if (tempRequetCNT == 2) tempConversionEndFlag = 1; // �������
			if (tempRequetCNT == 3) { // ������ �����������
				tempRequetCNT = 0; 
				//tempConversionEndFlag = 0;
				tempC = TempSensor.readTemp();
			}
			tempRequetCNT++; // ��� ��������� ������� ������
		});		
		
		// ��� ������ ������� �� ������ OK
		if (okButton.downLong(20000)) {
			irdaSettingsFlag ^= 1; // ����������� ��������� ����� �������� ��-������. �.�. ��� ������ � ��������� ��� ������� �� ���
			enterIrdaSettingsFlag = irdaSettingsFlag; // ���� �� � ���������� ��-������ - ����� ���� ���� ���������� � �������, ���-�� ������������������� ��������� ��������
		}
		
		// ��-�� ����� � ����
		if (menuButtton.downLong(20000)) {
			menuFlag ^= 1;
			enterMenuFlag = menuFlag;
		}
		
		// ��� �������� ������� �� ������ MENU
		if (menuButtton.down()) {
			buttonFlag = 1;
			// ���� �� � ���� - �� ����������� ������ ����, ���� �� � ���� - ��� ������ ������� �������� ������� � ���������� ��-������
			command = (menuFlag) ? irdaCommands[FWND] : irdaCommands[CLEAR];
		}
		
		// ������ "����"
		if (downButton.down()) {
			buttonFlag = 1;
			// ���� �� � ���� - �� ���������� ��������, ���� �� ���, �� ����������� ������������ ������
			command = (menuFlag) ? irdaCommands[PLUS] : irdaCommands[FWND];
			
			// ���� �� ����������� ������ ��-������
			if (irdaSettingsFlag) {
				irdaSettingButtonIndex++; // ���������� ������������� ������
				if (irdaSettingButtonIndex > 8) irdaSettingButtonIndex = 0; // ���� ���������� �� ������� - ���� �������
			}
		}
		
		// ������ "�����"
		if (upButton.down()) {
			buttonFlag = 1;
			// ���� �� � ���� - �� �������� ��������, ���� �� ���, �� ����������� ������������ ������
			command = (menuFlag) ? irdaCommands[MINUS] : irdaCommands[RWND];
			
			// ���� �� ����������� ������ ��-������
			if (irdaSettingsFlag) {
				irdaSettingButtonIndex--; // �������� ������������� ������
				if (irdaSettingButtonIndex < 0) irdaSettingButtonIndex = 8; // ���� ���������� �� ������� - ���� � �����
			}
		}
		
		// ������ ������� �� ������ "�����"
		if (upButton.press(900)) {
			buttonPressFlag = 1; // ���������� ������������� ������
			command = (menuFlag) ? irdaCommands[MINUS] : 0x00;
		}
		
		// ������ ������� �� ������ "����"
		if (downButton.press(900)) {
			buttonPressFlag = 1;
			command = (menuFlag) ? irdaCommands[PLUS] : 0x00;
		}
		
		// ��� ���������� ������ ����� ��� ���� - ������� �������, ��� ������ ��������
		if (upButton.up() || downButton.up()) {
			buttonPressFlag = 0;
		}
		
		// ������ ��
		if (okButton.down()) {
			buttonFlag = 1;
			command = (menuFlag) ? irdaCommands[PLAY] : 0x00;
		}
		
		// ���� ������ ������� � ������ ��� ���������� ��-������
		if (NEC_DONE_FLAG && irdaSettingsFlag && !irdaSaveCommandFlag) {
			NEC_DONE_FLAG = 0; // ��� ������
			irdaRecvCommand = command; // ��������� �� ��������� ����������
			irdaRecvCommandFlag = 1; // ������� �������, ��� ������� ���������
		}
		
		/*
			���������� ������
				- ��-������
				- ������� �������
				- ������� �� ������
				- ��������� ������
		*/
		if (NEC_DONE_FLAG || NEC_REPEAT_FLAG || buttonFlag || buttonPressFlag) {
			// �������� ��� �������� �����
			NEC_DONE_FLAG = 0;
			NEC_REPEAT_FLAG = 0;
			buttonFlag = 0;	
			
			if (command == irdaCommands[CLEAR] && irdaSettingsFlag && !menuFlag && !NEC_REPEAT_WORK) {
				irdaRecvCommandFlag = 0;
				irdaSaveCommandFlag = 0;
			}
			
			if (command == irdaCommands[FWND] && !menuFlag && !irdaSettingsFlag && !NEC_REPEAT_WORK) {
				fillDisplay(0x00);
				EffectNextDisplay();
				printMode++;
				if (printMode > 2) printMode = 0;
			}
			
			if (command == irdaCommands[RWND] && !menuFlag && !irdaSettingsFlag && !NEC_REPEAT_WORK) {
				fillDisplay(0x00);
				EffectPrevDisplay();
				printMode--;
				if (printMode < 0) printMode = 2;
			}
			
			if (command == irdaCommands[PLUS] && menuFlag && !irdaSettingsFlag) {
				if (menuLevel == 0) {
					setHour++;
					if (setHour > 23) setHour = 0;
					printTime(setHour, setMinutes, setSeconds);
				}
				if (menuLevel == 1) {
					setMinutes++;
					if (setMinutes > 59) setMinutes = 0;
					printTime(setHour, setMinutes, setSeconds);
				}
				if (menuLevel == 2) {
					setSeconds++;
					if (setSeconds > 59) setSeconds = 0;
					printTime(setHour, setMinutes, setSeconds);
				}

				if (menuLevel == 3) {
					setDay++;
					maxDaysPerMonth = dayPerMonth(setMonth, setYear);
					if (setDay > maxDaysPerMonth) setDay = 1;
					printDate(setDay, setMonth, setYear);
				}
				if (menuLevel == 4) {
					setMonth++;
					if (setMonth > 12) setMonth = 1;
					maxDaysPerMonth = dayPerMonth(setMonth, setYear);
					if (setDay > maxDaysPerMonth) setDay = maxDaysPerMonth;
					printDate(setDay, setMonth, setYear);
				}
				if (menuLevel == 5) {
					setYear++;
					if (setYear > 99) setYear = 0;
					maxDaysPerMonth = dayPerMonth(setMonth, setYear);
					if (setDay > maxDaysPerMonth) setDay = maxDaysPerMonth;
					printDate(setDay, setMonth, setYear);
				}
			}
			if (command == irdaCommands[MINUS] && menuFlag && !irdaSettingsFlag) {
				if (menuLevel == 0) {
					setHour--;
					if (setHour < 0) setHour = 23;
					printTime(setHour, setMinutes, setSeconds);
				}
				if (menuLevel == 1) {
					setMinutes--;
					if (setMinutes < 0) setMinutes = 59;
					printTime(setHour, setMinutes, setSeconds);
				}
				if (menuLevel == 2) {
					setSeconds--;
					if (setSeconds < 0) setSeconds = 59;
					printTime(setHour, setMinutes, setSeconds);
				}
				if (menuLevel == 3) {
					setDay--;
					maxDaysPerMonth = dayPerMonth(setMonth, setYear);
					if (setDay < 1) setDay = maxDaysPerMonth;
					printDate(setDay, setMonth, setYear);
				}
				if (menuLevel == 4) {
					setMonth--;
					if (setMonth < 1) setMonth = 12;
					maxDaysPerMonth = dayPerMonth(setMonth, setYear);
					if (setDay > maxDaysPerMonth) setDay = maxDaysPerMonth;
					printDate(setDay, setMonth, setYear);
				}
				if (menuLevel == 5) {
					setYear--;
					if (setYear < 0) setYear = 99;
					maxDaysPerMonth = dayPerMonth(setMonth, setYear);
					if (setDay > maxDaysPerMonth) setDay = maxDaysPerMonth;
					printDate(setDay, setMonth, setYear);
				}
			}
			
			if (command == irdaCommands[MENU] && !NEC_REPEAT_WORK && !irdaSettingsFlag) {
				menuFlag = 1;
				enterMenuFlag = 1;
			}
			if (command == irdaCommands[RETURN] && menuFlag && !NEC_REPEAT_WORK && !irdaSettingsFlag) {
				menuFlag = 0;
			}
			
			if (command == irdaCommands[FWND] && menuFlag && !NEC_REPEAT_WORK && !irdaSettingsFlag) {
				menuLevel++;
				if (menuLevel > 5) menuLevel = 0;
			}
			if (command == irdaCommands[RWND] && menuFlag && !NEC_REPEAT_WORK && !irdaSettingsFlag) {
				menuLevel--;
				if (menuLevel < 0) menuLevel = 5;
			}
			if (command == irdaCommands[PLAY] && menuFlag && !NEC_REPEAT_WORK && !irdaSettingsFlag) {
				DateTime.Hour = setHour;
				DateTime.Min = setMinutes;
				DateTime.Sec = setSeconds;
				DateTime.Day = setDay;
				DateTime.Month = setMonth;
				DateTime.Year = setYear;
				RTC.SetDateTime(DateTime);
				menuFlag = 0;
			}

			command = (buttonPressFlag) ? 0x00 : command;
		}
		
		if (menuFlag) {
			if (enterMenuFlag) {
				enterMenuFlag = 0;
				menuLevel = 0;
				RTC.ReadDateTime(&DateTime);
				setHour = DateTime.Hour;
				setMinutes = DateTime.Min;
				setSeconds = DateTime.Sec;
				setDay = DateTime.Day;
				setMonth = DateTime.Month;
				setYear = DateTime.Year;
				printDividers(0);
			}
			
			if (oldMenuLevel != menuLevel) {
				oldMenuLevel = menuLevel;
				if (menuLevel >= 0 && menuLevel <= 2) printDividers(0);
			}
			
			setInterval(540, tmr0, {
				menuItemBlinkFlag ^= 1;
				if (menuLevel == 0) printTime(setHour, setMinutes, setSeconds, menuItemBlinkFlag, 1, 1);
				if (menuLevel == 1) printTime(setHour, setMinutes, setSeconds, 1, menuItemBlinkFlag, 1);
				if (menuLevel == 2) printTime(setHour, setMinutes, setSeconds, 1, 1, menuItemBlinkFlag);
				if (menuLevel == 3) printDate(setDay, setMonth, setYear, menuItemBlinkFlag, 1, 1);
				if (menuLevel == 4) printDate(setDay, setMonth, setYear, 1, menuItemBlinkFlag, 1);
				if (menuLevel == 5) printDate(setDay, setMonth, setYear, 1, 1, menuItemBlinkFlag);
			});
		}
		
		if (irdaSettingsFlag) {
			if (enterIrdaSettingsFlag) {
				enterIrdaSettingsFlag = 0;
				irdaSettingButtonIndex = 0;
				fillDisplay(0x00);
				printInt(1);
			}
			if (irdaSettingButtonIndex != irdaSettingButtonIndex_old) {
				irdaSettingButtonIndex_old = irdaSettingButtonIndex;
				printInt(irdaSettingButtonIndex +1);
				irdaRecvCommandFlag = 0;
				irdaRecvCommand = 0x00;
				irdaSaveCommandFlag = 0;
			}
			
			if (!irdaRecvCommandFlag && !irdaSaveCommandFlag) {
				setInterval(140, tmr0, {
					if (irdaSettingAnimationReadyPos++ > 2) irdaSettingAnimationReadyPos = 0;
					fillDisplay(0x00, 1, 7);
					
					display_data[irdaSettingAnimationReadyPos +1] = divider_horizontal;
					display_data[5] = 0b00000011;
					display_data[6] = 0b00011111;
					display_data[7] = 0b00111110;
				});
			}
			if (irdaRecvCommandFlag) {
				irdaRecvCommandFlag = 0;
				irdaSaveCommandFlag = 1;
				fillDisplay(0x00);
				printInt(irdaSettingButtonIndex +1);
				irdaCommands[irdaSettingButtonIndex] = irdaRecvCommand;
				
				for (int i = 0; i < 9; i++) {
					Memory.WriteDWORD(i * 4, irdaCommands[i]);
				}
				
				display_data[4] = 0b00011111;
				display_data[5] = digits[0];
				display_data[6] = 0b10101101;
				display_data[7] = 0b10110011;
			}
		}
		
		if (!menuFlag && !irdaSettingsFlag) {
			if (printMode == 0) {
				setInterval(540, tmr0, {
					blinkDividersFlag ^= 1;
					printDividers(blinkDividersFlag);
				});				
				if (SQW_Flag) {
					SQW_Flag = 0;
					RTC.ReadDateTime(&DateTime);
					printTime(DateTime.Hour, DateTime.Min, DateTime.Sec);
				}
			}
			if (printMode == 1) {
				setInterval(540, tmr2, {
					blinkDotsFlag ^= 1;
					RTC.ReadDateTime(&DateTime);
					printDate(DateTime.Day, DateTime.Month, DateTime.Year, 1, 1, 1, blinkDotsFlag);
				});
			}
			if (printMode == 2) {
				setInterval(1000, tmr2, {
					printTemperature(tempC);
				});
			}
		}
		
	}
}