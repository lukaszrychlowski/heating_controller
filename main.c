#define F_CPU 8000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <stdbool.h>

// //////// I2C ///////////////////////////////////////////////

//pins
#define sda PB0
#define scl PB2
#define pwm_out PB1
#define encoderA  PB3
#define encoderB  PB4
// //macros
#define sda_high() DDRB &= ~(1<<sda); //release sda, pulled high by oled chip
#define sda_low() DDRB |= (1<<sda); //pulled low by tiny
#define scl_high() DDRB &= ~(1<<scl);
#define scl_low() DDRB |= (1<<scl);

//init
void i2c_init(void) {
  DDRB  &= ~((1<<sda)|(1<<scl));  //release both lines
  PORTB &= ~((1<<sda)|(1<<scl));  //both low
}

//data transfer
void i2c_write(uint8_t dat){
    for (uint8_t i = 8; i; i--){
        sda_low();
        if (dat & 0b10000000) sda_high(); //check if msb == 1, if so sda high
        scl_high();
        dat = dat << 1;
        scl_low();
    }
    sda_high();
    scl_high();
    asm("nop"); 
    scl_low();
}

//start condition
void i2c_start(uint8_t addr){
    sda_low();
    scl_low();
    i2c_write(addr);
}

//stop condition
void i2c_stop(){
    sda_low();
    scl_high();
    sda_high();
}

//////// OlED ///////////////////////////////////////////////

/* communication with oled driver starts with i2c start condition -> 7bit slave address -> r/w bit -> ack bit is send by slave ->
    -> control byte (C0 | DC | 0 | 0 | 0 | 0 | 0 | 0) -> ack bit 

    if C0 = 0, only data is send
    if DC = 0, sent data is for command operation
    if DC = 1, data byte is for RAM operation

*/

#define oled_addr 0x78 // i2c address is 0x3C, we're adding rw bit == 0 (0x3C << 1 = 0x78)
#define oled_send_cmd 0x00 // command mode
#define oled_send_data 0x40 // data mode

const uint8_t oled_init_commands[] PROGMEM = {                  //init cmds stored in FLASH
    0xAE,             // oled off
    0x20, 0x00,       // set horizontal memory addressing mode, POR (whatever it is) = 0x00
    0xA8, 0x7F,       // set multiplex 0x7F for 128x128px display (0x3F displays half a screen (64px), 0x1F displays 32px)
    0xA4,             // A4 - normal, A5 - every pixel on display on regardles of the display data RAM
    0xAF,             // switch on OLED
};

const uint8_t oled_font[][6] PROGMEM = {
{ 0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00 }, //0
{ 0x00, 0x42, 0x7F, 0x40, 0x00, 0x00 }, //1
{ 0x72, 0x49, 0x49, 0x49, 0x46, 0x00 }, //2
{ 0x21, 0x41, 0x49, 0x4D, 0x33, 0x00 }, //3
{ 0x18, 0x14, 0x12, 0x7F, 0x10, 0x00 }, //4
{ 0x27, 0x45, 0x45, 0x45, 0x39, 0x00 }, //5
{ 0x3C, 0x4A, 0x49, 0x49, 0x31, 0x00 }, //6
{ 0x41, 0x21, 0x11, 0x09, 0x07, 0x00 }, //7
{ 0x36, 0x49, 0x49, 0x49, 0x36, 0x00 }, //8
{ 0x46, 0x49, 0x49, 0x29, 0x1E, 0x00 }, //9
{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, //10 Space
{ 0xc6, 0x66, 0x30, 0x18, 0xcc, 0xc4 }, //11 %
{ 0x3E, 0x41, 0x41, 0x41, 0x22, 0x00 }, //12 C
{ 0x7f, 0x04, 0x04, 0x04, 0x78, 0x00 }, //13 H
{ 0x00, 0x08, 0x1c, 0x1c, 0x3e, 0x7f }, //14 arrow head
{ 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c }, //15 arrow tail
{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, //16 clear segment
};
const int space = 10;
const int percent = 11;
const int centigrade = 12;
const int letter_h = 13;
const int arrow_head = 14;
const int arrow_tail = 15;
const int clear_segment = 16;

void oled_init(void){
    i2c_init();
    i2c_start(oled_addr);
    i2c_write(oled_send_cmd);
    for (uint8_t i = 0; i < sizeof(oled_init_commands); i++) i2c_write(pgm_read_byte(&oled_init_commands[i]));
    i2c_stop();
}

void oled_set_cursor(uint8_t x, uint8_t y){     // x in range 0 - 127, y in range 0 - 15
    i2c_start(oled_addr);
    i2c_write(oled_send_cmd);
    i2c_write(x & 0b00001111);                 // set lower col adress (last four bits of 0b00000000, 00H - 0FH (0b00001111))
    i2c_write(0b00010000 | (x >> 4));         // set higher col adress (last 3 bits of 0b00010000, 10H - 17H)
    i2c_write(0xB0 | y);                     // set page address (last 4 bits of 0b10110000, B0H - BFH, 15 pages)
    i2c_stop();
}

void oled_pixel_off(void){
    i2c_start(oled_addr);
    i2c_write(oled_send_data);
    i2c_write(0x00);
    i2c_stop();
}

void oled_pixel_on(void){
    i2c_start(oled_addr);
    i2c_write(oled_send_data);
    i2c_write(0xFF);
    i2c_stop();
}

void oled_clear(void){
    for (uint8_t i = 0; i <= 127; i++){
        for (uint8_t j = 0; j <= 15; j++){
            oled_set_cursor(i, j);
            oled_pixel_off();
        }
    }
}

void oled_light_up(void){
    for (uint8_t j = 0; j <= 15; j++){
        _delay_ms(500);
        for (uint8_t i = 0; i <= 127; i++){
            oled_set_cursor(i, j);
            oled_pixel_on();
        }
    }
}

void oled_print_char(int c){
    i2c_start(oled_addr);
    i2c_write(oled_send_data);
    for (uint8_t column = 0; column < 6; column++){         //for each byte making the digit
        int bits = pgm_read_byte(&oled_font[c][column]);    //read corresponding bits
        i2c_write(bits);                                    //send bits to oled
    }
    i2c_stop();
}

uint8_t get_digit(unsigned int val, unsigned int div){      
    /* assume val = 1234
    div = 1000 -> val/div % 10 = 1.234 -> int 1                          
    div = 100  -> val/div % 10 = 2.34 -> int 2
    div = 10   -> val/div % 10 = 3.4 -> int 3                             
    div = 1    -> val/div % 10 = 4 -> int 4
    */                
    return (val/div) % 10;       
}

void oled_print(int temp, int col, int line){
    for (int i = 100; i > 0; i = i / 10){
        oled_set_cursor(col, line);
        char digit = get_digit(temp, i);
        oled_print_char(digit);
        col = col + 9;
    }
    oled_set_cursor(col, line);
}

void print_channel(uint8_t row, uint8_t channel_no){
    oled_set_cursor(0,row);
    oled_print_char(12);
    oled_set_cursor(7,row);
    oled_print_char(13);
    oled_set_cursor(14,row);
    oled_print_char(channel_no);
}

void print_arrow(uint8_t row){
    oled_set_cursor(25,row);
    oled_print_char(arrow_head);
    oled_set_cursor(31,row);
    oled_print_char(arrow_tail);
}

void clear_arrows(){
        oled_set_cursor(25, 0);
        oled_print_char(clear_segment);
        oled_set_cursor(31, 0);
        oled_print_char(clear_segment);
        oled_set_cursor(25, 5);
        oled_print_char(clear_segment);
        oled_set_cursor(31, 5);
        oled_print_char(clear_segment);
        oled_set_cursor(25, 10);
        oled_print_char(clear_segment);
        oled_set_cursor(31, 10);
        oled_print_char(clear_segment);
}


//////// PWM output ///////////////////////////////////////////////

/* 
atmega168 has 3 separate timers with 2 outputs each controlled by TCCR1, TCCR0 and TCCR2 registers - total 6 pwm outputs with 3 different freq/fill
*/
void pwm_ch1(int i){
    //DDRB = (1 << pwm_out);
    //PORTB = 0b00000001;
	TCCR1A = 0b10000001;
    TCCR1B = 0b00000101;
    OCR1A  = i;
}

void pwm_ch2(int i){
    //DDRB = (1 << pwm_out);
    //PORTB = 0b00000001;
	TCCR0A = 0b10000001;
    TCCR0B = 0b00000101;
    OCR0A  = i;
}

void pwm_ch3(int i){
    //DDRB = (1 << pwm_out);
    //PORTB = 0b00000001;
	TCCR2A = 0b10000001;
    TCCR2B = 0b00000111;
    OCR2A  = i;
}

volatile int a0;
volatile int c0;
volatile uint8_t count = 0;
volatile uint8_t count_ch1 = 0;
volatile uint8_t count_ch2 = 0;
volatile uint8_t count_ch3 = 0;
volatile int s0;
volatile int j = 0;
uint8_t channel = 1;
uint8_t switch_count = 0;

int change_count(bool Up){
    count = Up ? 1 : -1;
    return count;
}

void set_channel1(){
        clear_arrows();
        print_arrow(0);
        oled_print(count_ch1,0,0+2);
        oled_print(100*count_ch1/255,0,0+3);
}
void set_channel2(){
        clear_arrows();
        print_arrow(5);
        oled_print(count_ch2,0,5+2);
        oled_print(100*count_ch2/255,0,5+3);
}
void set_channel3(){
        clear_arrows();
        print_arrow(10);
        oled_print(count_ch3,0,10+2);
        oled_print(100*count_ch3/255,0,10+3);
}

int main(void){   
    oled_init();
    oled_clear();
    
    //ch1
    print_channel(0, 1);
    oled_print(0,0,2);
    oled_print(0,0,3);
    oled_print_char(percent);

    //ch2
    print_channel(5, 2);
    oled_print(0,0,7);
    oled_print(0,0,8);
    oled_print_char(percent);

    //ch3
    print_channel(10, 3);
    oled_print(0,0,12);
    oled_print(0,0,13);
    oled_print_char(percent);
    
    DDRB  = 0b00001010;              //PB1 (ch1) , PB3 (ch3) set to output, rest as inputs
    PORTB = 0b10111010;              //set pullup on PB7(encoder clk), PB4(encoder dt), PB5(encoder sw) set PB1 (ch1) PB3(ch3) high
    DDRD  = 0b01000000;              //PD3(ch2) set to output
    PORTD = 0b01000000;              //PD3(ch2)
    SREG  = 0b10000000;              //global interrupt enable
    PCMSK0 = 0b10100000;             //interrupt set on PB3(encoder clk) AND PB5(encoder sw)
    PCICR = 0b00000001;              //pin change interrupt enable
    EICRA = 0b00001111;              //falling edghe interrupt on INT0 INT1
    PCIFR  = 0b00000001;             //interrupt flag cleared 
    while (1) 
    {
    }

}  

ISR(PCINT0_vect){
    int a = PINB >> PB7 & 1; //encoder clk state
    int b = PINB >> PB4 & 1; //encoder dt state
    int s = PINB >> PB5 & 1; //button state
    if (a != a0){   //check if encoder was rotated
        a0 = a;
        if (b != c0){
            c0 = b;
            count = count + change_count(a == b);
            }
        }   
    else if(s != s0){ //check if button was pressed
        s0 = s;
        switch_count += 1;
        if(switch_count == 2) {
            channel += 1;
            switch_count = 0;
            if(channel > 3) channel = 1;
        }
    }

    if(channel == 1){
        count_ch1 = count_ch1 + count;
        set_channel1();
        pwm_ch1(count_ch1);
        count = 0;
    }
        else if(channel == 2){
            count_ch2 = count_ch2 + count;
            set_channel2();
            pwm_ch2(count_ch2);
            count = 0;
        }
        else if(channel == 3){
            count_ch3 = count_ch3 + count;
            set_channel3();
            pwm_ch3(count_ch3);
            count = 0;
        }
}

