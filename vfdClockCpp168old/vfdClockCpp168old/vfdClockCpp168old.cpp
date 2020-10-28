/*
 * vfdClockCpp168old.cpp
 *
 * Created: 16.10.2020 9:05:02
 *  Author: user
 */ 

// в этом файле все основные подключения
#include "main.h"

// Подключение библиотек
SPI SPITransfer(C3, C0, B5, LSBFIRST);
DS3231 RTC;
TDateTime DateTime;
EEPROM Memory;
DS18B20 TempSensor(C2);
BUTTON menuButtton(D5);
BUTTON upButton(D7);
BUTTON downButton(D6);
BUTTON okButton(B0);


/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~ ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */


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

/* NEC-управление */
#define NEC_REFERENCE_MESSAGE_MIN 108 // Минимальная длина преамбулы
#define NEC_REFERENCE_MESSAGE_MAX 114 // Максимальная длина преамбулы
#define NEC_REPEAT_MESSAGE_MIN 89 // Минимальная длина импульсов повтора
#define NEC_REPEAT_MESSAGE_MAX 94 // Максимальная длина импульсов повтора
#define NEC_CLEAR_BIT_TICK_MIN 9 // Минимальная длительность установки нуля
#define NEC_CLEAR_BIT_TICK_MAX 12 // Максимальная длительность установки нуля
#define NEC_SET_BIT_TICK_MIN 17 // Минимальная длительность установки еденицы
#define NEC_SET_BIT_TICK_MAX 20 // Максимальная длительность установки еденицы
#define NEC_MAX_WAIT_TICKS 1200 // Если в течении этих тиков ничего не пришло - сбрасываем все счетчики и останавливаемся
#define NEC_MIN_HANDLER_TICKS 5 // Минимальный порого обработки пакетов
#define T2_EN TCCR2B |= (1 << CS21) // Макрос запуска таймера 2
#define T2_STOP TCCR2B = 0x00 // Макрос остановки таймера 2

volatile unsigned char NEC_DONE_FLAG = 0; // Флаг завершение обработки команды NEC
volatile unsigned int NEC_SCLK = 0; // Пожалуй самая главная перменная в NEC. Это подсчет тив на 1 импульс
volatile unsigned char NEC_RECV_CNT = 0; // Счетчик приема пакетов
volatile unsigned char NEC_START_FLAG = 0; // Флаг приема стартовой последовательности
volatile unsigned char NEC_REPEAT_FLAG = 0; // Флаг того, что последовательность повторяется
volatile unsigned char NEC_REPEAT_WORK = 0; // Это работа повтора. Подробнее см. в коде прерывания
volatile DWORD command = 0x00000000; // Собственно сама команда
/*
	В NEC-пакете всего 4 байта:
		1. Адрес
		2. Инверсия адреса или его продолжение
		3. Команда
		4. Инверсия команды
*/
volatile unsigned char necAddr1 = 0x00;
volatile unsigned char necAddr2 = 0x00;
volatile unsigned char necCmd1 = 0x00;
volatile unsigned char necCmd2 = 0x00;

// Индексы кодов кнопок управления в массиве
#define POWER_OFF 0
#define MENU 1
#define PLUS 2
#define MINUS 3
#define CLEAR 4
#define PLAY 5
#define FWND 6
#define RWND 7
#define RETURN 8

/* Динамическая индикация */
const int max_groups  = 8; // Минимальное колличество групп
volatile unsigned char display_pos = 0; // Текущая позиция отображения
volatile unsigned char display_data[8]; // Массив индикации

/* Прочие переменные для работы системы */
volatile unsigned char SQW_Flag = 0x00;
unsigned long long int tmr0 = 0;
unsigned long long int tmr1 = 0;
unsigned long long int tmr2 = 0;
unsigned long long int tmr3 = 0;
volatile unsigned long long int millis = 0; // Колличество миллисекунд с момента запуска

/* Полезные макросы */
#define setInterval(n, tmr, code) { if ((millis - tmr) >= n) { tmr = millis; code; }}

/* Раскоментировать для отладки. Сейчас они пока не нужны :)) */
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
// // Отправка строки через UART
// void USORT_String(char *stringPtr){
// 	while(*stringPtr != 0) {
// 		USORT_Transmit(*stringPtr);
// 		stringPtr++;
// 	}
// }

// Динамическая индикация
INTERRUPT (TIM0_OVF_isr) {
	TCNT0 = 180; // Это число я подбирал опытным путем, что-бы не мерцало, не тормозило систему и светилось ярко

	/* Отсылаем текущие данные в сдвиговые регистры */
	display_pos = (display_pos + 1) % max_groups; // Считаем до максимального значения и начинаем заново
	SPITransfer.send(grids[display_pos]);  // Отсылаем данные на решетки
	SPITransfer.send(display_data[display_pos]); // Отсылаем данные на сегменты
	SPITransfer.CSLatch(); // Защелкиваем регистры
}

// Вход в прерывание 2го таймера по переполнению. каждые 100мкс
INTERRUPT (TIM2_OVF_isr) {
	TCNT2 = 0x38; // это мы уже сто раз проходили
	if (++NEC_SCLK >= NEC_MAX_WAIT_TICKS) { // Отсчитываем переменную NEC_SCLK каждые 100мкс
		// А при достижении какого либо порога - все сбрасываем
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

// Вход во внешнее прерывание
INTERRUPT (IRQ1_isr) {
	T2_EN; // Как поймали первый спадающий фронт - запускаем таймер
	if (NEC_SCLK > NEC_MIN_HANDLER_TICKS) { // Все, что меньше какого-то значения - не обрабатываем
		// Стартовый импульс вместе с опорным длятся ~13.2мс
		if (NEC_SCLK >= NEC_REFERENCE_MESSAGE_MIN && NEC_SCLK <= NEC_REFERENCE_MESSAGE_MAX) {
			NEC_START_FLAG = 1; // Принят стартовый заголовок
			NEC_RECV_CNT = 0; // Сбрасываем счетчик приема данных
			/*
				 Вся суть этого флага, что он сбрасывается только при приеме стартового импульса, 
				 что-бы его в основной программе можно было спокойно обработать. А флаг NEC_REPEAT_FLAG
				 нужен лишь для входа в обработчик повторов и не более, он уже сбрасывается в основной программе
			*/
			NEC_REPEAT_WORK = 0; // Сбрасываем флаг работы повтора команды
		}
		// Если поймали правильный стартовый импульс - можем начать дальнейшую обработку
		if (NEC_START_FLAG) {
			// Повторы в NEC дляться ~11мс
			if (NEC_SCLK >= NEC_REPEAT_MESSAGE_MIN && NEC_SCLK <= NEC_REPEAT_MESSAGE_MAX) {
				NEC_REPEAT_FLAG = 1; // Принят повтор
				NEC_REPEAT_WORK = 1; // Идет работа повтора
			}
			/* Знаю, по-идиотски, но умнее лень было придумывать */
			if (NEC_SCLK >= NEC_CLEAR_BIT_TICK_MIN && NEC_SCLK <= NEC_CLEAR_BIT_TICK_MAX) { // Длительность лог. 0 - ~1.1мс
				NEC_RECV_CNT++; // Прибавляем счетчик принятых данных
				/* тут просто все распихиваем по переменным */
				if (NEC_RECV_CNT >= 0 && NEC_RECV_CNT < 9) bitClear(necAddr1, NEC_RECV_CNT);
				if (NEC_RECV_CNT >= 9 && NEC_RECV_CNT < 17) bitClear(necAddr2, (NEC_RECV_CNT - 9));
				if (NEC_RECV_CNT >= 17 && NEC_RECV_CNT < 25) bitClear(necCmd1, (NEC_RECV_CNT - 17));
				if (NEC_RECV_CNT >= 25 && NEC_RECV_CNT < 33) bitClear(necCmd2, (NEC_RECV_CNT - 25));
			}
		
			if (NEC_SCLK >= NEC_SET_BIT_TICK_MIN && NEC_SCLK <= NEC_SET_BIT_TICK_MAX) { // Длительность лог. 1 - ~2.2мс
				/* Тут все то-же самое, что и с нулями */
				NEC_RECV_CNT++;
				if (NEC_RECV_CNT >= 0 && NEC_RECV_CNT < 9) bitSet(necAddr1, NEC_RECV_CNT);
				if (NEC_RECV_CNT >= 9 && NEC_RECV_CNT < 17) bitSet(necAddr2, (NEC_RECV_CNT - 9));
				if (NEC_RECV_CNT >= 17 && NEC_RECV_CNT < 25) bitSet(necCmd1, (NEC_RECV_CNT - 17));
				if (NEC_RECV_CNT >= 25 && NEC_RECV_CNT < 33) bitSet(necCmd2, (NEC_RECV_CNT - 25));
			}
			// Как только чтение 32х бит завершено
			if (NEC_RECV_CNT == 32) {
				// Проверяем целостность принятых данных
				// - В первой версии протокола оба адреса и команды идут как прмые, так и инверсные
				// - Во второй версии адрес занимает два байта и не имеет инверсии
				// Собсно сложив прямые с нверсными байты и получив на выходе FFh - мы понимаем, что все целехонькое
				if ((((necAddr1 + necAddr2) == 0xFF) && ((necCmd1 + necCmd2) == 0xff)) || ((necCmd1 + necCmd2) == 0xFF)) {
					NEC_DONE_FLAG = 1;
					NEC_RECV_CNT = 0;
					bytesJoin(command, necAddr1, necAddr2, necCmd1, necCmd2);// Склеиваем все в одну команду
				}
				T2_STOP; // Останавливаем подсчет
			}
		}
	}
	NEC_SCLK = 0; // Обнуляем текущий счетчик
}

// Функция отображения температуры. Поддержка отрицательных значений. В общем 
// говорить тут особо нечего - тут куча математических вычислений
void printTemperature(float temp) {
	// Если число отрицательное - просто убираем минус и работаем как с обычным числом )
	if (temp < 0) {
		temp = fabs(temp);
		display_data[1] = divider_horizontal; // Но пририсуем минус во втором разряде
	} else {
		display_data[1] = 0x00; // Если положительное число - очищаем второй разряд
	}
	
	// Очищаем все остальное
	display_data[0] = 0x00;
	display_data[6] = 0x00;
	display_data[7] = 0x00;
	
	if (temp == 0.0F) { // Если температрура 0°, то...
		display_data[2] = digits[0] | (1 << 6); // Отображаем 0. в третьем разряде
		display_data[3] = digits[0]; // Ноль в четвертом разряде
		display_data[4] = cel_symbol; // Значек ° в пятом разряде
		display_data[5] = 0x00; // Очищаем шестой разряд, если там что-то было до этого
	} else if (temp >= 0.1F && temp <= 9.9F) { // Если температура больше 0.1° и меньше 9.9°, то...
		display_data[2] = digits[(unsigned char)temp] | (1 << 6); // Округляем значение до ближайшего и ставим точку после него
		display_data[3] = digits[(unsigned char)(temp * 10.0F) % 10]; // Умножаем число на 10 и остаток от деления на 10 - будет второй разряд 
		display_data[4] = cel_symbol; // Дальше значек цельсия
		display_data[5] = 0x00; // Если тут что-то было до этого, то удаляем
	} else if (temp >= 10.0F && temp <= 99.9F) { // Если температура больше или равна 10.0° и меньше или равна 99.9°, то...
		display_data[2] = digits[(unsigned char)(temp) % 100 / 10]; // Первый разряд это - остаток от деления на 100, разделенный на 10
		display_data[3] = digits[(unsigned char)(temp) % 10] | (1 << 6); // Второй разряд это остаток от деления на 10. так-же ставим точку
		display_data[4] = digits[(unsigned char)(temp * 10.0F) % 10]; // Третий разряд - это остаток от деления на десять, от числа умноженного на десять
		display_data[5] = cel_symbol; // Отображаем занчек цельсия
	}
}

/*
	 Функция отображения даты
		День, месяц, год - это основные аргументы
		dVisible, mVisible, yVisible - Параметры отображения дня, месяца и года. 1 - отображается, 0 - не отображается
		dt - параметр отображения точки-разделителя между днем, месяцем и годом
*/
void printDate(int day, int month, int year, int dVisible=1, int mVisible=1, int yVisible=1, unsigned char dt=1) {
	if (dVisible) { // Если мы хотим отображать дни, то ..
		if (day == 0) { // Если день равен нулю (такое конечно не возможно, но я был-бы не я - если-бы фунция не умела полноценно числа отображать)
			display_data[0] = digits[0]; // В первый разряд помещаем ноль
			display_data[1] = (dt) ? digits[0] | (1 << 6) : digits[0]; // И сюда ноль. При надобностьи отображаем точку-разделитель
		} else if (day >= 1 && day <= 9) { // Дни от 1 до 9
			display_data[0] = digits[0]; // Отображаем ведущий ноль
			display_data[1] = (dt) ? digits[day] | (1 << 6) : digits[day]; // Отображаем день
		} else if (day >= 10 && day <= 99) { // Дни от 10 до 99
			display_data[0] = digits[(unsigned char)((day % 100) / 10)]; // Известной нам формулой получаем первые разряд
			display_data[1] = (dt) ? digits[(unsigned char)(day % 10)] | (1 << 6) : digits[(unsigned char)(day % 10)]; // и второй. При надобности показываем точку-разделитель
		}
	} else {
		// Если не отображаем - то не отобраем. Оставляем только точку-разделитель
		display_data[0] = 0x00;
		display_data[1] = 0b01000000;
	}
	/*
		С месяцем и годом - все тоже самое, мне лень все это расписывать )))))
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
	Функция отображение времени на дисплее
	hour, minutes, seconds - Часы, минуты и секунды
	hVisible, mVisible, sVisible - отображение. 0 - Не отображать, 1 - Отображать
	
	Функция почти идентична функции printDate, тут расписывать ничего не буду.
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

// Функция отображения черточек-разделителей
void printDividers (char print) {
	if (print) {
		display_data[2] = divider_horizontal;
		display_data[5] = divider_horizontal;
	} else {
		display_data[2] = 0x00;
		display_data[5] = 0x00;
	}
}

// Вход в нулевое прерывание. Тут SQW_Flag каждую секундц устанавливается в 1. Дальше поймете - зачем
INTERRUPT (IRQ0_isr) {
	SQW_Flag = 1;
}

// Вход в прерывание первого даймера. Тупо считаем миллисекунды с момента запуска системы
INTERRUPT (TIM1_OVF_isr) {
	TCNT1 = 0xFF06;
	millis++;	
}

// Фунция нахождения дней в месяце с определением високосного года. Нужна для настроек даты/времени
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

// Функция для очистки дисплея
void fillDisplay(unsigned char character) {
	for (unsigned char i = 0; i < 8; i++) display_data[i] = character;
}

// Функция для очистки дисплея с парметрами позиции начала очистки и конца
void fillDisplay(unsigned char character, unsigned char pos, unsigned char length) {
	unsigned char i = 0;
	for (i = pos; i <= length; i++) {
		display_data[i] = character;
	}
}

// Эффект переключения на следующий экран
void EffectNextDisplay(void) {
	fillDisplay(0x00);
	for (int i = 0; i < 8; i++) {
		display_data[i] = divider_horizontal;
		_delay_ms(40);
	}
}

// Эффект переключения на предыдущий экран
void EffectPrevDisplay(void) {
	fillDisplay(0x00);
	for (int i = 8; i > 0; i--) {
		display_data[i] = divider_horizontal;
		_delay_ms(40);
	}
}

// Функция отображения числа на дисплее
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
	cli(); // Запрещаем прерывания
	
	//USORT_Init(8); 
	RTC.SetSQWOutState(_1HZ);

	// Инициализация таймера/счестчика 0 для динамической индикации
	TCCR0B |= (1 << CS02);
	TIMSK0 |= (1 << TOIE0);
	
	T2_STOP; // Остановка таймера 2
	TIMSK2 |= (1 << TOIE2); // Настраиваем таймер 2 на переполнение
	
	// Таймер 1 для подсчета миллисекунда с момента запуска. Нужно для задержек без фризов
	TCCR1B |= (0 << CS12) | (1 << CS11) | (1 << CS10);
	TIMSK1 |= (1 << TOIE1);
	TCNT1 = 0xFF06;
	
	// Инициализация внешних прерывааний
	EICRA |= (1 << ISC11) | (0 << ISC10); // Прерывание 1 на спад фронта для обработки NEC
	EICRA |= (0 << ISC01) | (1 << ISC00); // Прерывание 0 на любое изменение фронтов для обработки SQW с DS3231
	EIMSK |= (1 << INT1) | (1 << INT0); // Разрешение внешних прерываний
	
	sei(); // Глобально разрешаем прерывания
	
	char buttonFlag = 0; // Флаг нажатия на кнопку (нужен для входа в обработчик)
	char buttonPressFlag = 0; // Флаг нажатия и удержания кнопки (нужен для входа в обработчик)
	char menuFlag = 0; // Флаг меню
	char enterMenuFlag = 0; // Флаг входа в меню (нужен для задания текущих значений для настройки)
	char irdaSettingsFlag = 0; // Флаг настроек ИК-пульта
	char enterIrdaSettingsFlag = 0; // Флаг входа в настройки ИК-пульта
	char menuItemBlinkFlag = 0; // флаг мигания настраиваемого пункта в меню
	char blinkDividersFlag = 0; // Флаг мигания разделителей во времени
	char blinkDotsFlag = 0; // Флаг мигания точек-разделителей в дате
	//char tempConversionEndFlag = 0; // Флаг окончания преобразования температуры (сейчас не нужен, ибо температура пишется прямо в обработчике)
	char irdaRecvCommandFlag = 0; // Флаг приема команды с пульта (для настроек пульта)
	char irdaSaveCommandFlag = 0; // Флаг окончания записи команды в EEPROM
	
	int setHour = 0, setMinutes = 0, setSeconds = 0; // Настраиваемые значения (время)
	int setDay = 0, setMonth = 0, setYear = 0; // Настраиваемые значения (дата)
	int maxDaysPerMonth = 0; // Дней в месяце
	int printMode = 0; // Отображение на основном дисплее (0 - Часы, 1 - Дата, 2 - Температура)
	int tempRequetCNT = 0; // Счетчик опроса датчика
	int irdaSettingButtonIndex = 0; // Текущая, настраиваемая кнопка ИК-пульта
	int irdaSettingButtonIndex_old = 0; // Предыдущая, настраиваемая кнопка на ИК-пульте
	int irdaSettingAnimationReadyPos = 0; // Позиция бегущей полоски (анимация при настройке ИК-пульта)
	int menuLevel = 0; // Текущая позиция в меню
	int oldMenuLevel = 0; // Предыдущая позиция в меню
	
	DWORD irdaRecvCommand = 0x00000000; // Принятая команда с пульта для настроек ИК-пульта
	DWORD irdaCommands[9]; // Массив кодов команд с ИК-пульта
	
	float tempC = 0.0F; // Температура на основном дисплее
	
	// Читаем все значения кодов кнопок ИК-пульта
	for (unsigned char i = 0; i < 9; i++) {
		// Умножаем на 4 - ибо DWORD состоит из 4х байт
		irdaCommands[i] = Memory.ReadDWORD(i*4);
	}

	while (1) {
		
		/*
			Опрос датчика температуры DS18b20
			0 - Ничего не делаем. 
			1 - Запрос на преобразование. 
			2 - Пропуск. 
			3 - Можно читать температуру.
		*/
		setInterval(1000, tmr1, {
			if (tempRequetCNT == 1) TempSensor.beginConversion(); // Запрос на преобразование
			//if (tempRequetCNT == 2) tempConversionEndFlag = 1; // Пропуск
			if (tempRequetCNT == 3) { // Читаем температуру
				tempRequetCNT = 0; 
				//tempConversionEndFlag = 0;
				tempC = TempSensor.readTemp();
			}
			tempRequetCNT++; // Тут прибавлем счетчик опроса
		});		
		
		// При долгом нажатии на кнопку OK
		if (okButton.downLong(20000)) {
			irdaSettingsFlag ^= 1; // Переключаем состояния флага настроек ИК-пульта. т.е. или входим в настройки или выходим из них
			enterIrdaSettingsFlag = irdaSettingsFlag; // Если мы в настройках ИК-пульта - нужно этот флаг приравнять к еденице, что-бы проинициализировать начальные значения
		}
		
		// То-же самое с меню
		if (menuButtton.downLong(20000)) {
			menuFlag ^= 1;
			enterMenuFlag = menuFlag;
		}
		
		// При коротком нажании на кнопку MENU
		if (menuButtton.down()) {
			buttonFlag = 1;
			// Если мы в меню - то переключаем пункты меню, если не в меню - эта кнопка очищаем принятую команду в настройках ИК-пульта
			command = (menuFlag) ? irdaCommands[FWND] : irdaCommands[CLEAR];
		}
		
		// Кнопка "Вниз"
		if (downButton.down()) {
			buttonFlag = 1;
			// Если мы в меню - то прибавляем значения, если же нет, то переключаем отображаемые экраны
			command = (menuFlag) ? irdaCommands[PLUS] : irdaCommands[FWND];
			
			// Если мы настраиваем кнопки ИК-пульта
			if (irdaSettingsFlag) {
				irdaSettingButtonIndex++; // Прибавляем настраиваемую кнопку
				if (irdaSettingButtonIndex > 8) irdaSettingButtonIndex = 0; // Если вывалились за пределы - идем сначала
			}
		}
		
		// Кнопка "Вверх"
		if (upButton.down()) {
			buttonFlag = 1;
			// Если мы в меню - то убавляем значения, если же нет, то переключаем отображаемые экраны
			command = (menuFlag) ? irdaCommands[MINUS] : irdaCommands[RWND];
			
			// Если мы настраиваем кнопки ИК-пульта
			if (irdaSettingsFlag) {
				irdaSettingButtonIndex--; // убавляем настраиваемую кнопку
				if (irdaSettingButtonIndex < 0) irdaSettingButtonIndex = 8; // Если вывалились за пределы - идем с конца
			}
		}
		
		// Долгое нажатие на кнопку "Вверх"
		if (upButton.press(900)) {
			buttonPressFlag = 1; // Прибавляем настраиваемую кнопку
			command = (menuFlag) ? irdaCommands[MINUS] : 0x00;
		}
		
		// Долгое нажатие на кнопку "Вниз"
		if (downButton.press(900)) {
			buttonPressFlag = 1;
			command = (menuFlag) ? irdaCommands[PLUS] : 0x00;
		}
		
		// При отпускании кнопок вверх или вниз - гвоорим системе, что кнопки отпущены
		if (upButton.up() || downButton.up()) {
			buttonPressFlag = 0;
		}
		
		// Кнопка ОК
		if (okButton.down()) {
			buttonFlag = 1;
			command = (menuFlag) ? irdaCommands[PLAY] : 0x00;
		}
		
		// Если пришла команда с пульта при настройках ИК-пульта
		if (NEC_DONE_FLAG && irdaSettingsFlag && !irdaSaveCommandFlag) {
			NEC_DONE_FLAG = 0; // Уже ничего
			irdaRecvCommand = command; // Записваем во временную переменную
			irdaRecvCommandFlag = 1; // Говорим системе, что команда поступила
		}
		
		/*
			Обработчки команд
				- ИК-пульта
				- Повтора команды
				- Нажатия на кнопки
				- Удержание кнопки
		*/
		if (NEC_DONE_FLAG || NEC_REPEAT_FLAG || buttonFlag || buttonPressFlag) {
			// Обнуляем все входящие флаги
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