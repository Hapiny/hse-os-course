/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <kern/kclock.h>

void
rtc_init(void)
{
	nmi_disable();
	
	// Для управления часами используются три байта в памяти CMOS, называемые регистрами A, B и C. 
	// Для работы с этими регистрами используются порты ввода-вывода 0x70 и 0x71 
	// (в JOS для работы с ними используются константы IO_RTC_CMND и IO_RTC_DATA).
	outb(IO_RTC_CMND, RTC_BREG);            // 1. Переключение на регистр часов B.
	uint8_t B_REG = inb(IO_RTC_DATA);       // 2. Чтение значения регистра B из порта ввода-вывода.
	B_REG = B_REG | RTC_PIE;                // 3. Установка бита RTC_PIE.
	outb(IO_RTC_CMND, RTC_BREG);            // 4. Запись обновленного значения регистра в порт ввода-вывода.
	outb(IO_RTC_DATA, B_REG);


	// меняем частоту часов, чтобы прерывания приходили раз в 0.5 сек
	outb(IO_RTC_CMND, RTC_AREG);
	uint8_t A_REG = inb(IO_RTC_DATA);
	A_REG = A_REG | 0x0F;  // в файле docs/rtc.pdf на 14 странице есть таблица (в нижней строке то, что нужно)
	outb(IO_RTC_CMND, RTC_AREG);
	outb(IO_RTC_DATA, A_REG);

	nmi_enable();
}

uint8_t
rtc_check_status(void)
{
	uint8_t status = 0;
	
	outb(IO_RTC_CMND, RTC_CREG);
	status = inb(IO_RTC_DATA);

	return status;
}

