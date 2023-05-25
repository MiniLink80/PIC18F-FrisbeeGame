#include <xc.h>        /* XC8 General Include File */
#include <pic18f4620.h>

#include "lcd.h"
#include "the3.h"
#include <stdint.h>        /* For uint8_t definition */
#include <stdbool.h>       /* For true/false definition */
#include <stdio.h>

#define _XTAL_FREQ  10000000L
#define TEAM_A_P1   objs[0]
#define TEAM_A_P2   objs[1]
#define TEAM_B_P1   objs[2]
#define TEAM_B_P2   objs[3]
#define TARGET      objs[4]
#define FRISBEE     objs[5]
#define OBJ_COUNT   6


bool acceptInterrupts;
int a = 0, b = 0, c = 0, selectedPlayer = 0, scoreA = 0, scoreB = 0, currentSegment = 0, gameSpeed = 16, pulseCounter = 0, frisbeeSteps = 0, curFrisbeeSteps = 0;

GameStates game_state = GS_INACTIVE;
GameElement objs[6];
GameElement* display[4][16];
byte segValues[11] = {0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110, 0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01101111, 0b01000000};
byte portbVals = 0;

void SetupDebouncingTimer();
void PrintPORTBandIntCounts();
void UpdateAndPrintDisplay();
void InitInterrupts();
void InitGameObjects();
GameElement* CheckIfCaughtFrisbee();

void left(GameElement*);
void right(GameElement*);
void up(GameElement*);
void down(GameElement*);
void upleft(GameElement*);
void upright(GameElement*);
void downleft(GameElement*);
void downright(GameElement*);

void __interrupt(high_priority) highIsr(){
    portbVals = PORTB;     // Read portb asap because bouncing can fuck the input up 
    
    if (INTCONbits.TMR0IF) {    // Start accepting interrupts again once TMR0 triggers
        acceptInterrupts = true;
        T0CONbits.TMR0ON = 0;
        INTCONbits.TMR0IF = 0;
    }
        
    if (PIR1bits.TMR1IF) { // Entered every 100ms exactly (inshallah)
        PIR1bits.TMR1IF = 0;
        T1CONbits.TMR1ON = 0;
        TMR1L = 0xED;
        TMR1H = 0x85;
        T1CONbits.TMR1ON = 1;
        
        pulseCounter++;
        
        if (FRISBEE.state == OS_FLYING && pulseCounter % 2 == 0 && game_state == GS_ACTIVE) {
            TARGET.active = ~TARGET.active;
        }
        
        if (pulseCounter == gameSpeed && game_state == GS_ACTIVE) {
            pulseCounter = 0;
            if (FRISBEE.state == OS_FLYING) {
                curFrisbeeSteps++;
                
                FRISBEE.oldX = FRISBEE.x;
                FRISBEE.oldY = FRISBEE.y;
                
                FRISBEE.x = frisbee_steps[curFrisbeeSteps][0];
                FRISBEE.y = frisbee_steps[curFrisbeeSteps][1];
                
                CheckIfCaughtFrisbee();
                
                
                if (FRISBEE.x == TARGET.x && FRISBEE.y == TARGET.y) {
                    FRISBEE.state = OS_FELL;
                    TARGET.active = false;
                    display[TARGET.y-1][TARGET.x-1] = &FRISBEE;
                    TARGET.x = TARGET.y = TARGET.oldX = TARGET.oldY = 0;
                    game_state = GS_INACTIVE;
                    //T1CONbits.TMR1ON = 0;
                    curFrisbeeSteps = 0;
                }
            }
        }
    }
    
    if (INTCONbits.INT0IF/* && PORTBbits.RB0*/){         // INT0
        if (acceptInterrupts) {
            a++;
            SetupDebouncingTimer();
            
            if (objs[selectedPlayer].state == OS_SEL_W_FRISBEE) {
                frisbeeSteps = compute_frisbee_target_and_route(objs[selectedPlayer].x, objs[selectedPlayer].y);
                
                TARGET.active = true;
                //display[TARGET.y-1][TARGET.x-1] = NULL;
                TARGET.x = frisbee_steps[frisbeeSteps-1][0];
                TARGET.y = frisbee_steps[frisbeeSteps-1][1];
                TARGET.oldX = TARGET.oldY = 0;
                
                FRISBEE.x = frisbee_steps[0][0];
                FRISBEE.y = frisbee_steps[0][1];
                FRISBEE.oldX = FRISBEE.oldY = 0;
                FRISBEE.active = true;
                FRISBEE.state = OS_FLYING;
                
                objs[selectedPlayer].state = OS_SELECTED;
                
                CheckIfCaughtFrisbee();
                
                
                game_state = GS_ACTIVE;
                
                PIR1bits.TMR1IF = 0;
                T1CONbits.TMR1ON = 0;
                TMR1L = 0xED;
                TMR1H = 0x85;
                T1CONbits.TMR1ON = 1;
            }
            

        }
    }
    
    else if (INTCON3bits.INT1IF/* && PORTBbits.RB1*/) {  // INT1
        if (acceptInterrupts) {
            b++;
            if (objs[selectedPlayer].state != OS_SEL_W_FRISBEE) {
                objs[selectedPlayer].state = OS_DEFAULT;
                selectedPlayer++;
                selectedPlayer = selectedPlayer % 4;
                objs[selectedPlayer].state = OS_SELECTED;
            }
            
            
            SetupDebouncingTimer();    
        }
    } 
    
    else if (INTCONbits.RBIF) {     // RB<7:4> change
        if (acceptInterrupts) {
            portbVals = portbVals >> 4;
            switch (portbVals) {
                case 0b1110:    // A RB4
                    up(&objs[selectedPlayer]);
                    break;
                case 0b1101:    // B RB5
                    right(&objs[selectedPlayer]);
                    break;
                case 0b1011:    // C RB6
                    down(&objs[selectedPlayer]);
                    break;
                case 0b0111:    // D RB7
                    left(&objs[selectedPlayer]);
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

void __interrupt(low_priority) lowIsr(){
    PIR1bits.TMR2IF = 0;
    PIE1bits.TMR2IE = 0;
    
    byte temp_d = PORTD;
    byte temp_a = PORTA;
    
    LATA = 0b1000;
    LATD = segValues[scoreA];
    __delay_us(4000);
    
    LATA = 0b10000;
    LATD = segValues[10];
    __delay_us(4000);
    
    LATA = 0b100000;
    LATD = segValues[scoreB];
    __delay_us(4000);
    
    LATA = temp_a;
    LATD = temp_d;
    
    PIE1bits.TMR2IE = 1;
}

//   A    B    C    D
//  RB4  RB5  RB6  RB7


void main(void)
{
    InitInterrupts();
    InitLCD();
    InitGameObjects();
    
    AddCustomCharacters();  // Load the player characters to the LCD display
    
    
    
    while(1) {        
        UpdateAndPrintDisplay();
    }
}

void UpdateAndPrintDisplay() {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 16; j++) {
            display[i][j] = NULL;           // Clear the display array
        }
    }
    for (int i = OBJ_COUNT-1; i >= 0; i--) {
        if (objs[i].active) {
            display[objs[i].y-1][objs[i].x-1] = &objs[i];   // Write each game object to the array
        }
    }
    
    for (int i = 0; i < OBJ_COUNT; i++) {   // Clear each element's old position (instead of clearing every LCD cell)
                                            // Doing it this way is more optimal as LCDGoto and LCDDat take 4ms each.
                                            // Keeping track of old positions is very important
        if ((objs[i].oldX == 0 && objs[i].oldY == 0) || display[objs[i].oldY-1][objs[i].oldX-1] != NULL) {
            continue;
        }
        lcd_x = objs[i].oldX;
        lcd_y = objs[i].oldY;
        LCDGoto(lcd_x, lcd_y);
        LCDDat(' ');
    }
    
    for (int i = 0; i < OBJ_COUNT; i++) {
        lcd_x = objs[i].x;
        lcd_y = objs[i].y;
        if (objs[i].type == OT_TARGET || objs[i].type == OT_FRISBEE) {
            if (display[lcd_y-1][lcd_x-1]->type == OT_PLAYERA) continue;
            if (display[lcd_y-1][lcd_x-1]->type == OT_PLAYERB) continue;
        }

        if (lcd_x == 0 || lcd_y == 0) {
            continue;
        }
        LCDGoto(lcd_x, lcd_y);
        
        if (objs[i].active == false) {
            LCDDat(' ');
            continue;
        }
        
        switch(objs[i].type) {
            case OT_FRISBEE:
                LCDDat(FRISBEE_OFFSET);
                break;
            case OT_TARGET:
                LCDDat(TARGET_OFFSET);
                break;
            case OT_PLAYERA:
                switch (objs[i].state) {
                    case OS_DEFAULT:
                        LCDDat(PLA_OFFSET);
                        break;
                    case OS_SELECTED:
                        LCDDat(PLA_SEL_OFFSET);
                        break;
                    case OS_SEL_W_FRISBEE:
                        LCDDat(PLA_FRI_OFFSET);
                        break;
                    default:
                        // shouldn't get here
                        break;
                }
                break;
            case OT_PLAYERB:
                switch (objs[i].state) {
                    case OS_DEFAULT:
                        LCDDat(PLB_OFFSET);
                        break;
                    case OS_SELECTED:
                        LCDDat(PLB_SEL_OFFSET);
                        break;
                    case OS_SEL_W_FRISBEE:
                        LCDDat(PLB_FRI_OFFSET);
                        break;
                    default:
                        // shouldn't get here
                        break;
                }
                break;
            default:
                // shouldn't get here
                break;
        }
    }
}

void InitGameObjects() {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 16; j++) {
            display[i][j] = NULL;
        }
    }
    
    selectedPlayer = 0;
    
    TEAM_A_P1.x = 3;
    TEAM_A_P1.y = 2;
    TEAM_A_P1.oldX = 0;
    TEAM_A_P1.oldY = 0;
    TEAM_A_P1.type = OT_PLAYERA;
    TEAM_A_P1.state = OS_SELECTED;
    TEAM_A_P1.active = true;
    
    TEAM_A_P2.x = 3;
    TEAM_A_P2.y = 3;
    TEAM_A_P2.oldX = 0;
    TEAM_A_P2.oldY = 0;
    TEAM_A_P2.type = OT_PLAYERA;
    TEAM_A_P2.state = OS_DEFAULT;
    TEAM_A_P2.active = true;
    
    TEAM_B_P1.x = 14;
    TEAM_B_P1.y = 2;
    TEAM_B_P1.oldX = 0;
    TEAM_B_P1.oldY = 0;
    TEAM_B_P1.type = OT_PLAYERB;
    TEAM_B_P1.state = OS_DEFAULT;
    TEAM_B_P1.active = true;
    
    TEAM_B_P2.x = 14;
    TEAM_B_P2.y = 3;
    TEAM_B_P2.oldX = 0;
    TEAM_B_P2.oldY = 0;
    TEAM_B_P2.type = OT_PLAYERB;
    TEAM_B_P2.state = OS_DEFAULT;
    TEAM_B_P2.active = true;
    
    FRISBEE.x = 9;
    FRISBEE.y = 2;
    FRISBEE.oldX = 0;
    FRISBEE.oldY = 0;
    FRISBEE.type = OT_FRISBEE;
    FRISBEE.state = OS_FELL;
    FRISBEE.active = true;
    
    TARGET.x = 1;
    TARGET.y = 1;
    TARGET.oldX = 0;
    TARGET.oldY = 0;
    TARGET.type = OT_TARGET;
    TARGET.active = false;
    
    for (int i = 0; i < OBJ_COUNT; i++) {
        display[objs[i].y][objs[i].x] = &objs[i];
    }
}

void PrintPORTBandIntCounts() {
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
    TMR0H = 0x80;
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
    
    INTCONbits.RBIE = 0;    // Weird roundabout way to prevent RB interrupt from triggering at the start
    INTCONbits.GIE = 1;     // Enable global interrupts
    PORTB = PORTB;
    INTCONbits.RBIF = 0;
    INTCONbits.RBIE = 1;
    
    
    INTCON2bits.RBIP = 1;   // Set priorities for interrupts
    INTCON2bits.TMR0IP = 1;
    INTCON3bits.INT1IP = 1;
    IPR1bits.TMR1IP = 1;
    
    PIE1bits.TMR1IE = 1;    // Setup TMR1 for a pulse every 100ms
    PIR1bits.TMR1IF = 0;
    IPR1bits.TMR1IP = 1;
    TMR1L = 0xED;
    TMR1H = 0x85;
    T1CON = 0b10110000;
    
    PIE1bits.TMR2IE = 1;    // Setup TMR2 for the 7 segment display
    PIR1bits.TMR2IF = 0;
    IPR1bits.TMR2IP = 0;
    RCONbits.IPEN = 1;
    T2CON = 0b01111111;
}




void SetupDebouncingTimer() {
    acceptInterrupts = false;
    TMR0L = 0;
    TMR0H = 0;
    T0CONbits.TMR0ON = 1;
}

GameElement* CheckIfCaughtFrisbee() {
    // Assume frisbee is in state OS_FLYING
    
    for (int i = 0; i < 4; i++) {
        if (objs[i].x == FRISBEE.x && objs[i].y == FRISBEE.y) {
            if (objs[i].type == OT_PLAYERA) {
                scoreA++;
            }
            else {
                scoreB++;
            }
            game_state = GS_INACTIVE;
            curFrisbeeSteps = 0;
            TARGET.active = false;
            FRISBEE.active = false;
            FRISBEE.state == OS_FELL;
            objs[selectedPlayer].state = OS_DEFAULT;
            objs[i].state = OS_SEL_W_FRISBEE;
            selectedPlayer = i;
            
            //T1CONbits.TMR1ON = 0;
            
            return &objs[i];
        }
    }
    
    return NULL;
}

void CatchFrisbee(GameElement* pl) {
    if (game_state == GS_INACTIVE) {
        pl->state = OS_SEL_W_FRISBEE;
        FRISBEE.active = false;
        display[FRISBEE.y-1][FRISBEE.x-1] = NULL;
        FRISBEE.x = FRISBEE.y = FRISBEE.oldX = FRISBEE.oldY = 0;
    }
    
}

void left(GameElement* pl) {
    if (pl->x == 1) return;
    if (display[pl->y-1][pl->x-2] != NULL) {
        if (display[pl->y-1][pl->x-2]->type == OT_PLAYERA) return;
        if (display[pl->y-1][pl->x-2]->type == OT_PLAYERB) return;
        if (display[pl->y-1][pl->x-2]->type == OT_FRISBEE) {
            CatchFrisbee(pl);
        }
    }
    pl->oldX = pl->x;
    pl->oldY = pl->y;
    pl->x--;
    return;
}
void right(GameElement* pl) {
    if (pl->x == 16) return;
    if (display[pl->y-1][pl->x] != NULL) {
        if (display[pl->y-1][pl->x]->type == OT_PLAYERA) return;
        if (display[pl->y-1][pl->x]->type == OT_PLAYERB) return;
        if (display[pl->y-1][pl->x]->type == OT_FRISBEE) {
            CatchFrisbee(pl);
        }
    }
    pl->oldX = pl->x;
    pl->oldY = pl->y;
    pl->x++;
    return;
}
void up(GameElement* pl) {
    if (pl->y == 1) return;
    if (display[pl->y-2][pl->x-1] != NULL) {
        if (display[pl->y-2][pl->x-1]->type == OT_PLAYERA) return;
        if (display[pl->y-2][pl->x-1]->type == OT_PLAYERB) return;
        if (display[pl->y-2][pl->x-1]->type == OT_FRISBEE) {
            CatchFrisbee(pl);
        }
    }
    pl->oldX = pl->x;
    pl->oldY = pl->y;
    pl->y--;
    return;
}
void down(GameElement* pl) {
    if (pl->y == 4) return;
    if (display[pl->y][pl->x-1] != NULL) {
        if (display[pl->y][pl->x-1]->type == OT_PLAYERA) return;
        if (display[pl->y][pl->x-1]->type == OT_PLAYERB) return;
        if (display[pl->y][pl->x-1]->type == OT_FRISBEE) {
            CatchFrisbee(pl);
        }
    }
    pl->oldX = pl->x;
    pl->oldY = pl->y;
    pl->y++;
    return;
}


void upleft(GameElement* pl) {
    if (pl->x == 1 || pl->y == 1) return;
    if (display[pl->y-2][pl->x-2] != NULL) {
        if (display[pl->y-2][pl->x-2]->type == OT_PLAYERA) return;
        if (display[pl->y-2][pl->x-2]->type == OT_PLAYERB) return;
        if (display[pl->y-2][pl->x-2]->type == OT_FRISBEE) {
            CatchFrisbee(pl);
        }
    }
    pl->oldX = pl->x;
    pl->oldY = pl->y;
    pl->x--;
    pl->y--;
    return;
}
void upright(GameElement* pl) {
    if (pl->x == 16 || pl->y == 1) return;
    if (display[pl->y-2][pl->x] != NULL) {
        if (display[pl->y-2][pl->x]->type == OT_PLAYERA) return;
        if (display[pl->y-2][pl->x]->type == OT_PLAYERB) return;
        if (display[pl->y-2][pl->x]->type == OT_FRISBEE) {
            CatchFrisbee(pl);
        }
    }
    pl->oldX = pl->x;
    pl->oldY = pl->y;
    pl->x++;
    pl->y--;
    return;
}

void downleft(GameElement* pl) {
    if (pl->x == 1 || pl->y == 16) return;
    if (display[pl->y][pl->x-2] != NULL) {
        if (display[pl->y][pl->x-2]->type == OT_PLAYERA) return;
        if (display[pl->y][pl->x-2]->type == OT_PLAYERB) return;
        if (display[pl->y][pl->x-2]->type == OT_FRISBEE) {
            CatchFrisbee(pl);
        }
    }
    pl->oldX = pl->x;
    pl->oldY = pl->y;
    pl->x--;
    pl->y++;
    return;
}
void downright(GameElement* pl) {
    if (pl->x == 16 || pl->y == 16) return;
    if (display[pl->y][pl->x] != NULL) {
        if (display[pl->y][pl->x]->type == OT_PLAYERA) return;
        if (display[pl->y][pl->x]->type == OT_PLAYERB) return;
        if (display[pl->y][pl->x]->type == OT_FRISBEE) {
            CatchFrisbee(pl);
        }
    }
    pl->oldX = pl->x;
    pl->oldY = pl->y;
    pl->x++;
    pl->y++;
    return;
}
