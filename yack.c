/*!
 
Some of the first generation keyers exhibited this behaviur so the chip can simulate that
 @file      yack.c
 @brief     CW Keyer library
 @author    Jan Lategahn DK3LJ jan@lategahn.com (C) 2011; modified by Jack Welch AI4SV
 
 @version   0.78
 
 This program is free software: you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the
 Free Software Foundation, either version 3 of the License, or (at your
 option) any later version.
 
 This program is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 General Public License for more details.
 
 You should have received a copy of the GNU General Public License

 Some of the first generation keyers exhibited this behaviour so the
 chip can simulate that

 
 @date      15.10.2010  - Created
 @date      03.10.2013  - Last update
 @date      2025-02-21  - Modified by Anders Helmersson, SM5KAE

*/ 

#include <avr/io.h> 
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <stdint.h>
#include "yack.h"

// Forward declaration of private functions
static      void yackkey (byte mode); 
static      void keylatch (byte lastkey);

// Enumerations

enum FSMSTATE { 
  S_DAH,   //!< Keyed, waiting for duration of current element
  S_DIT,   //!< Keyed, waiting for duration of current element
  S_IDLE   //!< Idle, waiting for the first element of the next symbol
};   

// The FSM state also includes a time, counting down. When reaching zero
// the FSM determines the next state. Also, volflags is included here.

// Module local definitions

static byte yackflags;        // Permanent (stored) status of module flags
static byte volflags = 0;     // Temporary working flags (volatile)
static word ctcvalue;         // Pitch
static word wpmcnt;           // Speed
static byte wpm;              // Real wpm

// EEPROM Data

byte magic EEMEM = MAGPAT;    // Needs to contain 'A5' if mem is valid
byte flagstor EEMEM = (IAMBA | TXKEY | SIDETONE);  //  Defaults  
word ctcstor EEMEM = DEFCTC;  // Pitch = 800Hz
byte wpmstor EEMEM = DEFWPM;  // 15 WPM
word user1 EEMEM = 0;         // User storage
word user2 EEMEM = 0;         // User storage

byte eebuffer1[100] EEMEM = {C_M, C_E, C_S, C_S, C_A, C_G, C_E, C_SPACE, C_1, 0};
byte eebuffer2[100] EEMEM = {C_M, C_E, C_S, C_S, C_A, C_G, C_E, C_SPACE, C_2, 0};

// Fibonacci series used for coding Morse symbols
// f[0) = f[1] = 1, f[2] = 2, f[3] = 3, f[n] = f[n-1] + f[n-2]  
// The last two values are set to 1 to avoid overflow


#if (NFIB == 13)
const byte f[NFIB] = {1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233};
#else
const word f[NFIB] = {1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377,
	  610, 987, 1597, 2584, 4181, 6765, 10946, 17711, 28657, 46368};
#endif

// Example
// SK ···-·- 
//           (c, i) =  (1, 0)
// dit:  c += f[i++],  (2, 1)
// dit:  c += f[i++],  (3, 2) 
// dit:  c += f[i++],  (5, 3) 
// dah:  c += f[++i], (10, 4)
//       c += f[++i], (18, 5) 
// dit:  c += f[i++], (26, 6) 
// dah:  c += f[++i],
//       c += f[++i], (81, 8) 
//
// I         (c, i) =  (1, 0)
// dit:  c += f[i++],  (2, 1)
// dit:  c += f[i++],  (3, 2) 
//
// T         (c, i) =  (1, 0)
// dah:  c += f[++i],  (2, 1)
//       c += f[++i],  (4, 2) 

// Å ·--·-
// dit   (2,1)
// dah   (4,2)
//       (7,3)
// dah  (12,4)
//      (20,5)
// dit  (28,6)
// dah  (49,7)
//      (83,8)
//
//! Morse code encoding
//!
//!  _    1,  f[1], Used for word space
//!
//!  E    2,  f[2]
//!
//!  I    3,  f[3]
//!  T    4,  f[3]+f(1]
//!
//!  S    5,  f[4]
//!  N    6
//!  A    7,  f[4]+f(2]
//!
//!  H    8,  f[5]
//!  D    9  
//!  R	 10
//!  U	 11,  f[5]+f[3]
//!  M	 12
//!
//!  5   13,  f[6]
//!  B   14
//!  L   15
//!  F   16
//!  G   17
//!  V   18,  f[6]+f[4]
//!  K   19
//!  W   20
//
//  SS   21,  f[7]
//   6   22
//  AS   23
//   É   24
//   Z   25
//  SN   26
//   C   27
//   P   28
//   4   29,  f[7]+f[5]
//   X   30
//   Ä   31
//   Ü   32
//   O   33
//
//  SH   34,  f[8]
//  T5   35
//  AH   36
//  US   37
//   7   38
//  SL   39
//   Ç   40
//  WI   41
//  HN   42
//   /   43
//   +   44
//  UN   45
//   Ö   46
//  SU   47,  f[8]+f[6]
//   =   48
//       49
//       50
//   Q   51
//   3   52
//   Y   53
//   J   54
//
//  HH   55,  f[9]
//       56
//       57
//       58
//       59
//       60
//       61
//       62
//       63
//       64
//  BN	 65
//   ?   66
//   8   67
//       68
//       69
//       70
//       71
//       72
//       73
//       74
//       75
//  HU   76,  f[9]+f[7]
//       77
//       78
//       79
//  GA   80
//  SK   81
//       80
//       81
//       82
//   Å   83
//       84
//       85
//       86
//   2   87
//  Ch   88
//   
//  H5   89,  f[10]
//
//   9  122
//  HV  123,  f[10]+f[8]
//
//   .  133
//   1  143
//
//  55  144,  f[11]
//
//  H4  199,  f[11]+f[9]
//
//  BK  209
//
//   ,  224
//
//   0  232
//
// SHH  233,  f[12],  invalid 


// Functions

// ***************************************************************************
// Control functions
// ***************************************************************************

void yackreset (void)
/*! 
 @brief     Sets all yack parameters to standard values

 This function resets all YACK EEPROM settings to their default values
 as stored in the .h file. It sets the dirty flag and calls the save
 routine to write the data into EEPROM immediately.

*/
{

  ctcvalue  = DEFCTC;                  // Initialize to 800 Hz
  wpm       = DEFWPM;                  // Init to default speed
  wpmcnt    = (12000/YACKBEAT)/DEFWPM; // default speed
  yackflags = FLAGDEFAULT;  

  volflags |= DIRTYFLAG;
  yacksave ();                         // Store them in EEPROM

}


void yackinit (void) {
/*! 
 @brief     Initializes the YACK library
 
 This function initializes the keyer hardware according to the
 configurations in the .h file. Then it attempts to read saved
 configuration settings from EEPROM. If not possible, it will reset all
 values to their defaults. This function must be called once before the
 remaining fuctions can be used.

*/
  // Configure DDR. Make OUT and ST output ports
  SETBIT (OUTDDR,  OUTPIN);    
  SETBIT (STDDR,   STPIN);
  
  // Raise internal pullups for all inputs
  SETBIT (KEYPORT, DITPIN);  
  SETBIT (KEYPORT, DAHPIN);
  SETBIT (BTNPORT, BTNPIN);
  
  byte magval = eeprom_read_byte (&magic);    // Retrieve magic value
  
  if (magval == MAGPAT) {                     // Is memory valid
    ctcvalue = eeprom_read_word (&ctcstor);   // Retrieve last ctc setting
    wpm = eeprom_read_byte (&wpmstor);        // Retrieve last wpm setting
    wpmcnt = (12000/YACKBEAT)/wpm;            // Calculate speed
    yackflags = eeprom_read_byte (&flagstor); // Retrieve last flags  
  } else {
    yackreset ();
  }  
  
  yackinhibit (OFF);

#ifdef POWERSAVE
    PCMSK |= PWRWAKE;      // Define which keys wake us up
    GIMSK |= (1 << PCIE);  // Enable pin change interrupt
#endif
    
    // Initialize timer1 to serve as the system heartbeat. CK runs at 1
    // MHz. Prescaling by 8 makes that 125 kHz. Counting 125 cycles of
    // that generates an overflow every 1.0 ms
    
    OCR1C = 124; // 125 counts per cycle
    TCCR1 |= (1 << CTC1) | 0b00000100; // Clear Timer on match, prescale ck by 8
    OCR1A = 1; // CTC mode does not create an overflow so we use OCR1A
    
}

#ifdef POWERSAVE

ISR (PCINT0_vect)
/*! 
 @brief     A dummy pin change interrupt
 
 This function is called whenever the system is in sleep mode and there
 is a level change on one of the contacts we are monitoring (Dit, Dah
 and the command key). As all handling is already taken care of by
 polling in the main routines, there is nothing we need to do here.

 */
{
  // Nothing to do here. All we want is to wake up.. 
}

void yackpower (byte n)
/*! 
 @brief     Manages the power saving mode
 
 This is called in yackbeat intervals with either a TRUE or FALSE as
 parameter. Whenever the parameter is TRUE a beat counter is advanced
 until the timeout level is reached. When timeout is reached, the chip
 shuts down and will only wake up again when issued a level change
 interrupt on either of the input pins.
 
 When the parameter is FALSE, the counter is reset.
 
 @param n   TRUE: OK to sleep, FALSE: Can not sleep now
 
*/

{
  static uint32_t shdntimer=0;
  if (n) {
    // True = we could go to sleep
    if (shdntimer++ == YACKSECS (PSTIME)) {
      shdntimer = 0; // So we do not go to sleep right after waking up
      set_sleep_mode (SLEEP_MODE_PWR_DOWN);
      sleep_bod_disable ();
      sleep_enable ();
      sei ();
      sleep_cpu ();
      cli ();
      // There is no technical reason to CLI here but it avoids hitting
      // the ISR every time the paddles are touched. If the remaining
      // code needs the interrupts this is OK to remove.
    }
  } else {
    // Passed parameter is FALSE
    shdntimer = 0;
  }
}
#endif

void yacksave (void)
/*! 
 @brief     Saves all permanent settings to EEPROM
 
 To save EEPROM write cycles, writing only happens when the flag
 DIRTYFLAG is set. After writing the flag is cleared
 
 @callergraph
 
 */
{
  if (volflags & DIRTYFLAG) {  // Dirty flag set?
    eeprom_write_byte (&magic,    MAGPAT);
    eeprom_write_word (&ctcstor,  ctcvalue);
    eeprom_write_byte (&wpmstor,  wpm);
    eeprom_write_byte (&flagstor, yackflags);
    volflags &= ~DIRTYFLAG;    // Clear the dirty flag
  }
  
}

void yackinhibit (byte mode)
/*! 
 @brief     Inhibits keying during command phases
 
 This function is used to inhibit and re-enable TX keying (if
 configured) and enforce the internal sidetone oscillator to be active
 so that the user can communicate with the keyer.
 
 @param mode   ON inhibits keying, OFF re-enables keying 
 
 */
{
  if (mode) {
    volflags &= ~(TXKEY | SIDETONE);
    volflags |= SIDETONE;
  } else {
    volflags &= ~(TXKEY | SIDETONE);
    volflags |= yackflags & (TXKEY | SIDETONE);
    yackkey (UP);
  }
}

word yackuser (byte func, byte nr, word content)
/*! 
 @brief     Saves user defined settings
 
 The routine using this library is given the opportunity to save up to
 two 16 bit sized values in EEPROM. In case of the sample main function
 this is used to store the beacon interval timer value. The routine is
 not otherwise used by the library.
 
 @param func    States if the data is retrieved (READ) or written (WRITE) to EEPROM
 @param nr      1 or 2 (Number of user storage to access)
 @param content The 16 bit word to write. Not used in read mode.
 @return        The content of the retrieved value in read mode.
 
 */
{
  if (func == READ) {
    if (nr == 1) 
      return eeprom_read_word (&user1);
    else if (nr == 2)
      return eeprom_read_word (&user2);
  }
  if (func == WRITE) {
    if (nr == 1)
      eeprom_write_word (&user1, content);
    else if (nr == 2)
      eeprom_write_word (&user2, content);
  }
  return FALSE;
}



word yackwpm (void)
/*! 
 @brief     Retrieves the current WPM speed
 
 This function delivers the current WPM speed. 

 @return        Current speed in WPM
 
 */
{
  return wpm; 
}


void yackspeed (byte dir)
/*! 
 @brief     Increases or decreases the current WPM speed
 
 The amount of increase or decrease is in amounts of wpmcnt. Those are
 close to real WPM in a 10 ms heartbeat but can significantly differ at
 higher heartbeat speeds.
 
 @param dir     UP (faster) or DOWN (slower)
 
 */
{
    
  if ((dir == UP)   && (wpm < MAXWPM)) wpm++;
  if ((dir == DOWN) && (wpm > MINWPM)) wpm--;
        
  wpmcnt = (12000/YACKBEAT+wpm/2)/wpm; // Calculate beats

  // wpm    wpmcnt
  //  10       120
  //  12       100
  //  14        86
  //  16        75
  //  18	67
  //  20	60
  //  22  	55
  //  24	50
  //  25	48
  //  30	40
  //  35	34
  //  40	30
  //  50        24

  volflags |= DIRTYFLAG; // Set the dirty flag  
    
  yackplay  (DIT);
  yackplay  (DAH);
    
}



void yackbeat (void)
/*! 
 @brief     Heartbeat delay
 
 Several functions in the keyer are timing dependent. The most prominent
 example is the yackiambic function that implements the IAMBIC keyer
 finite state machine. The same expects to be called in intervals of
 YACKBEAT milliseconds. How this is implemented is left to the user. In
 a more complex application this would be done using an interrupt or a
 timer. For simpler cases this is a busy wait routine that delays
 exactly YACKBEAT ms.
 
 */
{
  while ((TIFR & (1 << OCF1A)) == 0); // Wait for Timeout
  TIFR |= (1 << OCF1A);               // Reset output compare flag
}

void yackpitch (byte dir)
/*! 
 @brief     Increases or decreases the sidetone pitch
 
 Changes are done not in Hz but in ctc control values. This is to avoid
 extensive calculations at runtime. As is all calculations are done by
 the preprocessor.
 
 @param dir     UP or DOWN
 
 */
{
  if (dir == UP)   ctcvalue--;
  if (dir == DOWN) ctcvalue++;
  if (ctcvalue < MAXCTC) ctcvalue = MAXCTC;
  if (ctcvalue > MINCTC) ctcvalue = MINCTC;
  
  volflags |= DIRTYFLAG; // Set the dirty flag  
  
}

void yacktune (void)
/*! 
 @brief     Activates Tuning mode
 
 This produces a solid keydown for TUNEDURATION seconds. After this the
 TX is unkeyed. The same can be achieved by presing either the DIT or
 the DAH contact or the control key.
 
*/
{
  word timer = YACKSECS (TUNEDURATION);
  
  yackkey (DOWN);
  while (timer && (KEYINP & (1 << DITPIN)) 
         && (KEYINP & (1 << DAHPIN)) && !yackctrlkey (TRUE) ) {
    timer--;
    yackbeat ();
  }
  yackkey (UP);
}

byte yackmode (byte mode)
/*! 
 @brief     Sets the keyer mode (e.g. IAMBIC A)
 
 This allows to set the content of the two mode bits in yackflags.
 Currently only two modes are supported, IAMBIC A and IAMBIC B.
 
 @param mode IAMBICA or IAMBICB
 @return    TRUE is all was OK, FALSE if configuration lock prevented changes
 
 */
{
  byte oldmode = yackflags & MODE;
  yackflags &= ~MODE;
  yackflags |= (MODE & mode);
  volflags  |= DIRTYFLAG;             // Set the dirty flag  
  return oldmode;
}


byte yackflag (byte flag)
/*! 
 @brief     Query feature flags
 
 @param flag A byte which indicate which flags are to be queried 
 @return     0 if the flag(s) were clear, >0 if flag(s) were set
 
 */
{
  return yackflags & flag;
}

void yacktoggle (byte flag)
/*! 
 @brief     Toggle feature flags
 
 When passed one (or more) flags, this routine flips the according bit
 in yackflags and thereby enables or disables the corresponding feature.
 
 @param flag    A byte where any bit to toggle is set e.g. SIDETONE 
 @return    TRUE if all was OK, FALSE if configuration lock prevented changes
 
 */
{
    
  yackflags ^= flag;       // Toggle the feature bit
  volflags  |= DIRTYFLAG;  // Set the dirty flag  

}

// ***************************************************************************
// CW Playback related functions
// ***************************************************************************

static void yackkey (byte mode) 
/*! 
 @brief     Keys the transmitter and produces a sidetone
 
 but only if the corresponding functions (TXKEY and SIDETONE) have been
 set in the feature register. This function also handles a request to
 invert the keyer line if necessary (TXINV bit).
 
 This is a private function.

 @param mode    UP or DOWN
 
 */
{
  
  if (mode == DOWN) {
    if (volflags & SIDETONE) {
      // Are we generating a Sidetone?
      OCR0A = ctcvalue;    // Then switch on the Sidetone generator
      OCR0B = ctcvalue;
            
      // Activate CTC mode
      TCCR0A |= (1 << COM0B0 | 1 << WGM01);
            
      // Configure prescaler
      TCCR0B = 1 << CS01;
    }
        
    if (volflags & TXKEY) {
      // Are we keying the TX?
      if (yackflags & TXINV) // Do we need to invert keying?
        CLEARBIT (OUTPORT, OUTPIN);
      else
        SETBIT (OUTPORT, OUTPIN);
    }

  }
    
  if (mode == UP) {
    if (volflags & SIDETONE) {
      // Sidetone active?
      TCCR0A = 0;
      TCCR0B = 0;
    }
        
    if (volflags & TXKEY) {
      // Are we keying the TX?
      if (yackflags & TXINV) // Do we need to invert keying?
        SETBIT (OUTPORT, OUTPIN);
      else
        CLEARBIT (OUTPORT, OUTPIN);
    }
  }
}

void yackdel (byte n)
/*! 
 @brief     Produces an active waiting delay for n Dit counts
 
 This is used during the playback functions where active waiting is needed
 
 @param n   number of Dit durations to delay (dependent on current keying speed!
 
 */
{
  while (n--) {
    byte x = wpmcnt;
    while (x--) yackbeat ();
  }
}

void yackplay (byte i) 
/*! 
 @brief     Key the TX / Sidetone for the duration of a Dit or a Dah
 
 @param i   DIT or DAH
 
 */
{
  yackkey (DOWN); 

#ifdef POWERSAVE
  yackpower (FALSE); // Avoid powerdowns when keying
#endif
    
  switch (i) {
    case DAH:
      yackdel (DAHLEN-IEGLEN);
      break;
      
    case DIT:
      yackdel (DITLEN-IEGLEN);
      break;
  }
  yackkey (UP);
  yackdel (IEGLEN);    // Inter Element gap  

}

void yackchar (byte c)
/*! 
 @brief     Send a character in Morse code
 
 This function translates a character passed as parameter into Morse It
 then keys transmitter / sidetone with the characters elements and adds
 all necessary gaps (as if the character was part of a longer word).
 
 If the character can not be translated, nothing is sent.
 
 If a space is received, an interword gap is sent.
  
 @param c   The character to send
 
*/

{
  byte n;           // Dit counter
  char buf[NFIB]; 
  byte i = 0;       // element counter

  if (c == 0) return;

  if (c == 1) {
    yackdel (IWGLEN);
    return;
  }

  for (n = NFIB-2; n > 1; n--) {
    if (c >= f[n]) {
      c -= f[n-2];
      if (c >= f[n]) {
	c -= f[--n]; 
        buf[i++] = 1;   // Dah
      } else {
	buf[i++] = 0;   // Dit
      }
    }
  }
  while (i > 0) {
    yackplay (buf[--i] ? DAH : DIT);
  }
  yackdel (ICGLEN);
}

void yackstring (const byte *p)
/*! 
 @brief     Sends a 0-terminated string in CW which resides in Flash
 
 Reads character by character from flash, translates into CW and keys
 the transmitter and/or sidetone depending on feature bit settings.
 
 @param p   Pointer to string location in FLASH 
 
 */
{
  byte c;
  while ((c = pgm_read_byte (p++)) && !(yackctrlkey (FALSE))) yackchar (c);
  // While end of string in flash not reached and ctrl not pressed abort
  // now if someone presses command key Play the read character
}

void yacknumber (word n)
/*! 
 @brief     Sends a number in CW
 
 Transforms a number up to 65535 into its digits and sends them in CW
 
 @param n   The number to send
 
 */
{
 byte buffer[5];
 byte i = 0;
  while (n > 0) {
    buffer[i++] = n % 10; // Store remainder of division by 10
    n /= 10;              // Divide by 10
   }
  while (i > 0) {
    switch (buffer[--i]) {
      case 0: yackchar (C_0); break;
      case 1: yackchar (C_1); break;
      case 2: yackchar (C_2); break;
      case 3: yackchar (C_3); break;
      case 4: yackchar (C_4); break;
      case 5: yackchar (C_5); break;
      case 6: yackchar (C_6); break;
      case 7: yackchar (C_7); break;
      case 8: yackchar (C_8); break;
      case 9: yackchar (C_9); break;
    }
  }
  yackchar (C_SPACE);
}



// ***************************************************************************
// CW Keying related functions
// ***************************************************************************

static void keylatch (byte lastkey)
/*! 
 @brief     Latches the status of the DIT and DAH paddles
 
 If either Dit or Dah are keyed, this function sets the corresponding
 bit in volflags. This is used by the Iambic keyer to determine which
 element needs to be sounded next.
 
 This is a private function.

 */

  // TODO: include IAMBIC function: detect which key is pressed
  // first
{
  byte swap = yackflags & PDLSWAP;
  // Note dit and dah go zero when key is pressed
  byte dit = (KEYINP & (1 << (swap ? DAHPIN : DITPIN))) != 0;
  byte dah = (KEYINP & (1 << (swap ? DITPIN : DAHPIN))) != 0;

  static byte ditcnt = 0;
  static byte dahcnt = 0;

  if (ditcnt < YACKCNTS && dit) ditcnt++;
  if (ditcnt > 0 && (!dit)) ditcnt--;

  if (dahcnt < YACKCNTS && dah) dahcnt++;
  if (dahcnt > 0 && (!dah)) dahcnt--;

  if ((ditcnt >= YACKCNTS) && (lastkey & DITLATCH))
    volflags &= ~DITLATCH; 
  else if ((ditcnt <= 0) && !(lastkey & DITLATCH))
    volflags |= DITLATCH; 

  if ((dahcnt >= YACKCNTS) && (lastkey & DAHLATCH))
    volflags &= ~DAHLATCH; 
  else if ((dahcnt <= 0) && !(lastkey & DAHLATCH))
    volflags |= DAHLATCH; 
}

byte yackctrlkey (byte mode) {
/*! 
 @brief     Scans for the Control key
 
 This function is regularly called at different points in the program.
 In a normal case it terminates instantly. When the command key is found
 to be closed, the routine idles until it is released again and returns
 a TRUE return value.
 
 If, during the period where the contact was closed one of the paddles
 was closed too, the wpm speed is changed and the keypress not
 interpreted as a Control request. 

 @param mode    TRUE if caller has taken care of command key press, FALSE if not
 @return        TRUE if a press of the command key is not yet handled. 
 
 @callergraph
 
 */
  byte volbfr = volflags; // Remember current volatile settings
    
  if (!(BTNINP & (1 << BTNPIN))) {
    // If command button is pressed
    volbfr |= CKLATCH; // Set control key latch
    
    // Apparently the control key has been pressed. To avoid bouncing We
    // will now wait a short while and then busy wait until the key is
    // released. Should we find that someone is keying the paddle, let
    // him change the speed and pretend ctrl was never pressed in the
    // first place..

    yackinhibit (ON); // Stop keying, switch on sidetone.
    
    _delay_ms (50);
    
    while(!(BTNINP & (1 << BTNPIN))) {
      // Busy wait for release
            
      if (!( KEYINP & (1 << DITPIN))) {
        // Someone pressing DIT paddle
        yackspeed (UP);
        volbfr &= ~CKLATCH; // Ignore that control key was pressed
      }  
      
      if (!( KEYINP & (1 << DAHPIN))) {
        // Someone pressing DAH paddle
        yackspeed (DOWN);
        volbfr &= ~CKLATCH;
      }  
    }
    _delay_ms (50); // Trailing edge debounce  
  }

  volflags = volbfr; // Restore previous state

  if (mode == TRUE) {
    // Does caller want us to reset latch?
    volflags &= ~CKLATCH;
  }
    
  yacksave (); // In case we had a speed change
    
  return (volbfr & CKLATCH) != 0; 
  // Tell caller if we had a ctrl button press
  
}


void yackmessage (byte function, byte msgnr)
/*! 
 @brief     Handles EEPROM stored CW messages (macros)
 
 When called in RECORD mode, the function records a message up to 100
 characters and stores it in EEPROM. The routine stops recording when
 timing out after DEFTIMEOUT seconds. Recording can be aborted using the
 control key. If more than 100 characters are recorded, the error
 prosign is sounded and recording starts from the beginning. After
 recording and timing out the message is played back once before it is
 stored. To erase a message, do not key one.
 
 When called in PLAY mode, the message is just played back. Playback can
 be aborted using the command key.
 
 @param     function    RECORD or PLAY
 @param     msgnr       1 or 2
 @return    TRUE if all OK, FALSE if lock prevented message recording
 
 */
{
  byte rambuffer[RBSIZE];  // Storage for the message
#if (NFIB == 13)
  byte c;                  // Work character
#else
  word c;
#endif

  word extimer = 0;        // Detects end of message (10 sec)
  
  byte i = 0;              // Pointer into RAM buffer
  byte n;                  // Generic counter
  
  if (function == RECORD) {
    extimer = YACKSECS (DEFTIMEOUT);  // 5 Second until message end
    while (extimer--) {
      // Continue until we waited 5 seconds
      if (yackctrlkey (FALSE)) return;
      
      if ((c = yackiambic (ON))) {
        // Check for a character from the key
        rambuffer[i++] = c; // Add that character to our buffer
        extimer = YACKSECS (DEFTIMEOUT); // Reset End of message timer
      }
      
      if (i >= RBSIZE) {
        // End of buffer reached?
        yackchar (C_HH);
        i = 0;
      }
      yackbeat (); // 1 ms heartbeat
    }  
    
    // Extimer has expired. Message has ended
    
    if (i > 0) {
      // Was anything received at all?
      rambuffer[--i] = 0; // Add a \0 end marker over last space
      
      // Replay the message
      for (n = 0; n < i; n++) yackchar (rambuffer[n]);
      
      // Store it in EEPROM
      switch (msgnr) {
        case 1:
          eeprom_write_block (rambuffer, eebuffer1, RBSIZE);
	  break;
        default:
          eeprom_write_block (rambuffer, eebuffer2, RBSIZE);
	  break;
      }
    } else
      yackchar (C_HH);
  }
  
  if (function == PLAY) {
    // Retrieve the message from EEPROM
    switch (msgnr) {
      case 1:
        eeprom_write_block (rambuffer, eebuffer1, RBSIZE);
        break;
      default:
        eeprom_write_block (rambuffer, eebuffer2, RBSIZE);
        break;
    }
    
    // Replay the message
    for (n = 0; (c = rambuffer[n]) && (n < RBSIZE); n++) 
      // Read until end of message
      yackchar (c); // play it back 
  }
}

#if (NFIB == 13)
byte yackiambic (byte ctrl)
#else
word yackiambic (byte ctrl)
#endif
/*! 
 @brief     Finite state machine for the keyer
 
 This routine, which usually terminates immediately needs to be called
 in regular intervals of YACKBEAT milliseconds.
 
 This can happen though an outside busy waiting loop or a counter
 mechanism.
 
 @param ctrl    ON if the keyer should recognize when a word ends. OFF if not.
 @return        The character if one was recognized, /0 if not
 
 */
{
  static enum FSMSTATE state = S_IDLE; // FSM state indicator
  static word timer;                // A countdown timer
  static word idletimer = 0;        // A timer incremented in S_IDLE
  static byte lastkey = 0;          // The last key pressed
  static byte bcntr   = 0;          // Number of elements sent
  static byte prelatch = 0;
  const byte mode = yackflags & MODE;
#if (NFIB == 13)
  static byte buffer  = 1;          // A place to store the character
  byte retchar = 0;   // character in Fibonacci coding
#else
  static word buffer  = 1;          // A place to store the character
  word retchar = 0;
#endif

  // This routine is called every YACKBEAT 0.1 ms. It starts with idle
  // mode where the paddles are polled. Once a contact close is sensed,
  // the TX key is closed, the sidetone oscillator is fired up and the
  // FSM progresses to the next state (KEYED). There it waits for the
  // timer to expire, afterwards progressing to IEG (Inter Element Gap).
  // Once the IEG has completed, processing returns to the S_WPS state.
  
  // If the FSM remains in idle state long enough (one Dah time), the
  // character is assumed to be complete and a decoding is attempted. If
  // succesful, the ascii code of the character is returned to the
  // caller
  
  // If the FSM remains in idle state for another 4 Dit times (7 Dit
  // times altogether), we assume that the word has ended. A space char
  // is transmitted in this case.
  
  if (timer > 0) timer--;           // Count down

#ifdef POWERSAVE            
  yackpower (state == S_IDLE); // OK to go to sleep when S_IDLE
#endif

  // The following handles the inter-character gap. When there are
  // three (default) Dit lengths of space after an element, the
  // character is complete and can be returned to caller

  // This handles the inter-word gap. Already 3 Dits have been
  // waited for, if 4 more follow, interpret this as a word end

  // In the Semi-cootie mode it seems difficult to produce
  // Dah-Dit-Dah which often beomes Dah-Dah or Dah-Dit-Dit-Dah. We
  // should fix the timing of the latching to improve the
  // situation. It feels easier to key in the Iambic modes 

  // When a Dits are sent, we need to keep a equidistant sampling
  // of the paddle. Note that using a single paddle there are
  // strictly three positions of the paddle, Space (released), Dit
  // and Dah, represented by 0, 1 and 2, there is no Squeezed
  // position.

  // When a Dah is sent, an extra Dit interval is introduced, which
  // relaxes the timing. In the Semi-cootie mode, a Dah is always
  // related to a movement of the paddle. In the other modes its related
  // to pressing the Dah paddle, not necessarily related to a move.

  // The decision what to send next must be taken before the TX key is
  // to go up, that is at the end of the previous space.
  
  // It is possible to consume the extra interval directly or we can
  // spread it over the following Dits as well:

  // Pre-latch interval
  //                Dah     Dit      Dit      Dit
  // Early           1      1/2      1/4      1/8
  // Late            0       0        0        0

  // We define a prelatch interval, which is set to a value between 0
  // and 100% of a Dit, when Dah is sent. When a Dit is sent the
  // prelatch interval is halved.
  
  // The latching logic detects changes in the position of the paddle
  // compared to the previous position stored in lastkey. For instance,
  // when the paddle is released (Space) it detects Dit or Dah, and when
  // the paddle is in Dah position, it detects Dit and Space.

  // The present logic is using Late decisions. Reading the latch at the
  // end the previous space. In the early scheme we read the latch
  // earlier but still during the inter-element space
  
  // During WSPACE we need to remember the keys presssed, this also
  // includes the order of the keys being pressed: None, Dah, Dit,
  // Dah-Dit, and Dit-Dah.

  // Latching is performed during spaces and when in Iambic B, also
  // during keying. The latched state is read at the end of the
  // inter-character space. The latch is cleared at the beginning of the
  // Dit or Dah.

  /*
   * Prelatch                  <------->           <--->
   * Early                     v                   v
   * Late                              v               v
   *      +--------------------+          +----+          +----+
   *     /                      \        /      \        /      \
   *    /                        \      /        \      /
   * --+                          +----+          +----+
   *
   * Prelatch                  <------->                       <---
   * Early                     v                               v
   * Late                              v
   *      +--------------------+          +--------------------+
   *     /                      \        /                      \
   *    /                        \      /
   * --+                          +----+
   *
   *      +--------------------+                          +
   *     /                      \                        /
   *    /                        \                      /
   * --+                          +--------------------+---
   *
   * Prelatch       <-->             <->              <>
   * Early          v                v                v
   * Late              v               v               v
   *      +----+          +----+          +----+          +
   *     /      \        /      \        /      \        /
   *    /        \      /        \      /        \      /
   * --+          +----+          +----+          +----+---
   *
   *      +----+          +----+          +--------------------+
   *     /      \        /      \        /                      \
   *    /        \      /        \      /
   * --+          +----+          +----+
   *
   *      +----+          +----+                     +
   *     /      \        /      \                   /
   *    /        \      /        \                 /
   * --+          +----+          +---------------+---
   */

  if (timer >= prelatch) keylatch (lastkey);
           
  if (timer == 0) {
    if (state == S_IDLE) {
      if (bcntr > 0) {
        retchar = (buffer < f[NFIB-1]) ? buffer : 0;
        bcntr = 0;
        buffer = C_SPACE;
      } else if (ctrl && idletimer == IWGLEN * wpmcnt) {
        retchar = C_SPACE;
      };
#if (NFIB == 13)
      if (idletimer < MAX_BYTE) idletimer++;
#else
      if (idletimer < MAX_WORD) idletimer++;
#endif
    }

    // Now evaluate the latch and determine what to send next
    byte key = volflags & SQUEEZED;
    prelatch = prelatch/2;
    if (key > 0) {
      if (mode == IAMBA && key == SQUEEZED) {
        state = (state == S_DIT) ? S_DAH : S_DIT;
      } else if ((((lastkey == 0) || (mode != DACTYL)) 
            && (key & DITLATCH)) 
           || ((mode == DACTYL) && (lastkey == key))) {
        state = S_DIT;
      } else {
        state = S_DAH;
      }
      if (state == S_DIT) {
        timer = DITLEN * wpmcnt;
        if (bcntr < NFIB-2) buffer += f[bcntr++];
#if (NFIB == 13)
        else buffer = MAX_BYTE;
#else
        else buffer = MAX_WORD;
#endif
      }  else  {
        prelatch = DITLEN/2;
        timer = DAHLEN * wpmcnt;
        if (bcntr < NFIB-3) {
          buffer += f[++bcntr];
          buffer += f[++bcntr];
#if (NFIB == 13)
        } else buffer = MAX_BYTE;
#else
        } else buffer = MAX_WORD;
#endif
      }
      yackkey (DOWN);
    } else {
      prelatch = 0;
      if (state != S_IDLE) timer = ICGLEN * wpmcnt;
      state = S_IDLE;
    }
    lastkey = key;
  } 
  if (timer <= IEGLEN * wpmcnt) yackkey (UP);

  return retchar; // Nothing to return if not returned above
  
}

