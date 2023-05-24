#include <xc.h>        /* XC8 General Include File */
#include <pic18f4620.h>

#include "lcd.h"
#include "the3.h"
#include <stdint.h>        /* For uint8_t definition */
#include <stdbool.h>       /* For true/false definition */
#include <stdio.h>
#define _XTAL_FREQ   10000000L


bool acceptInterrupts;
int a, b, c;

GameStates game_state;
GameElement* objs[6];
GameElement* display[4][16];


void SetupDebouncingTimer();
void InitInterrupts();
void left();
void right();
void up();
void down();

void __interrupt(high_priority) highIsr(){
    byte portbVals = PORTB;     // Read portb cause apparently we have to do that to clear the flag
    
    if (INTCONbits.TMR0IF) {    // Start accepting interrupts again once TMR0 triggers
        acceptInterrupts = true;
        T0CONbits.TMR0ON = 0;
        INTCONbits.TMR0IF = 0;
    }
        
    if (INTCONbits.INT0IF/* && PORTBbits.RB0*/){         // INT0
        if (acceptInterrupts) {
            a++;
            SetupDebouncingTimer();
        }
    }
    
    if (INTCON3bits.INT1IF/* && PORTBbits.RB1*/) {  // INT1
        if (acceptInterrupts) {
            b++;
            SetupDebouncingTimer();    
        }
    } 
    
    if (INTCONbits.RBIF) {     // RB<7:4> change
        if (acceptInterrupts) {
            portbVals = portbVals >> 4;
            switch (portbVals) {
                case 0b1110:    // A RB4
                    up();
                    break;
                case 0b1101:    // B RB5
                    right();
                    break;
                case 0b1011:    // C RB6
                    down();
                    break;
                case 0b0111:    // D RB7
                    left();
                    break;
                default:
                    portbVals = 0;
                    break;
            }
            
            c++;
            SetupDebouncingTimer();    
        }
    } 
    INTCON3bits.INT1IF = 0;
    INTCONbits.INT0IF = 0;
    INTCONbits.RBIF = 0;
    
}


//   A    B    C    D
//  RB4  RB5  RB6  RB7


void main(void)
{
    
    game_state = GS_INACTIVE;
    
    InitInterrupts();
            
    InitLCD();
    lcd_x = 1;
    lcd_y = 1;
    AddCustomCharacters();  // Load the player characters to the LCD display
    
    
    while(1)
    {
        char arr[5];
        
        lcd_x = 1;  // Print the current state of PORTB
        lcd_y = 1;
        LCDGoto(lcd_x, lcd_y);
        for (int i = 7; i >= 0; i--) {
            LCDDat(((PORTB >> i) & 1) ? '1' : '0');
        }
        
        lcd_y = 2;  // Print the number of times INT0 was triggered (properly after debouncing)
        lcd_x = 1;
        LCDGoto(lcd_x, lcd_y);
        sprintf(arr, "%d", a);
        LCDStr(arr);
        
        lcd_y = 3;  // INT1
        lcd_x = 1;
        LCDGoto(lcd_x, lcd_y);
        sprintf(arr, "%d", b);
        LCDStr(arr);
        
        lcd_y = 4;  // RB
        lcd_x = 1;
        LCDGoto(lcd_x, lcd_y);
        sprintf(arr, "%d", c);
        LCDStr(arr);
    }
}



void InitInterrupts() {
    a = b = c = 0;          // a, b, and c are the number of times INT0, INT1, and RB are triggered (for debugging)
    
    ADCON1 = 0b0111;        // Set RB<4:0> as digital pins (See page 16 and 225 of datasheet for more info)
    INTCON2bits.RBPU = 0;   // Enable portb pull-ups (whatever the fuck that means since it doesn't do shit)
    PORTB = 0;              // Futile attempt at giving PORTB a default value of 0
    
    T3CON = 0b10000001;     // Initialize TMR3 (16 bits) for pseudo-random number generation
    TRISB = 0xff;           // Set input/outputs
    TRISA = 0x0;
    TRISD = 0x0;
    
    T0CON = 0b00010011;     // TMR0 setup for button debouncing (will start as inactive)
    acceptInterrupts = false;
    TMR0L = 0;
    TMR0H = 0xFB;
    T0CONbits.TMR0ON = 1;
    
    INTCONbits.PEIE = 1;    // Enable peripheral interrupts
    INTCONbits.INT0E = 1;   // Enable INT0
    INTCONbits.RBIE = 1;    // Enable PORTB on change interrupts
    INTCONbits.T0IE = 1;    // Enable TMR0 interrupts
    INTCON3bits.INT1E = 1;  // Enable INT1
    
    INTCON2bits.INTEDG0 = 0;// INT0 and INT1 only trigger on falling edge (doesn't seem to do shit either)
    INTCON2bits.INTEDG1 = 0;
    
    
    INTCONbits.RBIF = 0;    // Set interrupt flags to 0 just in case
    INTCONbits.INT0IF = 0;
    INTCON3bits.INT1IF = 0;
    
    //INTCONbits.INT0E = 0;
    INTCONbits.RBIE = 0;    // Weird roundabout way to prevent RB interrupt from triggering at the start
    INTCONbits.GIE = 1;     // Enable global interrupts
    PORTB = PORTB;
    INTCONbits.RBIF = 0;
    //INTCONbits.INT0IF = 0;
    INTCONbits.RBIE = 1;
    //INTCONbits.INT0E = 1;
}


void SetupDebouncingTimer() {
    acceptInterrupts = false;
    TMR0L = 0;
    TMR0H = 0;
    T0CONbits.TMR0ON = 1;
}

void left() {
    a++;
    return;
}
void right() {
    a++;
    return;
}
void up() {
    b++;
    return;
}
void down() {
    b++;
    return;
}