/* ********************************************************************
 Program  : yack.h
 Author   : Jan Lategahn DK3LJ modified by Jack Welch AI4SV
 Purpose  : definition of keyer hardware
 Created  : 15.10.2010
 Update   : 03.10.2013
 Update   : 2025-02-20  - Modified by Anders SM5KAE
 Version  : 0.77
 
 Changelog
 ---------
 Version    Date    Change
 ----------------------------------------------------------------------
 
 Todo
 ----
 
 *********************************************************************/

// User configurable settings

// The following settings define the hardware connections to the keyer
// chip

// Definition of where the keyer itself is connected 
#define KEYDDR  DDRB
#define KEYPORT PORTB
#define KEYINP  PINB
#define DITPIN  3
#define DAHPIN  4

// Definition of where the transceiver keyer line is connected
#define OUTDDR  DDRB
#define OUTPORT PORTB
#define OUTPIN  0

// Definition of where the sidetone output is connected (beware, this is
// chip dependent and can not just be changed at will)

#define STDDR   DDRB
#define STPORT  PORTB
#define STPIN   1

// Definition of where the control button is connected
#define BTNDDR  DDRB
#define BTNPORT PORTB
#define BTNINP  PINB
#define BTNPIN  2

// The following defines the meaning of status bits in the yackflags and
// volflags global variables

// Definition of the yackflags variable. These settings get stored in
// EEPROM when changed.

#define NFIB 13

#define CONFLOCK    0b00000001  // Configuration locked down
#define MODE        0b00001110  // 3 bits to define keyer mode (see next section)
#define SIDETONE    0b00010000  // Set if the chip must produce a sidetone
#define TXKEY       0b00100000  // Set if the chip keys the transmitter
#define TXINV       0b01000000  // Set if TX key line is active low
#define PDLSWAP     0b10000000  // Set if DIT and DAH are swapped

#define IAMBA       0b00000000  // Iambic A mode
#define IAMBB       0b00000010  // Iambic B mode
#define ULTIM       0b00000100  // Ultimatic Mode
#define DITPR       0b00001000  // Always give DIT priority
#define DAHPR       0b00001010  // Always give DAH priority
#define DACTYL      0b00001110  // Dactylic mode

#define FLAGDEFAULT DACTYL | TXKEY | SIDETONE

// Definition of volflags variable. These flags do not get stored in EEPROM.
#define DITLATCH    0b00000001  // Set if DIT contact was closed
#define DAHLATCH    0b00000010  // Set if DAH contact was closed
#define SQUEEZED    0b00000011  // DIT and DAH = squeezed
#define DIRTYFLAG   0b00000100  // Set if cfg data was changed and needs storing
#define CKLATCH     0b00001000  // Set if the command key was pressed at some point
#define VSCOPY      0b00110000  // Copies of Sidetone and TX flags from yackflags

// The following defines timing constants. In the default version the
// keyer is set to operate in 10 ms heartbeat intervals. If a higher
// resolution is required, this can be changed to a faster beat

// YACK heartbeat frequency (in 0.1 ms)
#define YACKBEAT    10
#define YACKSECS(n) (n*(10000/YACKBEAT)) // Beats in n seconds
#define YACKMS(n)   (n*(10/YACKBEAT))    // Beats in n ms
#define YACKCNTS   2                     // counts number of samples to
					 // swap dit/dah

// Power save mode
#define POWERSAVE    // Comment this line if no power save mode required
#define PSTIME 30    // 30 seconds until automatic powerdown
#define PWRWAKE ((1<<PCINT3) | (1<<PCINT4) | (1<<PCINT2)) // Dit, Dah or Command wakes us up..

// These values limit the speed that the keyer can be set to
#define MAXWPM 50  
#define MINWPM  6
#define DEFWPM 16

#define WPMCALC(n) ((12000/YACKBEAT)/n) // Calculates number of beats in a dot 

#define IEGLEN 1  // Length of a inter-element gap, which is included in
		  // DITLEN and DAHLEN
#define DITLEN 2  // Length of a dit
#define DAHLEN 4  // Length of a dah
#define ICGLEN 2  // Length of inter-character gap
#define IWGLEN 4  // Additional Length of inter-word gap

// Duration of various internal timings in seconds
#define TUNEDURATION 20  // Duration of tuning keydown (in seconds)
#define DEFTIMEOUT    5  // Default timeout 5 seconds
#define MACTIMEOUT   15  // Timeout after playing back a macro

// The following defines various parameters in relation to the pitch of the sidetone

// CTC mode prescaler.. If changing this, ensure that ctc config
// is adapted accordingly
#define PRESCALE  8
#define CTCVAL(n) ((F_CPU/n/2/PRESCALE)-1) // Defines how to compute CTC setting for
                                                     // a given frequency

// Default sidetone frequency
#define DEFFREQ  800     // Default sidetone frequency
#define MAXFREQ 1500     // Maximum frequency
#define MINFREQ  400     // Minimum frequenc

#define MAXCTC  CTCVAL(MAXFREQ) // CTC values for the above three values
#define MINCTC  CTCVAL(MINFREQ) 
#define DEFCTC  CTCVAL(DEFFREQ)

// The following are various definitions in use throughout the program
#define RBSIZE 100     // Size of each of the two EEPROM buffers

#define MAGPAT 0xa5    // If this number is found in EEPROM, content assumed valid

#define SPC    3
#define DIT    1
#define DAH    2

#define UP     1
#define DOWN   2

#define ON     1
#define OFF    0

#define RECORD 1
#define PLAY   2

#define READ   1
#define WRITE  2

#define TRUE   1
#define FALSE  0

#define C_SPACE   1

#define C_A       7
#define C_B      14
#define C_C      27
#define C_D       9
#define C_E       2
#define C_F      16
#define C_G      17
#define C_H       8
#define C_I       3
#define C_J      54
#define C_K      19
#define C_L      15
#define C_M      12
#define C_N       6
#define C_O      33
#define C_P      28
#define C_Q      51
#define C_R      10
#define C_S       5
#define C_T       4
#define C_U      11
#define C_V      18
#define C_W      20
#define C_X      30
#define C_Y      53
#define C_Z      25
#define C_EE     24
#define C_AE     30
#define C_AA     31
#define C_UE     32
#define C_HH     55

#define C_0     232
#define C_1     143
#define C_2      87
#define C_3      52
#define C_4      29
#define C_5      13
#define C_6      22
#define C_7      38
#define C_8      67
#define C_9     122

#define C_SLASH  43
#define C_PLUS   44
#define C_AS     61
#define C_QUEST  66
#define C_SK     81
#define C_DOT   133
#define C_BK    209

// Generic functionality
#define SETBIT(ADDRESS,BIT)     (ADDRESS |= (1<<BIT))
#define CLEARBIT(ADDRESS,BIT)   (ADDRESS &= ~(1<<BIT))

typedef uint8_t  byte;
typedef uint16_t word;

#define MAX_WORD 65535
#define MAX_BYTE 255

// Forward declarations of public functions
void yackinit (void);
void yackchar (byte c);
void yackstring (const byte *p);
#if (NFIB == 13)
byte yackiambic (byte ctrl);
#else
word yackiambic (byte ctrl);
#endif
void yackpitch (uint8_t dir);
void yacktune (void);
byte yackmode (uint8_t mode);
void yackinhibit (uint8_t mode);
void yacktoggle (byte flag);
byte yackflag (byte flag);
void yackbeat (void);
void yackmessage (byte function, byte msgnr);
void yacksave (void);
byte yackctrlkey (byte mode);
void yackreset (void);
word yackuser (byte func, byte nr, word content);
void yacknumber (word n);
word yackwpm (void);
void yackplay (byte i);
void yackdel (byte n);
void yackspeed (byte dir);

#ifdef POWERSAVE
void yackpower (byte n);
#endif
