#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <stdbool.h>

/// pins
#define sda PB0
#define scl PB2
#define encoder_A PB5
#define encoder_B PB7
#define encoder_button PB4
#define output_1 PB1
#define output_2 PD3
#define output_3 PB3

/// macros
#define sda_high() DDRB &= ~(1 << sda);
#define sda_low() DDRB |= (1 << sda);
#define scl_high() DDRB &= (1 << scl);
#define scl_low() DDRB |= (1 << scl);
#define oled_send_cmd 0x00                                  // sh1107 send command mode
#define oled_send_data 0x40                                 // sh1107 send data mode

/// i2c addresses
#define oled_addr 0x78                                      // addting rw bit == 0, 0x3C << 1 = 0x78

/// variables
volatile uint8_t count = 0;
volatile uint8_t output1_count = 0;
volatile uint8_t output2_count = 0;
volatile uint8_t output3_count = 0;
volatile uint8_t a0;                                        // encoder state variables
volatile uint8_t c0;                                        // 
volatile uint8_t s0;                                        //
uint8_t button_event = 0;
uint8_t output_event = 0;

/// i2c init
void i2c_init(){
    DDRB  &= ~((1 << sda) | (1 << scl));
    PORTB &= ~((1 << sda) | (1 << scl));
}

/// i2c write data
void i2c_write(uint8_t dat){
    for (uint8_t i = 8; i; i--){                            // 8 bits
        sda_low();
        if (dat & 0b10000000) sda_high();                   // check if msb == 1, if so sda high
        scl_high();
        dat = dat << 1;
        scl_low();
    }
    sda_high();
    scl_high();
    asm("nop"); 
    scl_low();
}

/// i2c start condition
void i2c_start(uint8_t addr){
    sda_low();
    scl_low();
    i2c_write(addr);
}

/// i2c stop condition
void i2c_stop(){
    sda_low();
    scl_high();
    sda_high();
}

/// oled init commands, stored in flash
const uint8_t oled_init_commands[] PROGMEM = {
    0x20, 0x00,                                             // set horizontal memory addressing mode
    0xA8, 0x7F,                                             // set multiplex 0x7F for 128x128px display (0x3F displays half a screen (64px), 0x1F displays 32px)
    0xA4,                                                   // A4 - normal, A5 - every pixel on display on regardles of the display data RAM
    0xAF,                                                   // switch on oled
};

/// font used, stored in flash
const uint8_t oled_font[][6] PROGMEM = {
{ 0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00 },                     // 0
{ 0x00, 0x42, 0x7F, 0x40, 0x00, 0x00 },                     // 1
{ 0x72, 0x49, 0x49, 0x49, 0x46, 0x00 },                     // 2
{ 0x21, 0x41, 0x49, 0x4D, 0x33, 0x00 },                     // 3
{ 0x18, 0x14, 0x12, 0x7F, 0x10, 0x00 },                     // 4
{ 0x27, 0x45, 0x45, 0x45, 0x39, 0x00 },                     // 5
{ 0x3C, 0x4A, 0x49, 0x49, 0x31, 0x00 },                     // 6
{ 0x41, 0x21, 0x11, 0x09, 0x07, 0x00 },                     // 7
{ 0x36, 0x49, 0x49, 0x49, 0x36, 0x00 },                     // 8
{ 0x46, 0x49, 0x49, 0x29, 0x1E, 0x00 },                     // 9
{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },                     // 10 Space
{ 0xc6, 0x66, 0x30, 0x18, 0xcc, 0xc4 },                     // 11 %
{ 0x3E, 0x41, 0x41, 0x41, 0x22, 0x00 },                     // 12 C
{ 0x7f, 0x04, 0x04, 0x04, 0x78, 0x00 },                     // 13 H
{ 0x00, 0x08, 0x1c, 0x1c, 0x3e, 0x7f },                     // 14 arrow head
{ 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c },                     // 15 arrow tail
{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },                     // 16 clear segment
};
const uint8_t space = 10;
const uint8_t percent = 11;
const uint8_t centigrade = 12;
const uint8_t letter_h = 13;
const uint8_t arrow_head = 14;
const uint8_t arrow_tail = 15;
const uint8_t clear_segment = 16;

/// oled setup and send init commands
void oled_init(void){
    i2c_init();
    i2c_start(oled_addr);
    i2c_write(oled_send_cmd);
    for (uint8_t i = 0; i < sizeof(oled_init_commands); i++) i2c_write(pgm_read_byte(&oled_init_commands[i]));
    i2c_stop();
}

/// oled where the text will be displayed
void oled_set_cursor(uint8_t x, uint8_t y){                 // x in range 0 - 127, y in range 0 - 15
    i2c_start(oled_addr);
    i2c_write(oled_send_cmd);
    i2c_write(x & 0b00001111);                              // set lower col adress (last four bits of 0b00000000, 00H - 0FH (0b00001111))
    i2c_write(0b00010000 | (x >> 4));                       // set higher col adress (last 3 bits of 0b00010000, 10H - 17H)
    i2c_write(0xB0 | y);                                    // set page address (last 4 bits of 0b10110000, B0H - BFH, 15 pages)
    i2c_stop();
}

/// oled set pixel to 0
void oled_pixel_off(void){
    i2c_start(oled_addr);
    i2c_write(oled_addr);
    i2c_write(oled_send_data);
    i2c_write(0x00);
}

/// oled clear the screen
void oled_clear(void){
    for (uint8_t i = 0; i <= 15; i++){
        for (uint j = 0; j <= 127; j++){
            oled_set_cursor(j,i);
            oled_pixel_off();
        }
    }
}

/// oled print the letter
void oled_print_char(int c){
    i2c_start(oled_addr);
    i2c_write(oled_send_data);
    for (uint8_t column = 0; column < 6; column++){         // for each byte making the digit
        int bits = pgm_read_byte(&oled_font[c][column]);    // read corresponding bits
        i2c_write(bits);                                    // send bits to oled
    }
    i2c_stop();
}

/// get individual digits of given number 
uint8_t get_digit(unsigned int val, unsigned int div){      
     return (val/div) % 10;                                 
}

/// oled print a number
void oled_print(int value, int col, int line){
    for (int i = 100; i > 0; i = i / 10){
        oled_set_cursor(col, line);
        char digit = get_digit(value, i);
        oled_print_char(digit);
        col = col + 9;
    }
    oled_set_cursor(col, line);
}

/// oled print 'ch#' at given row
void oled_print_channel(uint8_t row, uint8_t channel_no){
    uint8_t k = 12;
    for (uint8_t i = 0; i < 15; i = i + 7){
        oled_set_cursor(i, row);
        if(i < 8) oled_print_char(k);
        else if(i > 7) oled_print_char(channel_no);
        k = k + 1;
    }
}

/// oled print arrow symbol
void oled_print_arrow(uint8_t row){
    oled_set_cursor(25,row);
    oled_print_char(arrow_head);
    oled_set_cursor(31,row);
    oled_print_char(arrow_tail);
}

/// oled clear arrows
void oled_clear_arrows(){
    for (uint8_t i = 0; i <= 10; i = i + 5){
        for (uint8_t j = 25; j <= 31; j = j + 6){
            oled_set_cursor(j, i);
            oled_print_char(clear_segment);
        } 
    }
}

/// oled print current values for ouputs
void oled_print_output1(){
    oled_clear_arrows();
    oled_print_arrow(0);
    oled_print(output1_count, 0, 0 + 2);
    oled_print(100 * output1_count / 255, 0, 0 + 3)
}

void oled_print_output2(){
    oled_clear_arrows();
    oled_print_arrow(5);
    oled_print(output2_count, 0, 5 + 2);
    oled_print(100 * output2_count / 255, 0, 5 + 3)
}

void oled_print_output3(){
    oled_clear_arrows();
    oled_print_arrow(10);
    oled_print(output2_count, 0, 10 + 2);
    oled_print(100 * output2_count / 255, 0, 10 + 3)
}

/// oled print output names and init values
void oled_print_main(){
    oled_print_channel(0, 1);
    oled_print(0, 0, 2);
    oled_print(0, 0, 3);
    oled_print_char(percent);
    oled_print_channel(5, 2);
    oled_print(0, 0, 7);
    oled_print(0, 0, 8);
    oled_print_char(percent);
    oled_print_channel(10, 3);
    oled_print(0, 0, 12);
    oled_print(0, 0, 13);
    oled_print_char(percent);
}

/// pwm set up output1
void set_output1(volatile uint8_t i){
    TCCR1A = (1 << COM1A1) | (1 << WGM10);              // phase corrected pwm output with ocr1a compare
    TCCR1B = (1 << CS12) | (1 << CS10);                 // set the clock source for pwm to clk_io/1024
    OCR1A = i;
}

/// pwm set up output2
void set_output2(volatile uint8_t i){
    TCCR0A = (1 << COM0A1) | (1 << WGM00);
    TCCR0B = (1 << CS02) | (1 << CS00);
    OCR0A = i;
}

/// pwm set up output3
void set_output3(volatile uint8_t i){
    TCCR2A = (1 << COM2A1) | (1 << WGM20);
    TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20);   // clock source to clk_io/1024
    OCR2A = i;
}

/// encoder change value of the variable, up or down
int change_val(bool Up){
    count = Up ? 1:-1;
    return count;
}

int main(void){
    oled_init();
    oled_clear();
    oled_print_main();
    DDRB = (1 << output_1) | (1 << output_3);
    PORTB = (1 << encoder_A) | (1 << encoder_B) | (1 << encoder_button);
    DDRD = (1 << output_2);
    PORTD = (1 << output_2);
    SREG = (1 << 7);                                    // global interrupt enable
    PCMSK0 = (1 << encoder_A) | (1 << encoder_button);  // set interrupt event to trigger on state-change of encoder pins
    PCICR = (1 << PCIE0);                               // pin change interrupt enabled
    EICRA = (1 << ISC01);                               // falling edge of int0 generates interrupt req
    PCIFR = (1 << PCIF0);                               // clear interrupt flag

    while(1){}
}

ISR(PCINT0_vect){
    uint8_t a = PINB >> encoder_A & 1;                  // read pins state
    uint8_t b = PINB >> encoder_B & 1;
    uint8_t s = PINB >> encoder_button & 1;
    if (a != a0){                                       // check if encoder was rotated
        a0 = a;
        if (b != c0){                                   // clockwise, or counterclockwise?
            c0 = b;
            count = count + change_count(a == b);
        }
    }
    else if(s != s0){                                   // check if button was pressed
        s0 = s;
        button_event += 1;
        if (button_event == 2){                         // encoder makes two clicks when pressed
            output_event += 1;
            button_event = 0;
            if(output_event > 3) output_event = 1;
        }
    }
    
    if(output_event == 1){
        output1_count = output1_count + count;
        oled_print_output1();
        output1(output1_count);
        count = 0;
    }
        else if(output_event == 2){
            output2_count = output2_count + count;
            oled_print_output2();
            output2(output2_count);
            count = 0;
        }
        else if(output_event == 3){
            output3_count = output3_count + count;
            oled_print_output3();
            output3(output3_count);
            count = 0;
        }
    
  }


