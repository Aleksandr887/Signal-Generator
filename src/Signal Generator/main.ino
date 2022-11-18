#include <LiquidCrystal.h>
#define RS 8
#define EN 9
#define D4 10
#define D5 11
#define D6 12
#define D7 13
#define LCD_ROWS 16
#define LCD_COLS 2

// DISPLAY SETTINGS
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

// OFF=0 ON=1
uint8_t status = 0;
// PWM=0 SAW=1
uint8_t mode = 0;
// INTERRUPT COUNTER FOR PWM
uint8_t PWM_counter = 0;
// INTERRUPT COUNTER FOR SAW
uint8_t SAW_counter = 0;
// DUTY FOR PWM MODE
uint8_t duty = 255;
// SIGNAL FREQUENCY
uint16_t freq = 1;

// CONVERT STATUS TO STRING FOR PRINT TO DISPLAY
String status_to_str(uint8_t st)
{
    if (st) {
        return "on";
    } else {
        return "off";
    }
}

// CONVERT MODE TO STRING FOR PRINT TO DISPLAY
String mode_to_str(uint8_t md)
{
    if (md) {
        return "SAW";
    } else {
        return "PWM";
    }
}

// START TIMER FOR PWM WITH PRESCALE=1 WITH COMPARE MODE
//
void PWM_Timer_start(uint16_t value)
{
    // freq [1;62500]
    freq = value;
    if (freq > 20000) {
        freq = 100;
    }
    TCCR1A = 0;
    TCCR1B = 0;
    OCR1A = 16000000 / (256 * freq);
    TCCR1B = (1 << WGM12) | (1 << CS10);
    TIMSK1 |= (1 << OCIE1A);
}

// SET PWM FREQ
void PWM_set_freq(uint16_t value)
{
    freq = value;
    Serial.print("freq = " + String(freq));
    Serial.print('\n');
    OCR1A = 16000000 / (256 * freq);
}

// START TIMER FOR SAW WITH PRESCALE=8 WITH COMPARE MODE
void SAW_Timer_start(uint16_t value)
{
    // freq [1;62500]
    freq = value;
    if (freq > 20000) {
        freq = 100;
    }
    TCCR1A = 0;
    TCCR1B = 0;
    OCR1A = 16000000 / (32 * freq * 8);
    TCCR1B = (1 << WGM12) | (1 << CS11);
    TIMSK1 |= (1 << OCIE1A);
}

// SET FREQ FOR SAW
void SAW_set_freq(uint16_t value)
{
    freq = value;
    Serial.print("freq = " + String(freq));
    Serial.print('\n');
    OCR1A = 16000000 / (32 * freq * 8);
}

// STOP TIMER AND RESET COUNTERS
void shutdown()
{
    // TCCR1A(B) - Сбрасывает все регистры таймера
    TCCR1A = 0;
    TCCR1B = 0;
    PWM_counter = 0;
    SAW_counter = 0;
}

// INIT PORTS FOR BUTTONS AND DAC
void PORTS_init()
{
    DDRC = 0b00000000;
    DDRD |= 0b11111000;
}

// PRINT INFO TO DISPLAY
// MODE PWM/SAW OFF/ON
// F = FREQ (FOR SAW MODE)
// F = FREQ D = DUTY (FOR PWM MODE)
void print_info()
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MODE = " + mode_to_str(mode) + " " + status_to_str(status) + "  ");

    if (mode) {
        lcd.setCursor(0, 1);
        lcd.print("F=" + String(freq) + "               ");
    } else {
        lcd.setCursor(0, 1);
        lcd.print("F=" + String(freq) + " d=" + String(duty) + "     ");
    }
}

// SWITCH MODE PWM/SAW
void switch_mode()
{
    mode = !mode;
    lcd.setCursor(0, 0);
    lcd.print("MODE = " + mode_to_str(mode) + " " + status_to_str(status) + "  ");
    if (mode) {
        SAW_counter = 0;
        Serial.print("mode = " + String(mode));
        Serial.print('\n');
    } else {
        PWM_counter = 0;
        Serial.print("mode = " + String(mode));
        Serial.print('\n');
    }
    print_info();
}

void setup()
{
    Serial.begin(9600);
    PORTS_init();
    lcd.begin(LCD_ROWS, LCD_COLS);
    print_info();
}

// STATUS OF BUTTONS
uint8_t value_16 = 0;
uint8_t value_17 = 0;
uint8_t value_18 = 0;
uint8_t value_19 = 0;

void loop()
{
    // SWITCH MODE PWM->SAW, SAW->PWM
    if (digitalRead(16) & !value_16) {
        value_16 = 1;
        switch_mode();
    } else if (!digitalRead(16)) {
        value_16 = 0;
    }

    // INCREASE FREQ
    if (digitalRead(17) & !value_17) {
        value_17 = 1;

        if (mode == 0) {
            PWM_set_freq(freq + 10);
        } else {
            SAW_set_freq(freq + 10);
        }
        print_info();
    } else if (!digitalRead(17)) {
        value_17 = 0;
    }

    // INCREASE DUTY FOR PWM
    if (digitalRead(18) & !value_18) {
        value_18 = 1;

        if (mode == 0) {
            duty += 10;
            print_info();
            Serial.print("duty = " + String(duty));
            Serial.print('\n');
        }
    } else if (!digitalRead(18)) {
        value_18 = 0;
    }

    // ON/OFF
    if (digitalRead(19) & !value_19) {
        value_19 = 1;
        status = !status;
        print_info();

        if (status) {
            if (!mode) {
                PWM_Timer_start(freq);
            } else {
                SAW_Timer_start(freq);
            }
        } else {
            shutdown();
            PORTD &= 0b00000000;
        }

        Serial.print("status = " + String(status));
        Serial.print('\n');
    } else if (!digitalRead(19)) {
        value_19 = 0;
    }
}

// TIMER INTERRUPT
ISR(TIMER1_COMPA_vect)
{
    if (mode == 0) {
        // PWM mode
        if (PWM_counter == 0) {
            PORTD |= 0b11111000;
        }

        if ((PWM_counter == duty) && (duty < 255)) {
            PORTD &= 0b00000000;
        }

        PWM_counter++;
    } else {
        // SAW mode
        // (SAW_counter << 3) BECAUSE DAC START 3 PIN (5 BITS DAC)
        // 00011111 + << 3 -> 11111000
        PORTD = (SAW_counter << 3);
        SAW_counter++;

        // MAX VOLTAGE ON DAC IN 31 (0b00011111), BUT OFFSET BY 3 BITS (0b11111000)
        if (SAW_counter > 31) {
            SAW_counter = 0;
        }
    }
}