/* * Laptop_Cooling_Pad_Control_System.c * * Created: 28-10-2026 * Author : alok9coder */

#define F_CPU 16000000UL // 16 MHz Clock Speed
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>
#include "u8g2.h"

#define NUM_READINGS 20
#define OLED_ADDRESS 0x78 // 0x3C shifted left by 1 for write operation

// ==================== SYSTEM HARDWARE ROUTING LABELS ====================
#define LED_DDR              DDRD
#define LED_PORT             PORTD
#define LED_PIN_NUM          PD2          // Soldered Status Activity LED

#define POT_ADC_CHANNEL      0            // Throttle Potentiometer on PA0 (ADC0)
#define TEMP_ADC_CHANNEL     2            // LM35 Temp Sensor on PA2 (ADC2)

#define HS_FAN_DDR           DDRD
#define HS_FAN_PORT          PORTD
#define HS_FAN_PIN_NUM       PD5          // Native Hardware PWM Pin

#define BODY_FAN_DDR         DDRC
#define BODY_FAN_PORT        PORTC
#define BODY_FAN_PIN_NUM     PC3          // Drives parallel BC547 transistors

#define TOGGLE_SW_DDR        DDRB
#define TOGGLE_SW_PIN        PINB
#define TOGGLE_SW_PIN_NUM    PB0          // Driven directly HIGH or LOW by switch
// ========================================================================

void init_PWM(void);
void init_ADC(void);
uint16_t read_ADC(uint8_t channel);
void init_GPIO(void);
void init_TWI(void);
uint8_t u8x8_byte_avr_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
uint8_t u8x8_gpio_and_delay_avr(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

u8g2_t u8g2;

static uint16_t lm35Readings[NUM_READINGS];
static uint32_t lm35Total = 0;
static uint8_t readIndex = 0;

int main(void)
{
	init_GPIO();
	init_TWI();
	init_PWM();
	init_ADC();

	u8g2_Setup_sh1106_i2c_128x64_noname_1(&u8g2, U8G2_R0, u8x8_byte_avr_hw_i2c, u8x8_gpio_and_delay_avr);
	u8g2_InitDisplay(&u8g2);
	u8g2_SetPowerSave(&u8g2, 0);
	u8g2_ClearBuffer(&u8g2);

	// Seed Moving Average Array
	for(uint8_t i = 0; i < NUM_READINGS; i++) {
		lm35Readings[i] = read_ADC(TEMP_ADC_CHANNEL);
		lm35Total += lm35Readings[i];
		_delay_ms(2);
	}
	
	char tempBuffer[8];
	char hsFanBuffer[8];
	char throttleBuffer[8];
	uint8_t isBodyFanOn = 0;

	while (1)
	{
		// 1. Smooth out the LM35 Sensor
		lm35Total -= lm35Readings[readIndex];
		lm35Readings[readIndex] = read_ADC(TEMP_ADC_CHANNEL);
		lm35Total += lm35Readings[readIndex];
		readIndex++;
		if (readIndex >= NUM_READINGS) {
			readIndex = 0;
		}
		uint16_t avgLM35 = lm35Total / NUM_READINGS;
		float tempC = ((float)avgLM35 * 500.0) / 1023.0;

		// 2. Read Manual Throttle Potentiometer Value (With +511 Math Rounding Correction)
		uint16_t potValue = read_ADC(POT_ADC_CHANNEL);
		uint32_t manualThrottleDuty = (((uint32_t)potValue * 320) + 511) / 1023;

		// 3. Switch logic handling hard-driven HIGH or LOW states
		if (TOGGLE_SW_PIN & (1 << TOGGLE_SW_PIN_NUM)) {
			BODY_FAN_PORT |= (1 << BODY_FAN_PIN_NUM);
			isBodyFanOn = 1;
			} else {
			BODY_FAN_PORT &= ~(1 << BODY_FAN_PIN_NUM);
			isBodyFanOn = 0;
		}

		// 4. Pure Temperature Speed Calculation (With Float-to-Int +0.5 Rounding Correction)
		uint32_t thermalCalculatedDuty = 0;
		if (tempC < 30.0) {
			thermalCalculatedDuty = 0;
			} else if (tempC >= 40.0) {
			thermalCalculatedDuty = 320;
			} else {
			thermalCalculatedDuty = (uint32_t)((((tempC - 30.0) * 320.0) / 10.0) + 0.5);
		}

		// 5. MAX VALUE OVERRIDE LOGIC SELECTOR
		uint32_t finalPwmDuty = manualThrottleDuty;
		if (thermalCalculatedDuty > finalPwmDuty) {
			finalPwmDuty = thermalCalculatedDuty;
		}

		// Safety bounds enforcement check
		if (finalPwmDuty > 320) finalPwmDuty = 320;

		OCR1A = finalPwmDuty;
		
		// Rounded percentage calculation for clear OLED representation
		int fanPercent = ((finalPwmDuty * 100) + 160) / 320;

		dtostrf(tempC, 4, 1, tempBuffer);
		sprintf(hsFanBuffer, "%d%%", fanPercent);
		sprintf(throttleBuffer, "%d%%", (int)(((manualThrottleDuty * 100) + 160) / 320));

		// 6. Graphics Engine Update Cycle (Optimized Low-SRAM Page Buffer Loop)
		u8g2_FirstPage(&u8g2);
		do {
			u8g2_SetFont(&u8g2, u8g2_font_courB10_tr);

			// --- Row 1: Temp Display Layout ---
			u8g2_DrawStr(&u8g2, 0, 14, "Temp:");
			u8g2_DrawStr(&u8g2, 127 - u8g2_GetStrWidth(&u8g2, "'C") - u8g2_GetStrWidth(&u8g2, tempBuffer), 14, tempBuffer);
			u8g2_DrawUTF8(&u8g2, 127 - u8g2_GetStrWidth(&u8g2, "'C"), 14, "'C");

			// --- Row 2: Body Fan State Layout ---
			u8g2_DrawStr(&u8g2, 0, 30, "Body Fan:");
			if (isBodyFanOn) {
				u8g2_DrawStr(&u8g2, 127 - u8g2_GetStrWidth(&u8g2, "ON"), 30, "ON");
				} else {
				u8g2_DrawStr(&u8g2, 127 - u8g2_GetStrWidth(&u8g2, "OFF"), 30, "OFF");
			}

			// --- Row 3: Turbo Fan Profile Layout ---
			u8g2_DrawStr(&u8g2, 0, 46, "Turbo Fan:");
			u8g2_DrawStr(&u8g2, 127 - u8g2_GetStrWidth(&u8g2, hsFanBuffer), 46, hsFanBuffer);

			// --- Row 4: Input Throttle Base Layout ---
			u8g2_DrawStr(&u8g2, 0, 62, "Throttle:");
			u8g2_DrawStr(&u8g2, 127 - u8g2_GetStrWidth(&u8g2, throttleBuffer), 62, throttleBuffer);
			
		} while (u8g2_NextPage(&u8g2));

		_delay_ms(10);
	}
}

void init_GPIO(void) {
	TOGGLE_SW_DDR &= ~(1 << TOGGLE_SW_PIN_NUM);
	PORTB &= ~(1 << TOGGLE_SW_PIN_NUM);
	
	LED_DDR |= (1 << LED_PIN_NUM);
	LED_PORT &= ~(1 << LED_PIN_NUM);
	
	HS_FAN_DDR |= (1 << HS_FAN_PIN_NUM);
	HS_FAN_PORT &= ~(1 << HS_FAN_PIN_NUM);
	
	BODY_FAN_DDR |= (1 << BODY_FAN_PIN_NUM);
	BODY_FAN_PORT &= ~(1 << BODY_FAN_PIN_NUM);
}

void init_PWM(void) {
	TCCR1A = 0;
	TCCR1B = 0;
	TCNT1 = 0;
	TCCR1A |= (1 << COM1A1);
	TCCR1B |= (1 << WGM13) | (1 << CS10);
	ICR1 = 320;
	OCR1A = 0;
}

void init_ADC(void) {
	ADMUX = (1 << REFS0);
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

uint16_t read_ADC(uint8_t channel) {
	ADMUX = (ADMUX & 0xE0) | (channel & 0x07);
	_delay_us(10);
	ADCSRA |= (1 << ADSC);
	while (ADCSRA & (1 << ADSC));
	return ADC;
}

void init_TWI(void) {
	TWSR = 0x00;
	TWBR = 12;
	TWCR = (1 << TWEN);
}

uint8_t u8x8_byte_avr_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
	uint8_t *data;
	static uint16_t i2cByteCounter = 0;
	
	switch(msg) {
		case U8X8_MSG_BYTE_SEND:
		data = (uint8_t *)arg_ptr;
		while(arg_int > 0) {
			TWDR = *data;
			TWCR = (1 << TWINT) | (1 << TWEN);
			
			i2cByteCounter++;
			if (i2cByteCounter >= 500) {
				LED_PORT ^= (1 << LED_PIN_NUM);
				i2cByteCounter = 0;
			}
			
			while (!(TWCR & (1 << TWINT)));
			data++;
			arg_int--;
		}
		break;
		case U8X8_MSG_BYTE_START_TRANSFER:
		TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
		while (!(TWCR & (1 << TWINT)));
		TWDR = OLED_ADDRESS;
		TWCR = (1 << TWINT) | (1 << TWEN);
		while (!(TWCR & (1 << TWINT)));
		break;
		case U8X8_MSG_BYTE_END_TRANSFER:
		TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
		break;
	}
	return 1;
}

uint8_t u8x8_gpio_and_delay_avr(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
	switch(msg) {
		case U8X8_MSG_DELAY_MILLI:
		while(arg_int > 0) {
			_delay_ms(1);
			arg_int--;
		}
		break;
	}
	return 1;
}
