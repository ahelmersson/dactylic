/*!
 
 @file      main.c
 @brief     CW Keyer application
 @author    Jan Lategahn DK3LJ jan@lategahn.com (C) 2010 modified by Jack Welch AI4SV

 This file implements a sample CW keyer application by using the yack.c
 library. It is targeted at the ATTINY45 microcontroller but can be used
 for other ATMEL controllers in the same way. Note the enclosed documentation
 for further defails.
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 
 @version   0.78
 
 @date      15.10.2010  - Created
 @date      16.12.2010  - Submitted to SVN
 @date      03.10.2013  - Last change
 @date      2025-02-20  - Modified Anders 
 
 */ 


#ifndef F_CPU
#error F_CPU undefined!! Please define in Makefile
#endif

#include <avr/io.h> 
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "yack.h"

#define PITCHREPEAT 10  // 10 e's will be played for pitch adjust
// Some texts in Flash used by the application
const byte vers[] = {C_V, C_0, C_DOT, C_7, C_8, 0};
#define PRGX C_SK
#define IMOK C_R

void pitch (void)
/*! 
 @brief     Pitch change mode
 
 This function implements pitch change mode. A series of dots is played
 and pitch can be adjusted using the paddle levers.
 
 Once 10 dots have been played at the same pitch, the mode terminates
 */
{
  word timer = PITCHREPEAT;

  while (timer) {                      // while not yet timed out
    timer--;
    yackchar (C_E);                    // play an 'e'
    
    if (!(KEYINP & (1<<DITPIN))) {     // if DIT was keyed
      yackpitch (UP);                // increase the pitch
      timer = PITCHREPEAT;
    }
    
    if (!(KEYINP & (1<<DAHPIN))) {     // if DAH was keyed
      yackpitch (DOWN);                  // lower the pitch
      timer = PITCHREPEAT;
    }
  }
}

void beacon (byte mode)
/*! 
 @brief     Beacon mode
 
 This routine can read a beacon transmission interval up to 9999 seconds
 and store it in EEPROM (RECORD mode) In PLAY mode, when called in the
 YACKBEAT loop, it plays back message 2 in the programmed interval
 
 @param mode RECORD (read and store the beacon interval) or PLAY (beacon)

 @see main
 
*/
{

  static word interval = MAX_WORD; // A dummy value that can not be reached
  static word timer;
  char i = 11;
  
  if (interval == MAX_WORD) interval = yackuser (READ, 1, 0);  
  
  if (mode == RECORD) {
    interval = 0;                   // Reset previous settings
    timer = YACKSECS (DEFTIMEOUT);
  
    yackchar (C_N);
  
    while (--timer) {  
      byte c = yackiambic (OFF);
      yackbeat ();
  
      switch (c) {
	case C_0: i = 0; break; 
	case C_1: i = 1; break; 
	case C_2: i = 2; break; 
	case C_3: i = 3; break; 
	case C_4: i = 4; break; 
	case C_5: i = 5; break; 
	case C_6: i = 6; break; 
	case C_7: i = 7; break; 
	case C_8: i = 8; break; 
	case C_9: i = 9; break; 
      }
      if (i < 11) {
        interval *= 10;
        interval += i;
        timer = YACKSECS (DEFTIMEOUT);
      }
    }
    
    if (interval >= 0 && interval <= 9999) {
      yackuser (WRITE, 1, interval); // Record interval
      yacknumber (interval);         // Playback number
    } else {
      yackchar (C_HH);
    }
  }

  if ((mode == PLAY) && (interval > 0)) {

#ifdef POWERSAVE

    // If we execute this, the interval counter is positive which means
    // we are waiting for a message playback. In this case we must not
    // allow the CPU to enter sleep mode.

    yackpower(FALSE); // Inhibit sleep mode
#endif
        
    if (timer > 0)
      timer--;             // Countdown until a second has expired
    else {
      timer = YACKSECS(1); // Reset timer
      if ((--interval) == 0) {  // Interval > 0. Did decrement bring it to 0?
        interval = yackuser (READ, 1, 0); // Reset the interval timer
        yackmessage (PLAY, 2);            // and play message 2
      } 
    }
  }
}

void commandmode (void) {
/*! 
 @brief     Command mode
 
 This routine implements command mode. Entries are read from the paddle
 and interpreted as commands.
 
*/
  
  byte success = FALSE;

  word timer;          // Exit timer
  
  byte mode = yackmode (DACTYL);

  yackinhibit (ON);    // Sidetone = on, Keyer = off
  
  yackchar (C_R);      // Play Greeting
  
  timer = YACKSECS (DEFTIMEOUT); // Time out after 10 seconds
    
  while ((yackctrlkey (TRUE) == 0) && (timer-- > 0)) {
    byte c = yackiambic (OFF);

    if (c) timer = YACKSECS (DEFTIMEOUT);   // Reset timeout if character read
    yackbeat ();

    if (!yackflag (CONFLOCK)) {
      switch (c) {             // These are the lockable configuration commands

        case  C_R: // Reset
          yackreset ();
          success = TRUE;
          break;
                    
        case  C_A: // iambic A
          mode = IAMBA;
          success = TRUE;
          break;
                    
        case  C_B: // iambic B
          mode = IAMBB;
          success = TRUE;
          break;

        case C_L: // ultimatic
          mode = ULTIM;
          success = TRUE;
          break;
                    
        case C_E: // dit priority
          mode = DITPR;
          success = TRUE;
          break;
                  
        case C_T: // dah priority
          mode = DAHPR;
          success = TRUE;
          break;
                  
	case C_D: // dactylic mode
          mode = DACTYL;
          success = TRUE;
          break;
                  
        case C_X: // Paddle swapping
          yacktoggle (PDLSWAP);
          success = TRUE;
          break;
                    
        case C_J: // Sidetone toggle
          yacktoggle (SIDETONE);
          success = TRUE;
          break;
                    
        case C_K: // TX keying toggle
          yacktoggle (TXKEY);
          success = TRUE;
          break;
                    
        case C_I: // TX level inverter toggle
          yacktoggle (TXINV);
          success = TRUE;
          break;
                    
        case C_1: // Record Macro 1
          yackchar (C_1);
          yackmessage (RECORD, 1); 
          success = TRUE;
          break;
                    
        case C_2: // Record Macro 2
          yackchar (C_2);
          yackmessage (RECORD, 2); 
          success = TRUE;
          break;
                    
        case C_N: // Automatic Beacon
          beacon (RECORD);
          success = TRUE;
          break;
      }
    }
        
    switch (c) { // Commands that can be used anytime
             
      case C_V: // Version
        yackstring (vers);
        success = TRUE;
        break;
    
      case C_Z: // Pitch
        pitch();
        success = TRUE;
        break;
              
      case C_T: // Tune
        yackinhibit (OFF);
        yacktune ();
        yackinhibit (ON);
        success = TRUE;
        break;
                
      case C_0: // Lock changes
        yacktoggle (CONFLOCK);
        success = TRUE;
        break;
                
      case C_S: // Playback Macro 1
        yackinhibit (OFF);
        yackmessage (PLAY, 1);
        yackinhibit (ON);
        timer = YACKSECS (MACTIMEOUT);
        success = TRUE;
        break;
                
      case C_U: // Playback Macro 2
        yackinhibit (OFF);
        yackmessage (PLAY, 2);
        yackinhibit (ON);
        timer = YACKSECS (MACTIMEOUT);
        success = TRUE;
        break;
                
      case C_N: // Automatic Beacon
        beacon (RECORD);
        success = TRUE;
        break;
                
      case C_Q: // Query WPM
        yacknumber (yackwpm ());
        success = TRUE;
        break;
    }
        
    if (success) {
      break;
    } else if (c > 0) {
      yackchar (c);
      yackdel (IWGLEN);
      yackchar (C_QUEST);
      break;
    }
  }
  if (mode != yackmode (mode)) yacksave ();
  yackchar (PRGX);        // Sign off
  yackinhibit (OFF);      // Back to normal mode
}

int main (void) 
/*! 
 @brief     Trivial main routine
 
 Yack library is initialized, command mode is entered on request and
 both beacon and keyer routines are called in 10 ms intervals.
 
 @return Not relevant
*/
{
  yackinit ();              // Initialize YACK hardware
  yackinhibit (ON);         // side tone greeting to confirm the unit is alive
  yackdel (IWGLEN);
  yackchar (IMOK);
  yackinhibit (OFF);
  
  while (TRUE) {            // Endless core loop of the keyer app
    // If command key pressed, go to command mode
    if (yackctrlkey (TRUE)) commandmode ();
    yackbeat ();
    beacon (PLAY);          // Play beacon if requested
    yackiambic (OFF);
  }
  return 0;
}
