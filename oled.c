/* communication with oled driver starts with i2c start condition -> 7bit slave address -> r/w bit -> ack bit is send by slave ->
    -> control byte (C0 | DC | 0 | 0 | 0 | 0 | 0 | 0) -> ack bit 

    if C0 = 0, only data is send
    if DC = 0, sent data is for command operation
    if DC = 1, data byte is for RAM operation

*/
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include "i2c.h"
#include "oled.h"

const uint8_t oled_init_commands[] PROGMEM = {           //init cmds stored in FLASH
    0xAE,                                                       // oled off
    0x20, 0x00,                                                 // set horizontal memory addressing mode, POR (whatever it is) = 0x00
    0xA8, 0x7F,                                                 // set multiplex 0x7F for 128x128px display (0x3F displays half a screen (64px), 0x1F displays 32px)
    0xA4,                                                       // A4 - normal, A5 - every pixel on display on regardles of the display data RAM
    0xAF,                                                       // switch on OLED
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

void oled_init(void){
    i2c_init();
    i2c_start(oled_addr);
    i2c_write(oled_send_cmd);
    for (uint8_t i = 0; i < sizeof(oled_init_commands); i++) i2c_write(pgm_read_byte(&oled_init_commands[i]));
    i2c_stop();
}

void oled_set_cursor(uint8_t x, uint8_t y){                     // x in range 0 - 127, y in range 0 - 15
    i2c_start(oled_addr);
    i2c_write(oled_send_cmd);
    i2c_write(x & 0b00001111);                                  // set lower col adress (last four bits of 0b00000000, 00H - 0FH (0b00001111))
    i2c_write(0b00010000 | (x >> 4));                           // set higher col adress (last 3 bits of 0b00010000, 10H - 17H)
    i2c_write(0xB0 | y);                                        // set page address (last 4 bits of 0b10110000, B0H - BFH, 15 pages)
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
    i2c_init();
    for (uint8_t i = 0; i <= 127; i++){
        for (uint8_t j = 0; j <= 15; j++){
            oled_set_cursor(i, j);
            oled_pixel_off();
            //_delay_ms(100);
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
    for (uint8_t column = 0; column < 6; column++){             //for each byte making the digit
        int bits = pgm_read_byte(&oled_font[c][column]);        //read corresponding bits
        i2c_write(bits);                                        //send bits to oled
    }
    i2c_stop();
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
    oled_set_cursor(94,row);
    oled_print_char(arrow_head);
    oled_set_cursor(100,row);
    oled_print_char(arrow_tail);
}

/// oled clear arrows
void oled_clear_arrows(){
    for (uint8_t i = 0; i <= 15; i = i + 1){
        for (uint8_t j = 94; j <= 100; j = j + 6){
            oled_set_cursor(j, i);
            oled_print_char(clear_segment);
        } 
    }
}

/// oled print output names and init values
void oled_print_main(){
    oled_print_channel(2, 1);
    oled_print_big_char(0, 30, 1);
    oled_print_char(percent);
    oled_print_channel(8, 2);
    oled_print_big_char(0, 30, 7);
    oled_print_char(percent);
    oled_print_channel(14, 3);
    oled_print_big_char(0, 30, 13);
    oled_print_char(percent);
}


int oled_stretch (int x) {                                      // abcdefgh -> aabbccddeeffgghh
  x = (x & 0xF0) << 4 | (x & 0x0F);                            
  x = (x << 2 | x) & 0x3333;
  x = (x << 1 | x) & 0x5555;
  return x | x << 1;
}

void oled_print_big_char(int temp, int col, int line){                              //TODO: refactor big_char function
    uint8_t font_size = 3;
    for (int i = 100; i > 0; i = i / 10){
        oled_set_cursor(col, line);
        char digit = get_digit(temp, i);
        i2c_start(oled_addr);
        i2c_write(oled_send_data);
        for (uint8_t column = 0; column < 6; column++){                               //for each byte making the digit
            int bits = oled_stretch(pgm_read_byte(&oled_font[digit][column]));        //read corresponding bits        
            for (uint8_t j = 0; j < font_size; j++) i2c_write(bits);
        }
        i2c_stop();
        line = line + 1;
        oled_set_cursor(col, line);
        i2c_start(oled_addr);
        i2c_write(oled_send_data);

        for (uint8_t column = 0; column < 6; column++){                               //for each byte making the digit
            int bits = oled_stretch(pgm_read_byte(&oled_font[digit][column]));        //read corresponding bits        
            for (uint8_t k = 0; k < font_size; k++) i2c_write(bits>>8);
        }
        i2c_stop();
        col = col + 6*font_size;
        line = line - 1;
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

void oled_print_temp(int temp, int col, int line){
    for (int i = 100; i > 0; i = i / 10){
        oled_set_cursor(col, line);
        char digit = get_digit(temp, i);
        oled_print_char(digit);
        col = col + 8;
    }
    // oled_set_cursor(col, line);
    // oled_print_char(degree);
    // col = col + 8;
    // oled_set_cursor(col, line);
    // oled_print_char(centigrade);
}

