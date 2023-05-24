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
    c++;
    return;
}
void down() {
    c++;
    return;
}

void __interrupt(high_priority) highIsr(){
    if (INTCONbits.TMR0IF) {    // Start accepting interrupts again once TMR0 triggers
        acceptInterrupts = true;
        T0CONbits.TMR0ON = 0;
        INTCONbits.TMR0IF = 0;
    }
    
    int aminakoyum = PORTB; // Read portb cause apparently we have to do that to clear the flag
    
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
            // Call the appropriate function (if I'm able to make this fucking thing work)
            /*
            int rb74 = PORTB >> 4;
            switch (rb74) {
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
                    rb74 = 0;
                    break;
            }*/
            
            c++;
            SetupDebouncingTimer();    
        }
    } 
    INTCON3bits.INT1IF = 0;
    INTCONbits.INT0IF = 0;
    INTCONbits.RBIF = 0;
    
}



typedef enum GameStates {
    GS_ACTIVE,
    GS_INACTIVE
} GameStates;

typedef enum ObjectStates {
    OS_DEFAULT,
    OS_SELECTED,
    OS_W_FRISBEE,
    OS_SEL_W_FRISBEE,  // For players
            
    OS_FLYING,
    OS_FELL            // For frisbee
} ObjectStates;

typedef enum ObjectTypes {
    OT_PLAYERA,
    OT_PLAYERB,
    OT_FRISBEE,
    OT_TARGET
} ObjectTypes;

typedef struct GameElement { // Players, the frisbee, and the target will use this struct
    byte x, y;
    bool active;
    ObjectTypes type;
    ObjectStates state;
} GameElement;

typedef unsigned short ushort;
GameStates game_state;
GameElement objs[6];


//   A    B    C    D
//  RB4  RB5  RB6  RB7


void main(void)
{
    a = b = c = 0;          // a, b, and c are the number of times INT0, INT1, and RB are triggered (for debugging)
    
    ADCON1 = 0b0111;        // Set RB<4:0> as digital pins (See page 16 and 225 of datasheet for more info)
    INTCON2bits.RBPU = 0;   // Enable portb pull-ups (whatever the fuck that means since it doesn't do shit)
    PORTB = 0;              // Futile attempt at giving PORTB a default value of 0
    
    T3CON = 0b10000001;     // Initialize TMR3 (16 bits) for pseudo-random number generation
    TRISB = 0xff;           // Set input/outputs
    TRISA = 0x0;
    TRISD = 0x0;
    
    game_state = GS_INACTIVE;
    
    T0CON = 0b00010001;     // TMR0 setup for button debouncing (will start as inactive)
    acceptInterrupts = true;
    
    INTCONbits.PEIE = 1;    // Enable peripheral interrupts
    INTCONbits.INT0E = 1;   // Enable INT0
    INTCONbits.RBIE = 1;    // Enable PORTB on change interrupts
    INTCONbits.T0IE = 1;    // Enable TMR0 interrupts
    INTCON3bits.INT1E = 1;  // Enable INT1
    
    INTCON2bits.INTEDG0 = 1;// INT0 and INT1 only trigger on falling edge (doesn't seem to do shit either)
    INTCON2bits.INTEDG1 = 1;
    
    
    INTCONbits.RBIF = 0;    // Set interrupt flags to 0 just in case
    INTCONbits.INT0IF = 0;
    INTCON3bits.INT1IF = 0;
    
    
    INTCONbits.GIE = 1;     // Enable global interrupts
    
    
    
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