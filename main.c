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
int selectedPlayer = 0, scoreA = 0, scoreB = 0, currentSegment = 0, gameSpeed = 16, digital = 0;
int pulseCounter = 0, frisbeeSteps = 0, curFrisbeeSteps = 0, nextGameSpeed = 16;

GameStates game_state = GS_INACTIVE;
GameElement objs[6];
GameElement* display[4][16];
byte segValues[11] = {0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110, 0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01101111, 0b01000000};
byte portbVals = 0;


void SetupDebouncingTimer();
void UpdateAndPrintDisplay();
void InitInterrupts();
void InitGameObjects();
void MoveEveryone();
GameElement* CheckIfCaughtFrisbee();
void CatchFrisbee(GameElement*);

void left(GameElement*);
void right(GameElement*);
void up(GameElement*);
void down(GameElement*);
void upleft(GameElement*);
void upright(GameElement*);
void downleft(GameElement*);
void downright(GameElement*);


void __interrupt(high_priority) highIsr(){
    portbVals = PORTB;          // Read portb asap because bouncing can mess the input up 
    
    if (INTCONbits.TMR0IF) {    // Start accepting interrupts again once TMR0 triggers
        acceptInterrupts = true;
        T0CONbits.TMR0ON = 0;
        INTCONbits.TMR0IF = 0;
    }
    
    if (PIR1bits.ADIF) {
        //ADCON0bits.ADON = 0;
        PIR1bits.ADIF = 0;
        switch(ADRESH) {
            case 0b00:
                nextGameSpeed = 4;
                break;
            case 0b01:
                nextGameSpeed = 8;
                break;
            case 0b10:
                nextGameSpeed = 12;
                break;
            case 0b11:
                nextGameSpeed = 16;
                break;
            default:
                break;
        }
          
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
        
        if (pulseCounter == gameSpeed) {
            pulseCounter = 0;
            if (FRISBEE.state == OS_FLYING && game_state == GS_ACTIVE) {
                
                MoveEveryone();
                
                curFrisbeeSteps++;
                
                FRISBEE.x = frisbee_steps[curFrisbeeSteps][0];
                FRISBEE.y = frisbee_steps[curFrisbeeSteps][1];
                
                CheckIfCaughtFrisbee();
                
                
                if ((FRISBEE.x == TARGET.x && FRISBEE.y == TARGET.y) || curFrisbeeSteps == frisbeeSteps-1) {
                    FRISBEE.state = OS_FELL;
                    TARGET.active = false;
                    display[TARGET.y-1][TARGET.x-1] = &FRISBEE;
                    TARGET.x = TARGET.y = 0;
                    game_state = GS_INACTIVE;
                    curFrisbeeSteps = 0;
                }
            }
        }
        
    }
    
    if (INTCONbits.INT0IF){         // INT0
        if (acceptInterrupts) {
            
            if (objs[selectedPlayer].state == OS_SEL_W_FRISBEE) {
                
                frisbeeSteps = compute_frisbee_target_and_route(objs[selectedPlayer].x, objs[selectedPlayer].y);
                
                TARGET.active = true;
                TARGET.x = frisbee_steps[frisbeeSteps-1][0];
                TARGET.y = frisbee_steps[frisbeeSteps-1][1];
                
                FRISBEE.x = frisbee_steps[0][0];
                FRISBEE.y = frisbee_steps[0][1];
                
                
                FRISBEE.active = true;
                FRISBEE.state = OS_FLYING;
                
                objs[selectedPlayer].state = OS_SELECTED;
                
                
                
                game_state = GS_ACTIVE;
                
                pulseCounter = 0;
                gameSpeed = nextGameSpeed;
                
                PIR1bits.TMR1IF = 0;
                T1CONbits.TMR1ON = 0;
                TMR1L = 0xED;
                TMR1H = 0x85;
                T1CONbits.TMR1ON = 1;
                
                CheckIfCaughtFrisbee();
            }
            SetupDebouncingTimer();
        }
    }
    
    else if (INTCON3bits.INT1IF) {  // INT1
        if (acceptInterrupts) {
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
            
            SetupDebouncingTimer();    
        }
    } 
    INTCON3bits.INT1IF = 0;
    INTCONbits.INT0IF = 0;
    INTCONbits.RBIF = 0;
    PIR1bits.ADIF = 0;
    
}

void __interrupt(low_priority) lowIsr(){
    PIR1bits.TMR2IF = 0;
    PIE1bits.TMR2IE = 0;
    
    byte temp_d = PORTD;
    byte temp_a = PORTA;
    
    LATA = 0b1000;
    LATD = segValues[scoreA];
    __delay_us(3500);
    
    LATA = 0b10000;
    LATD = segValues[10];
    __delay_us(3500);
    
    LATA = 0b100000;
    LATD = segValues[scoreB];
    __delay_us(3500);
    
    
    LATA = temp_a;
    LATD = temp_d;
    
    PIE1bits.TMR2IE = 1;
    
    ADCON0bits.GO = 1;  // Start acquisition for AD
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

void MoveEveryone() {
    for (int i = 0; i < 4; i++) {
        if (i == selectedPlayer) continue;
        
        int val = random_generator(9);
        switch(val) {
            case 0:
                left(&objs[i]);
                break;
            case 1:
                right(&objs[i]);
                break;
            case 2:
                up(&objs[i]);
                break;
            case 3:
                down(&objs[i]);
                break;
            case 4:
                upleft(&objs[i]);
                break;
            case 5:
                upright(&objs[i]);
                break;
            case 6:
                downleft(&objs[i]);
                break;
            case 7:
                downright(&objs[i]);
                break;
            case 8:
                break;
        }
    }
}


void UpdateAndPrintDisplay() {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 16; j++) {
            display[i][j] = NULL;           // Clear the display array
        }
    }
    for (int i = OBJ_COUNT-1; i >= 0; i--) {
        if (objs[i].active && !(objs[i].x == 0 || objs[i].y == 0)) {
            display[objs[i].y-1][objs[i].x-1] = &objs[i];   // Write each game object to the array
        }
    }
    
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 16; j++) {
            lcd_x = j+1;
            lcd_y = i+1;
            LCDGoto(lcd_x, lcd_y);
            
            if (display[i][j] == NULL) {
                LCDDat(' ');
                continue;
            }
            
            switch(display[i][j]->type) {
                case OT_FRISBEE:
                    LCDDat(FRISBEE_OFFSET);
                    break;
                case OT_TARGET:
                    LCDDat(TARGET_OFFSET);
                    break;
                case OT_PLAYERA:
                    switch (display[i][j]->state) {
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
                    switch (display[i][j]->state) {
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
    TEAM_A_P1.type = OT_PLAYERA;
    TEAM_A_P1.state = OS_SELECTED;
    TEAM_A_P1.active = true;
    
    TEAM_A_P2.x = 3;
    TEAM_A_P2.y = 3;
    TEAM_A_P2.type = OT_PLAYERA;
    TEAM_A_P2.state = OS_DEFAULT;
    TEAM_A_P2.active = true;
    
    TEAM_B_P1.x = 14;
    TEAM_B_P1.y = 2;
    TEAM_B_P1.type = OT_PLAYERB;
    TEAM_B_P1.state = OS_DEFAULT;
    TEAM_B_P1.active = true;
    
    TEAM_B_P2.x = 14;
    TEAM_B_P2.y = 3;
    TEAM_B_P2.type = OT_PLAYERB;
    TEAM_B_P2.state = OS_DEFAULT;
    TEAM_B_P2.active = true;
    
    FRISBEE.x = 9;
    FRISBEE.y = 2;
    FRISBEE.type = OT_FRISBEE;
    FRISBEE.state = OS_FELL;
    FRISBEE.active = true;
    
    TARGET.x = 1;
    TARGET.y = 1;
    TARGET.type = OT_TARGET;
    TARGET.active = false;
    
    for (int i = 0; i < OBJ_COUNT; i++) {
        display[objs[i].y][objs[i].x] = &objs[i];
    }
}


void InitInterrupts() {    
    ADCON1 = 0b00001110;    // Set everything but AN0 as digital
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
    
    
    RCONbits.IPEN = 1;      // Set priorities for interrupts
    INTCON2bits.RBIP = 1;   
    INTCON2bits.TMR0IP = 1;
    INTCON3bits.INT1IP = 1;
    IPR1bits.TMR1IP = 1;
    IPR1bits.ADIP = 1;
    IPR1bits.TMR2IP = 0;
    
    PIE1bits.TMR1IE = 1;    // Setup TMR1 for a pulse every 100ms
    PIR1bits.TMR1IF = 0;
    IPR1bits.TMR1IP = 1;
    TMR1L = 0xED;
    TMR1H = 0x85;
    T1CON = 0b10110001;
    
    PIE1bits.TMR2IE = 1;    // Setup TMR2 for the 7 segment display
    PIR1bits.TMR2IF = 0;
    T2CON = 0b01111111;
    
    ADCON0 = 0b00000001;    // Enable AD on AN0
    ADCON2bits.ADCS = 0b010;
    ADCON2bits.ACQT = 0b010;
    ADCON2bits.ADFM = 1;
    
    TRISAbits.RA0 = 1;
    
    PIE1bits.ADIE = 1;
    
}




void SetupDebouncingTimer() {
    acceptInterrupts = false;
    TMR0L = 0;
    TMR0H = 0;
    T0CONbits.TMR0ON = 1;
}

GameElement* CheckIfCaughtFrisbee() {    
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
                        
            return &objs[i];
        }
    }
    
    return NULL;
}

void CatchFrisbee(GameElement* pl) {
    if (game_state == GS_INACTIVE) {
        pl->state = OS_SEL_W_FRISBEE;
        
        display[FRISBEE.y-1][FRISBEE.x-1] = pl;
        FRISBEE.active = false;
        FRISBEE.state = OS_FELL;
        FRISBEE.x = FRISBEE.y = 0;
        
    } else {
        objs[selectedPlayer].state = OS_DEFAULT;
        pl->state = OS_SEL_W_FRISBEE;
        
        for (int i = 0; i < 4; i++) {
            if (&objs[i] == pl) {
                selectedPlayer = i;
                break;
            }
        }
        
        display[FRISBEE.y-1][FRISBEE.x-1] = pl;
        FRISBEE.active = false;
        FRISBEE.x = FRISBEE.y = 0;
        FRISBEE.state = OS_FELL;
        
        TARGET.active = false;
        
        if (pl->type == OT_PLAYERA) scoreA++;
        else                        scoreB++;
        
        game_state = GS_INACTIVE;
        
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
    display[pl->y-1][pl->x-1] = NULL;
    pl->x--;
    display[pl->y-1][pl->x-1] = pl;
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
    display[pl->y-1][pl->x-1] = NULL;
    pl->x++;
    display[pl->y-1][pl->x-1] = pl;
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
    display[pl->y-1][pl->x-1] = NULL;
    pl->y--;
    display[pl->y-1][pl->x-1] = pl;
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
    display[pl->y-1][pl->x-1] = NULL;
    pl->y++;
    display[pl->y-1][pl->x-1] = pl;
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
    display[pl->y-1][pl->x-1] = NULL;
    pl->x--;
    pl->y--;
    display[pl->y-1][pl->x-1] = pl;
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
    display[pl->y-1][pl->x-1] = NULL;
    pl->x++;
    pl->y--;
    display[pl->y-1][pl->x-1] = pl;
    return;
}

void downleft(GameElement* pl) {
    if (pl->x == 1 || pl->y == 4) return;
    if (display[pl->y][pl->x-2] != NULL) {
        if (display[pl->y][pl->x-2]->type == OT_PLAYERA) return;
        if (display[pl->y][pl->x-2]->type == OT_PLAYERB) return;
        if (display[pl->y][pl->x-2]->type == OT_FRISBEE) {
            CatchFrisbee(pl);
        }
    }
    display[pl->y-1][pl->x-1] = NULL;
    pl->x--;
    pl->y++;
    display[pl->y-1][pl->x-1] = pl;
    return;
}
void downright(GameElement* pl) {
    if (pl->x == 16 || pl->y == 4) return;
    if (display[pl->y][pl->x] != NULL) {
        if (display[pl->y][pl->x]->type == OT_PLAYERA) return;
        if (display[pl->y][pl->x]->type == OT_PLAYERB) return;
        if (display[pl->y][pl->x]->type == OT_FRISBEE) {
            CatchFrisbee(pl);
        }
    }
    display[pl->y-1][pl->x-1] = NULL;
    pl->x++;
    pl->y++;
    display[pl->y-1][pl->x-1] = pl;
    return;
}
