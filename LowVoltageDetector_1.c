#include "stdint.h"
#include "built_in.h"

// key switch states
#define pressed 0
#define release 1
// output states
#define on      1
#define off     0
// boolean
#define false   0
#define true    1
//------------------------------------------------------------------------------

void interrupt(void);
void clockTick(void);
void timeToLCD(char * const str);
void startInit (void);
void updateVoltage(void);
void voltToStr(uint16_t volts);
void menu(void);
void resetToDefaults(void);
//void savePreset(void);
//void saveConfig(void);
//void loadPreset(void);
//void loadConfig(void);
char* codetxt_to_ramtxt(const char*);

// LCD module connections
sbit LCD_RS at RC2_bit;
sbit LCD_EN at RC3_bit;
sbit LCD_D4 at RC4_bit;
sbit LCD_D5 at RC5_bit;
sbit LCD_D6 at RC6_bit;
sbit LCD_D7 at RC7_bit;
sbit LCD_RS_Direction at TRISC2_bit;
sbit LCD_EN_Direction at TRISC3_bit;
sbit LCD_D4_Direction at TRISC4_bit;
sbit LCD_D5_Direction at TRISC5_bit;
sbit LCD_D6_Direction at TRISC6_bit;
sbit LCD_D7_Direction at TRISC7_bit;
// End LCD module connections

// Switch button connections
sbit KEY_SET   at RD1_bit;
sbit KEY_UP    at RD2_bit;
sbit KEY_DOWN  at RD3_bit;

// Relay output
sbit RELAY    at RB0_bit;

// Define flag variable and flag bits
uint8_t volatile flags = 0;           // flag variable for flag bits as below:
sbit newHalfSecond     at flags.B0;   // set in ISR
sbit tickTock          at flags.B1;   // set in clockTick() function
sbit lockout           at flags.B2;   // lockout true when under Vin < Lo Preset
sbit new50mS           at flags.B3;   // set in ISR
sbit HiVoltAuto        at flags.B2;   // auto-on when Vin > Hi Preset

//uint8_t  checksum;              // EEPPROM checksum compute
//uint8_t  byte;                  // EEPROM byte read/written
//uint8_t  addr = 0;              // EEPROM address pointer

uint8_t setKeyDown = 0;         // key pressed timer
char timeStr[] = "00:00:00";
uint8_t seconds = 0;            // elapsed time since output RELAY on
uint8_t minutes = 0;            // ..
uint8_t hours   = 0;            // ..

int8_t   inputOffset = 0;       // input voltage offset
uint16_t presetLoVolts;         // low voltage preset
uint16_t presetHiVolts;         // high voltage preset
uint16_t volts;
char voltstr[6];

uint16_t maxVolts = 1023;              // Maximum preset voltage constant nn.dd volts

uint8_t volatile Tick50mS;      // delay timer ISR tick count
uint8_t volatile delayCount;    // delay timer decrements in ISR every 50mS

void main()
{
 startInit();
 Lcd_Out(1,3,"MINI PROJECT");
 Lcd_Out(2,1,"S6 EEE, RIT,KTYM");
 Delay_ms(500);
 Lcd_Cmd(_LCD_CLEAR);               // Clear display
 Lcd_Out(1,5,"BATTERY");
 Lcd_Out(2,2,"PROTECTOR v1.0");
 Delay_ms(500);
 
 // Load preset from EEPROM, verify checksum and reinitialise if
 // checksum fails.
 //loadPreset();
 //if (checksum){
 //  Lcd_Out(1,1,"Bad config data");
 // Lcd_Out(2,1,"Initialising...");
 //  resetToDefaults();
 //  savePreset();
 //  Delay_ms(100);
 //}

 Lcd_Cmd(_LCD_CLEAR);               // Clear display
 Lcd_Out(1,12,"AUTO");
 Lcd_Out(2,6,"V");
 Lcd_Out(2,12,"OFF");

 lockout = false;
 presetLoVolts = 400;
 presetHiVolts = 600;
 inputOffset = 0;
 HiVoltAuto = true;

 while(1){
  while (1){
   if (newHalfSecond) clockTick();
   if (new50mS) break;          // Run ADC sample and display every 50mS
  }
  new50mS = false;
  updateVoltage();
  if (volts < presetLoVolts && !lockout){
      lockout = true;
      RELAY = off;
      Lcd_Out(2,12,"OFF");
    }
    if (volts > presetHiVolts &&  lockout){
      lockout = false;
      RELAY = on;
      Lcd_Out(2,12,"ON ");
      seconds = minutes = hours = 0;
    }

    if (KEY_SET == pressed){
      if (setKeyDown < 30) setKeyDown++;
      if (setKeyDown == 25){
             menu();
             setKeyDown = 0;
      }
    }
 }
}

void startInit(void){
 Lcd_Init(); // Initialize LCD
 Lcd_Cmd(_LCD_CLEAR); // Clear display
 Lcd_Cmd(_LCD_CURSOR_OFF); // Cursor off

 TRISA2_bit = 1;
 TRISB0_bit = 0;
 RELAY = false;
 TRISD1_bit = 1;
 TRISD2_bit = 1;
 TRISD3_bit = 1;

 PR2 = 249;
  T2CON = 0b01001101;
  /*        ------01  Prescaler is 4
            -----1--  Timer On
            -1001---  1:10 Postscaler
            0-------  not-used
  */

  // Enable Timer2 interrupts
  PEIE_bit   = 1;         // enable peripheral interrupts
  TMR2IE_bit = 1;         // enable TMR2 interrupt bit
  GIE_bit    = 1;         // enable global interrupts
}

void updateVoltage(void)
{
 volts = ADC_Read(2);
 voltToStr(volts);
 Lcd_Out(2,1,voltstr);
}

void voltToStr(uint16_t volts){
 volts = (volts + inputOffset) * 10 / 51;
 WordtoStr(volts,voltstr);
 voltstr[1] = voltstr[2];
 voltstr[2] = voltstr[3];
 voltstr[3] = '.';
 if (voltstr[2] == ' ') {voltstr[2] = '0';}
}

//=============================================================================
// Interrupt Handler
// Timer 2 configured to generate interrupt at 5mS intervals
// Also clear hardware Watch Dog Timer  (CLRWDT)
//=============================================================================
void interrupt(void){

  uint8_t static volatile Tick10mS   = 0;

  TMR2IF_bit = 0;    // clear interrupt flag

  // 10mS ticker
  Tick10mS++;
  if (Tick10mS == 50)   //10mS x 50 = 0.5 second
  {
    Tick10mS = 0;
    newHalfSecond = true;
  }

  Tick50ms++;
  if (Tick50mS == 5)   // 10mS x 5 = 50mS
  {
    if(delayCount) delayCount--;
    Tick50mS = 0;
    new50mS = true;
  }
}

//=============================================================================
//   Interrupt driven delay
//   i x 50mS, delayCount is decremented in ISR
//=============================================================================
void isrDelay(uint8_t i)
{
     delayCount = i;
     Tick50mS = 0;
     while(delayCount);
}

//=============================================================================
//   Call once per second, update time, make time string and output to LCD
//=============================================================================
void clockTick(void){
  newHalfSecond = false;  // clear flag set in interrupt

  if( lockout == false){
    Lcd_out(2,16,"*");
    // Blink centre : continually
    if (tickTock){
      tickTock = false;
      timeStr[2] = ':';
      timeStr[5] = ':';
    }else{
      tickTock = true;
      timeStr[2] = ' ';
      timeStr[5] = ' ';

      seconds++;
      if (seconds == 60){
        seconds = 0;
        minutes++;
      }
      if (minutes == 60){
        minutes = 0;
        hours++;
      }
      if (hours == 100){
        hours = 0;
      }
     }
  }else{
    timeStr[2] = ':';
    timeStr[5] = ':';
    if (tickTock){
      tickTock = false;
      Lcd_out(2,16,"*");
    }else{
      tickTock = true;
      Lcd_out(2,16," ");
    }
  }
  timeToLCD(timeStr);
}

//=============================================================================
//   Format time to string and display on LCD
//=============================================================================
void timeToLCD( char * const str){
  str[6] = 48+seconds/10;
  str[7] = 48+seconds%10;
  str[3] = 48+minutes/10;
  str[4] = 48+minutes%10;
  str[0] = 48+hours/10;
  str[1] = 48+hours%10;

  Lcd_Out (1,2, str);
}

//=============================================================================
//   Menu driven user configuration interface
//=============================================================================
/* Menu
   ----
   Set low preset   (voltage)
   Set high preset  (voltage)
   Set input offset (adjust realtime voltage)
   High preset mode (manual/auto)
   Default all (Yes/No)
   Exit
*/
void menu(void)
{
     uint8_t keyDelay;
     uint8_t menu = 0;
     uint8_t option = 0;

     // Setup menu text strings
     const char * const menuTxt[] =
     {
       "LOW PRESET",
       "HIGH PRESET",
       "INPUT OFFSET",
       "HIGH PRESET MODE",
       "DEFAULT ALL",
       "EXIT"
     };

     const char * const optionHivolt[] =
     {
       "MANUAL",
       "AUTO  ",
     };

     const char * const optionDefaults[] =
     {
       "YES?",
       "NO? ",
     };

     // User configuration interface
     while(1)
     {
       Lcd_Cmd(_LCD_CLEAR);
       Lcd_Out (1,1, codetxt_to_ramtxt(menuTxt[menu]));

       isrDelay(1);  // t x 50mS ISR driven delay
       while(KEY_SET == pressed);

       //------------------------------
       // Main menu option selection
       while(1)
       {
         if (KEY_UP == pressed)
         {
           menu++;
           if (menu > 5 ) menu = 0;
           Lcd_Cmd(_LCD_CLEAR);
           Lcd_Out (1,1, codetxt_to_ramtxt(menuTxt[menu]));
           while (KEY_UP == pressed);
         }
         if (KEY_DOWN == pressed)
         {
           if (menu == 0) menu = 6;
           menu--;
           Lcd_Cmd(_LCD_CLEAR);
           Lcd_Out (1,1, codetxt_to_ramtxt(menuTxt[menu]));
           while (KEY_DOWN == pressed);
         }
         isrDelay(1);  // t x 50mS ISR driven delay
         if (KEY_SET == pressed) break;

       }

       option = 0;
       while (KEY_SET == pressed);
       //Lcd_Out(2,1,LCDclearLine);

       // Set low preset
       while(menu == 0){
           keyDelay = 1;
           if (KEY_UP == pressed && presetLoVolts < maxVolts-5 ){
             presetLoVolts += 5;
             if (presetLoVolts == presetHiVolts) presetHiVolts += 5;
             keyDelay = 4;
           }
           if (KEY_DOWN == pressed && presetLoVolts){
             presetLoVolts -= 5;;
             keyDelay = 4;
           }

           voltToStr(presetLoVolts);
           Lcd_Out(2,1,voltStr);
           isrDelay(KeyDelay);  // t x 50mS ISR driven delay

           if (KEY_SET == pressed) break;
         }

         // Set high preset
         while(menu == 1 ){
           keyDelay = 1;
           if (KEY_UP == pressed && presetHiVolts < maxVolts){
             presetHiVolts += 5;
             keyDelay = 4;
           }
           if (KEY_DOWN == pressed && presetLoVolts>0){
             presetHiVolts -= 5;
             if (presetLoVolts == presetHiVolts) presetLoVolts -= 5;
             keyDelay = 4;
           }

           voltToStr(presetHiVolts);
           Lcd_Out(2,1,voltStr);
           isrDelay(keyDelay);
           if (KEY_SET == pressed) break;
         }

         // Set input offset
         while(menu == 2){
           keyDelay =  1;
           if (KEY_UP == pressed && inputOffset < 126 ){
             inputOffset++;
             keyDelay = 4;
           }
           if (KEY_DOWN == pressed && inputOffset > -126 ){
             inputOffset--;
             keyDelay = 4;
           }

           updateVoltage();

           isrDelay(keyDelay);
           if (KEY_SET == pressed) break;
         }

         //Set high preset operating mode
         while(menu == 3){
           Lcd_Out (2,1, codetxt_to_ramtxt(optionHivolt[option]));

           if (KEY_UP == pressed ){
             option++;
             if (option > 1) option = 0;
             while (KEY_UP == pressed);
           }
           if (KEY_DOWN == pressed){
             if (option == 0) option= 2;
             option--;
             while (KEY_DOWN == pressed);
           }
           switch (option){
             case 0: HiVoltAuto = false; break;
             case 1: HiVoltAuto = true;  break;
           }

           isrDelay(2);
           if (KEY_SET == pressed) break;
         }

         // Set options and presets to default values
         while(menu == 4){
           Lcd_Out (2,1, codetxt_to_ramtxt(optionDefaults[option]));
           if (KEY_UP == pressed ){
             option++;
             if (option > 1) option = 0;
             while (KEY_UP == pressed);
           }
           if (KEY_DOWN == pressed ){
             if (option == 0) option = 2;
             option--;

             while (KEY_DOWN == pressed);
           }
           if (option==0) resetToDefaults();

           isrDelay(2);
           if (KEY_SET == pressed) break;
         }
         // Exit user configuration interface
         if (menu == 5) break;
     }
     // Configuration complete, exit and return to runtime mode.

     Lcd_Out (1,1,"SETUP COMPLETE!");
     //savePreset();
     
     isrDelay(20);
     Lcd_Cmd(_LCD_CLEAR);
     lockout = true;    // force lockout since mode/set points may have been changed
     Lcd_Out(2,6,"V");
     Lcd_Out(1,12,"AUTO");
}
/*
//==============================================================================
//   Save preset and adc offset values to EEPROM
//   with simple checksum
//==============================================================================
void saveConfig()
{
     EEPROM_Write(addr,byte);
     checksum +=byte;
     addr++;
}

//==============================================================================
//   Load preset values from EEPROM and generate checksum
//
//==============================================================================
void loadConfig(void)
{
     byte = EEPROM_read(addr);
     checksum +=byte;
     addr++;
}

void savePreset(void){
     checksum=0;
     addr=0;
     byte = presetLoVolts % 256;
     saveConfig();
     byte = presetLoVolts >> 8;
     saveConfig();
     byte = presetHiVolts % 256;
     saveConfig();
     byte = presetHiVolts >> 8;
     saveConfig();
     byte = inputOffset;
     saveConfig();
     byte = flags;
     saveConfig();

     // Save the two's complement of the calculated checksum.
     // When reading back the saved data, if this is summed with the
     // running checksum, the result will be zero if checksum is good
     // and non-zero if it failed.
     byte = (~checksum)+1;
     saveConfig();
}
*/
void resetToDefaults(void)
{
     presetLoVolts     = 400;    // assume fixed point at 2 decimal places
     presetHiVolts     = 600;    // like 12.60
     inputOffset       = 0;       // +/- 127
     HiVoltAuto        = true;   // Output automatic or manual
}
/*
void loadPreset (void)
{
     checksum=0;
     addr=0;
     loadConfig();
     presetLoVolts = byte;
     loadConfig();
     presetLoVolts += byte * 256;

     loadConfig();
     presetHiVolts = byte;
     loadConfig();
     presetHiVolts += byte * 256;

     loadConfig();
     inputOffset = byte;

     loadConfig();
     flags = byte;

     // this last load reads the saved checksum.
     loadConfig();
}
*/
char* codetxt_to_ramtxt(const char* ctxt){
  static char txt[20];
  char i;
  for(i =0; txt[i] = ctxt[i]; i++);

  return txt;
}