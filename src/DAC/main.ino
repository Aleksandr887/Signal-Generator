#define FOSC 16000000
#define BAUD 230400
#define UBRR (double)FOSC / (8 * (double)BAUD) - 1
#define VOLTAGE_STEP (double)5 / (double)32
#define VMAX 5.0
#define VMIN 4.0
#define ADC_SCALE 1024
#define DAC_SCALE 32
#define ADC_PIN 1

uint8_t settings = 0;
double v_max;
double real_step;

void UART_init(uint16_t ubrr)
{
    UCSR0A = 1 << U2X0;
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)(ubrr);
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);
    UCSR0C = (1 << UCSZ00) | (1 << UCSZ01) | (1 << USBS0);
}

void UART_send(uint8_t data)
{
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = data;
}

unsigned char UART_receive(void)
{
    while (!(UCSR0A & (1 << RXC0)));
    return UDR0;
}

void DAC_init()
{
    DDRB = 0b00011111;
}

void DAC_config(char value)
{
    PORTB = value;
}

void UART_receive_strn(char *x, char size)
{
    uint8_t i = 0;

    while (i < size - 1) {
        uint8_t c;
        while (!(UCSR0A & (1 << RXC0)));
        c = UDR0;
        x[i] = c;
        i++;
    }
    x[i] = '\0';
}

void UART_send_float(double value, uint8_t n)
{
    // cast double to str
    String c(value, n);
    // send string one by one char
    for (uint8_t i = 0; i < c.length(); i++) {
        UART_send(c[i]);
    }
    UART_send('\n');
}

void UART_send_str(char *str)
{
    for (uint8_t i = 0; i < strlen(str); i++) {
        UART_send(str[i]);
    }
    UART_send('\n');
}

unsigned int ADC_read(uint8_t pin)
{
    // set pin for read value
    ADMUX |= ((pin & 0b1000) << MUX3) | ((pin & 0b0100) << MUX2) | ((pin & 0b0010) << MUX1) | ((pin & 0b0001) << MUX0);
    // enable converting input analog signal to binary code
    ADCSRA |= (1 << ADSC);
    // wait converting
    while (!(ADCSRA & (1 << ADIF)));
    // reset ADIF
    ADCSRA |= (1 << ADIF);
    // return value of ADC
    return ADC;
}

void ADC_init()
{
    // volrage from AVcc
    ADMUX = (1 << REFS0);
    // ADEN - enable adc
    // ADPSn = 1 (n - 0..2) prescaler = 128
    // F_ADC = 16M/128 = 125kHz
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

double ADC_measur_voltage(uint8_t pin)
{
    // read register value
    int i = ADC_read(1);
    // calculate voltage
    double j = ((double)VMAX / (double)ADC_SCALE) * i;
    // print calculated voltage
    // UART_send_float(j, 4);
    return j;
}

void measure_error()
{
    DAC_config(DAC_SCALE - 1);
    v_max = ADC_measur_voltage(ADC_PIN);
    real_step = v_max / DAC_SCALE;
    if (v_max < VMIN) {
        v_max = VMAX;
        real_step = VOLTAGE_STEP;
    }
    UART_send_float(v_max, 4);
    UART_send_float(real_step, 4);
    DAC_config(0);
}

void setup()
{
    UART_init(round(UBRR));
    // init pins fo out
    DAC_init();
    ADC_init();
}

void loop()
{
    if (!settings) {
        measure_error();
        settings = 1;
    }

    uint8_t pins_value = 0;
    double volt_value = 0.0;
    char volt_str[5];
    // get str from uart (n chars)
    UART_receive_strn(volt_str, sizeof(volt_str));
    // cast str to double (voltage)
    volt_value = strtod(volt_str, NULL);

    // set value for out pins

    while (volt_value > 0) {
        // one step = 5/32 (5 - max voltage, 32 - 2^5 (5 - bit depth of dac))
        // volt_value = volt_value - VOLTAGE_STEP;
        volt_value = volt_value - real_step;
        pins_value += 1;
    }
    // because loop while volt_value > 0, last value of volt_value - negative
    pins_value = (pins_value > 0) ? pins_value - 1 : 0;

    if (pins_value > DAC_SCALE - 1) {
        pins_value = DAC_SCALE - 1;
    }

    // set voltage on dac
    DAC_config(pins_value);

    // out str(voltage)
    UART_send_str(volt_str);
}