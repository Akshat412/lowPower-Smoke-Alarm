  
/* AVR Low Power Smoke Alarm
 * Senses smoke values, activates buzzer for warning
 * Created: 20-06-2021 22:00:03
 * Author : Akshat
 */ 

#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdlib.h>

#define SENSOR 0
#define LED 1

unsigned int digitalVal = 0; // 10-bit digital value of the MQ2 sensor
char buffer[6]; // buffer to send to UART

// sleep mode functions
void powerDown_setup();
void powerDown_watchDog();

// ADC functions
void ADC_init();
int ADC_read();

// UART functions
void UART_init();
void UART_send(char letter);
void UART_writeString(char *stringAdd);

// misc functions
void init_configure();
void alarmTimer();

int main(void) {
	DDRB |= (1 << LED);
	DDRD = (1 << 0);
	PORTD = (1 << 0);
	
	UART_init(); // initialize UART
	ADC_init(); // initialize ADC
	
	init_configure(); // make sure actual sensor readings lead to warnings
	powerDown_setup(); // initialize powerDown
	sei(); // enable global interrupts
	
	// device is to be deployed in clean air
	// stay in permanent loop till the sensor reaches clean air levels (<400)
	
    while (1) {
		// main-loop: check ADC level every few minutes, go to sleep
		digitalVal = ADC_read(); // read from ADC PC0
		itoa(digitalVal, buffer, 10);
		UART_writeString(buffer);
		UART_send(13); // carriage return
		UART_send(10); // new line
		
		_delay_ms(10); // mild delay for UART to work
		
		powerDown_watchDog(); // go to sleep
		
		if(ADC_read() > 600) {
			// permanently go into alarm mode
			while(1) {
				for(int i = 0; i < 350; i++) {
					alarmTimer();
				}
				PORTD &= ~(1 << 0);
				_delay_ms(100);	
			}
		}
    }
}

void init_configure() {
	// initial configuration to avoid a false alarm 
	while(ADC_read() > 400) {
		PORTB |= (1 << LED);
		digitalVal = ADC_read(); // read from ADC PC0
		itoa(digitalVal, buffer, 10);
		UART_writeString(buffer);
		UART_send(13); // carriage return
		UART_send(10); // new line
		
		_delay_ms(100); // mild delay for UART to work
	}
	PORTB &= ~(1 << LED);
}

void alarmTimer() {
	TCNT0 = 131;
	TCCR0A = 0x00;
	TCCR0B = 0x03;
	while((TIFR0 & (1 << TOV0)) == 0);
	TCCR0B = 0;
	TIFR0 = (1 << TOV0);
	PORTD ^= (1 << 0);
}

void powerDown_setup() {
	// setup for sleep mode
	SMCR |= (1 << SM1); // power-down mode
	SMCR |= (1 << SE); // enable sleep
	
	WDTCSR = (1 << WDE) | (1 << WDCE); // set watchdog enable (WDE) and watchdog change enable (WDCE)
	WDTCSR = (1 << WDP3); // prescaler, set for 4s counter, get rid of WDE and WDCE
	WDTCSR |= (1 << WDIE); // set interrupt enable
}

void powerDown_watchDog() {
	// 2 minutes of sleep time, 60 = 4 * 15;
	for(unsigned char i = 0; i < 15; i++) {
		__asm__  __volatile__("sleep"); //in line assembler to go to sleep
	}
}

ISR(WDT_vect) {
	// watchdog interrupt
}

void ADC_init() {
	// Initializes ADC for PD0
	DDRC &= ~(1 << SENSOR); // set PC0 as an input
	
	ADCSRA = (1 << ADEN); // enable ADC module
	ADCSRA |= (1 << ADPS0) | (1 << ADPS1) | (1 << ADPS2); // set speed to 125 kHz
	ADMUX = 0x40; // set VREF = 5V, select ADC0 as input
}

int ADC_read() {
	// Reads ADC value for PD0
	ADCSRA |= (1<<ADSC); // start ADC conversion
	while((ADCSRA & (1<<ADIF)) == 0); // wait for conversion to end
	
	unsigned int digWord = ADCL| (ADCH<<8); // read ADCL then ADCH
	return digWord; // return analog value
}

void UART_init() {
	// Initializes UART
	UCSR0B = (1<<TXEN0); // enable UART transmission
	UCSR0C = (1<<UCSZ01) | (1<<UCSZ00); // asynchronous mode, 8-bit data, 1 stop bit, no parity
	UBRR0L = 103; // 9600 baud rate at 16MHz clock
}

void UART_send(char letter) {
	// Sends a single character over UART
	while((UCSR0A & (1<<UDRE0)) == 0); // wait till TX buffer is empty
	UDR0 = letter; // send out letter
}

void UART_writeString(char *stringAdd) {
	// Sends a string over UART
	unsigned char i;
	for (i = 0; i < strlen(stringAdd); i++) {
		UART_send(stringAdd[i]);
	}
}
