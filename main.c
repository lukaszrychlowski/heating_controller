#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include "i2c.h"
#include "oled.h"

/// pins
#define encoder_A PB5
#define encoder_B PB7
#define encoder_button PB4
#define output_1 PB1
#define output_2 PD6
#define output_3 PB3

/// variables
volatile uint8_t count = 0;
volatile uint8_t change_val;
volatile uint8_t output1_count = 0;
volatile uint8_t output2_count = 0;
volatile uint8_t output3_count = 0;
volatile uint8_t a0;                                        // encoder state variables
volatile uint8_t c0;                                        // 
volatile uint8_t s0;                                        //
uint8_t button_event = 0;
uint8_t output_event = 0;


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
int change_value(bool Up){
    change_val = Up ? 1:-1;
    return change_val;
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
    EICRA = (1 << ISC01) | (1 << ISC00);                 // rising edge of int0 generates interrupt req
    PCIFR = (1 << PCIF0);                               // clear interrupt flag
    //sei();
    while(1){
        set_output1(output1_count);
        oled_print_output1(output1_count);
        set_output2(output2_count);
        oled_print_output2(output2_count);
        set_output3(output3_count);
        oled_print_output3(output3_count);
    }
}

ISR(PCINT0_vect){
    
    uint8_t a = PINB >> encoder_A & 1;                  // read pins state, sets a to 0 or 1
    uint8_t b = PINB >> encoder_B & 1;
    uint8_t s = PINB >> encoder_button & 1;
    if (a != a0){                                       // check if encoder was rotated
        a0 = a;
        if (b != c0){                                   // clockwise, or counterclockwise?
            c0 = b;
            count = count + change_value(a == b);
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
        
        //set_output1(output1_count);
        count = 0;
    }
    else if(output_event == 2){
        output2_count = output2_count + count;
        
        //set_output2(output2_count);
        count = 0;
    }
    else if(output_event == 3){
        output3_count = output3_count + count;
        
        //set_output3(output3_count);
        count = 0;
    }
}
  


