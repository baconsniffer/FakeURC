
#include <EEPROM.h>
#include <memorysaver.h>
#include <UTFT.h>
#include "URCFont.c"

#define BAUD  1200                  //Change to 1200 for production code
#define EEPROMSTART 256
#define SIG1  '5'
#define SIG2  'A'					//Set to other than 'A' to force re-config
#define SER_TIMEOUT 1000			//Serial read timeout (2000 is a guess)
#define ACK   6						//Response codes from radio
#define NAK   21					//The radio sends one of these three codes
#define HT    9						//after the response to all commands (even invalid ones)

// Declare which fonts we will be using
extern uint8_t SmallFont[];         //Almost everything
extern uint8_t BigFont[];           //Splash screen
//extern uint8_t URCLCDFont[];   //Frequency display
extern uint8_t SevenSegNumFont[];   //Frequency display

//
// ------------------------------------------------------------
// Global variables
//
struct _presetStruct {
  char  TxFreq[8]; 
  char  RxFreq[8];
  char  Mode = '0';
  char  TxMode = '0';
  char  scanList = '0';
  char  power = '0';
  char  cipher = '0';
} ;

_presetStruct Channel[10];
char      CurrentPreset;              //The current preset displayed
uint8_t   SQLpot;                     //value of the squelch pot
uint8_t   SQLlevel;                   //current programmed squelch level
uint8_t   SQLstate = true;            //is the radio squelched or not (true = squelched)
char      Lamp;                       //Lamp intensity, 0 = off, 3 = bright
char      NVLamp;                     //Lamp intensity saved in NVRAM (EEPROM)
char      CurrentPwr;                 //Power setting of current channel
char      CurrentMode;                //AM/FM, 0 = AM, 1 = FM
char      CurrentTxMode;              //Tx mode, 0 = AM, 1 = FM
char      cipherMode;                 //cipher or plain-text mode, 0 = plain, 1 = cipher
char      scanMode;                   //'1' = receiver in scan mode, '0' = not
char      beaconMode;                 //'1' = radio in beacon mode
char      keyed;                      //'1' = Tx keyed (RF is emitting), '0' = Rx (no RF radiated)
char      spkr;                       //Current state of speaker, '1' = enabled, '0' = muted
char      NVspkr;                     //Speaker state saved in NVRAM (EEPROM), default = '0'


// End Global variables
// ------------------------------------------------------------
int       eeAddr = 256;               //EEPROM address to start reading from

// ------------------------------------------------------------
//  Initialize the LCD
//
// Set the pins to the correct ones for your development shield
// ------------------------------------------------------------
// Standard Arduino Uno/2009 shield            : <display model>,A5,A4,A3,A2
// Standard Arduino Mega/Due shield            : <display model>,38,39,40,41
//UTFT LCD(ITDB32S,38,39,40,41);
//UTFT LCD(TYPE    ,RS,WR,CS,RST);
UTFT LCD(ILI9341_16,38,39,40,41);

// ------------------------------------------------------------

//  Function prototypes
void splash(void);                //Show splash screen on startup
void createDefaults(void);        //Create a default config
void loadConfig(void);            //Get URC state from flash or create default state
void storeConfig(void);           //Save the URC state in EEPROM
void setConfig(char preset);      //Set the active parameters based on the current preset
void drawLabels();                //Draw the outlines of the controls
void drawControls(void);          //Draw all the various widgets

int  processCommand(void);        //process command received via serial

void showChannel(void);           //Show the current channel preset
void showSqLevel(void);           //Show squelch level (0 - 255)
void showSql(void);               //Indicate receiver squelched or not (SQL/UNSQL)
void showPwr(void);               //Show current power output
void showSpkr(void);              //Indicate speaker enabled/disabled
void showMode(void);              //Show AM/FM transmit mode
void showTxMode(void);            //Show Tx mode (may be different from Rx)
void showRxTx(void);              //Indicate freq displayed is Rx or Tx
void showKey(void);               //Indicate if Tx active
void showCTPT(void);              //Indicate plain/cipher
void showScan(void);              //Indicate whether or not radio is in SCAN mode
void showOnScan(void);            //Indicate if current channel on scan list
void showBeacon(void);            //Indicate whether or not radio is in BEACON mode

void lblInBuf(void);              //Draw a box for displaying the Serial input buffer
void lblOutBuf(void);             //Draw a box for displaying the Serial output buffer
void lblSqLevel(void);            //Draw a box showing current squelch level
void lblSql(void);                //Draw a box to show current squelch state (SQL/UNSQL)
void lblPwr(void);                //Draw a box to show current power level
void lblSpkr(void);               //Draw a box to show current spkr state (enabled/disabled)
void lblMode(void);               //Draw a box showing the current Rx mode (AM/FM)
void lblTxMode(void);             //Draw a box showing the current Tx mode (AM/FM)
void lblRxTx(void);               //Draw a box indicating displayed freq is Rx or Tx
void lblKey(void);                //Draw a round button showing RF state (green = RX, red = TX)
void lblCTPT(void);               //Draw a box to show cipher state
void lblScan(void);               //Draw a box to show scan mode active/inactive
void lblOnScan(void);             //Draw a box to show scan list status for current channel
void lblBeacon(void);             //Draw a box to show beacon mode (active/inactive)

void setup() {
// Setup the LCD
  LCD.InitLCD(LANDSCAPE);

  LCD.setFont(SmallFont);
// Setup the Serial port
  Serial.begin(BAUD, SERIAL_8N1);

  while(!Serial) {
    //Wait for serial port to be available
  }
// Seed random number generator
randomSeed(analogRead(0));

// Show splash screen with delay
   splash();
   
// Get the radio state from flash (if present)
// otherwise, create defaults based on a MODE-Power-On of a real
// URC-200 (see documentation)
  loadConfig();

// Discard any spurious serial input
  Serial.flush();
}

void loop() {

// Clear the screen and draw the frame
  LCD.clrScr();

  //drawLabels();
  
  drawControls();

  while (1)
  {
	if (Serial.available())		//See if a command arrived
	{
	  if (processCommand())		//Process it
		drawControls();			//Update the screen if command succeeded
	} 
	//Potentially check for touch and process 
	//Allows for simulation of local operation of the radio
  };
}

//  void splash(void)
//  show a little splash screen on startup
void splash(void)
{
  LCD.setFont(BigFont);
  LCD.clrScr();
  LCD.setColor(VGA_WHITE);        //Print some white text on a black background
  LCD.setBackColor(VGA_BLACK);
  
  LCD.print("URC-200 Simulator", CENTER, 115);

  LCD.setFont(BigFont);
  delay(2000);
}

// Check for a configuration in EEPROM and load it if found,
// otherwise, generate a default config and store it in EEPROM
void loadConfig(void)
{
  eeAddr = EEPROMSTART;
  char  sig1, sig2;
  int   i;
  EEPROM.get(eeAddr++, sig1);
  EEPROM.get(eeAddr++, sig2);

  LCD.clrScr();
  
  if ((sig1 == SIG1) && (sig2 == SIG2))
  { //valid signature found, load structure from EEPROM
    //
    LCD.print("Reading EEPROM", CENTER, 115);
    delay(1000);
    
    EEPROM.get(eeAddr, Channel);
    eeAddr+=sizeof(Channel);
    EEPROM.get(eeAddr++, CurrentPreset);
    EEPROM.get(eeAddr++, SQLpot);
    EEPROM.get(eeAddr++, SQLlevel);
    EEPROM.get(eeAddr++, SQLstate);
    EEPROM.get(eeAddr++, Lamp);
	  EEPROM.get(eeAddr++, CurrentPwr);
	  EEPROM.get(eeAddr++, CurrentMode);
	  EEPROM.get(eeAddr++, CurrentTxMode);
	  EEPROM.get(eeAddr++, cipherMode);
	  EEPROM.get(eeAddr++, scanMode);
	  EEPROM.get(eeAddr++, NVLamp);
	  EEPROM.get(eeAddr++, NVspkr); 
  } else {  //Signature not found. Create default structures and place in storage
    //(225 MHz, PT, AM, low power
    LCD.print("Setting defaults", CENTER, 115);
    createDefaults();					//Initialize freq and mode
    storeConfig();
    delay(1000);
  }

  beaconMode = '0';						//not in beacon mode
  keyed = '0';							//Transmitter is silent
  Lamp = NVLamp;						//Set lamp intensity to stored value
  spkr = NVspkr;						//Set speaker state to stored value
  
  //set current values based on stored preset
}

void storeConfig(void)
{
	eeAddr = EEPROMSTART;
	EEPROM.put(eeAddr++, SIG1);
	EEPROM.put(eeAddr++, SIG2);
	EEPROM.put(eeAddr, Channel);
	eeAddr += sizeof(Channel);
	EEPROM.put(eeAddr++, CurrentPreset);
	EEPROM.put(eeAddr++, SQLpot);
	EEPROM.put(eeAddr++, SQLlevel);
	EEPROM.put(eeAddr++, SQLstate);
	EEPROM.put(eeAddr++, Lamp);
	EEPROM.put(eeAddr++, CurrentPwr);
	EEPROM.put(eeAddr++, CurrentMode);
	EEPROM.put(eeAddr++, CurrentTxMode);
	EEPROM.put(eeAddr++, cipherMode);
	EEPROM.put(eeAddr++, scanMode);
	EEPROM.put(eeAddr++, Lamp);
	EEPROM.put(eeAddr++, spkr);
}

//Set the active parameters based on the current preset
void setConfig(char preset)
{
  if ((preset < '0') || (preset > '9'))
    return;

  CurrentPwr    = Channel[preset].power;     //from current preset
  CurrentMode   = Channel[preset].Mode;
  CurrentTxMode = Channel[preset].TxMode;
  cipherMode    = Channel[preset].cipher;

}

//Create a default config
//Called when no config stored in NVRAM (EEPROM) or I is issued to Initialize a default config
void createDefaults(void)                //Create a default config
{
    int i = 0;
  
    for (i=0; i<10; i++) {
      strcpy(Channel[i].TxFreq, "2251234");     //"2250000"
      strcpy(Channel[i].RxFreq, "1324321");
      Channel[i].Mode = '0';            //AM modulation
      Channel[i].TxMode = '0';          //AM modulation
      Channel[i].scanList = '0';        //Not on scan list (unverified)
      Channel[i].power = '0';           //Low power
      Channel[i].cipher = '0';          //not cipher's
    }
    //Need to verify what a real URC-200 defaults to
    CurrentPreset = '0';
    CurrentPwr = '0';
    CurrentMode = '0';
    CurrentTxMode = '0';
    cipherMode = '0';
    scanMode = '0';
    Lamp = NVLamp = '0';    //Default is lamp off
    spkr = NVspkr = '0';    //Default is speaker disabled
}

void showChannel()
{
  char preset = CurrentPreset;
  //Make some space for the colours of the display
  int     f, b;
  //Save the current colors
  int       curFC = LCD.getColor();
  int       curBC = LCD.getBackColor();
  //Get the current preset as in integer for index into the Channel structure
  int       i = preset - '0';
  //Loop counter
  int       j = 0;
  //Save the current font to put back at the end
  int       fnt = LCD.getFont();
  //A buffer (may not be needed)  
  char      ch[15];
  //Null it
  memset(ch, 0, sizeof(ch));

  //Use the 7-segment font to mimic the actual display
  LCD.setFont(URCLCDFont);

  //Determine background color based on lamp intensity
  switch (Lamp)
  {
    case  '0': {
      f = VGA_WHITE;
      b = VGA_BLACK;
      break;
    }
    case  '1': {
      f = VGA_BLACK;
      b = VGA_OLIVE;
      break;
    }
    case  '2': {
      f = VGA_BLACK;
      b = VGA_GREEN;
      break;
    }
    case  '3': {
      f = VGA_BLACK;
      b = VGA_LIME;
    }
  }

  //Draw a box around the frequency
  LCD.setColor(b);
  LCD.setBackColor(b);
  LCD.fillRect(1,150, 319, 209);
  
  //Set the colours for displaying the text
  LCD.setColor(f);
  LCD.setBackColor(VGA_TRANSPARENT);
  
  //Build the frequency
  //Start with the preset + a colon
  sprintf(ch, "%1c:", preset);

  //Show Rx or Tx freq depending on transmitter state
  if (keyed == '0')   //Show Rx freq if not keyed
     strncat(ch, Channel[i].RxFreq, sizeof(Channel[i].RxFreq));
  else //Show Tx freq
     strncat(ch, Channel[i].TxFreq, sizeof(Channel[i].TxFreq));
     
  //Move the last 4 digits right one space and add a decimal
  for (j = 5; j >= 0; j--)
  {
    ch[j + 5] = ch[j + 4];
  }

  //Insert a colon in place of a decimal for now
  ch[5] = ':';
  
  //Display the frequency
  LCD.print(ch, 2, 154);
  
  //Restore the drawing color
  LCD.setColor(curFC);
  LCD.setBackColor(curBC);
  //Restore the font
  LCD.setFont(fnt);
}


int processCommand(void)
{
   int      rs = false;			    //Return status
   char     outBuf[64];         //Output buffer containing response
   char     inBuf[64];          //Input buffer - command read
   char     ch;                 //Next available character read from serial port
   char     c0;                 //Question mark command, sub-command MSB
   char     c1;                 //Question mark command, sub-command LSB
   byte     br = 0;             //Number of bytes read from input
   //char     rc = NAK;         //Response code to terminate response to host
   int      v = 0;              //General purpose value return by sub-functions
   byte     w = 0;              //General purpose value - byte sized
   int      fnt = LCD.getFont();        //Save font and colours since
   int      curFC = LCD.getColor();     // are writing to the screen
   int      curBC = LCD.getBackColor(); // and want to restore

   LCD.setFont(SmallFont);
   
   memset(outBuf, 0, sizeof(outBuf));     //Zero buffers
   memset(inBuf, 0, sizeof(inBuf));
   
   Serial.setTimeout(SER_TIMEOUT);
   
   while (Serial.available())
   {
      br = Serial.readBytes(inBuf, 15 );      //Read characters from serial port (actual input is normally less than 8 characters)
	   
	    if (!br)
	       return rs;						//If no chars read, return error

      //Display the input buffer on the display
      LCD.print(inBuf, 5, 219);
   
      ch = inBuf[0];						//Get the command
	   
      switch (ch) { 
        case 'Z':	{             //Zap command
          rs = true;						//Command is valid
          break;
        } // case 'Z'
			 
        case '$':	{             //Squelch control, next 3 chars are value
          if (br < 4) {					//See if sufficient bytes read
            break;
          } else {
            rs = true;
            v = 100 * (inBuf[1] - '0');
            v += 10 * (inBuf[2] - '0');
            v += (inBuf[3] - '0');        //Assemble new value
            SQLlevel = v;				//Set the squelch level
            break;
          }
        } //case '$'
			 
        case 'I': {						    //Initialize channels to default values
          rs = true;					    //Command is valid
			    createDefaults();				//Initialize freq and mode
			    storeConfig();
			    break;
			  } // case 'I'
			  
        case 'L': {               //Set lamp intensity
          if (isDigit(inBuf[1])) {          //If the 2nd char is digit
            w = inBuf[1];
            switch (w) {
              case '0':
              case '1':
              case '2':
              case '3': {
                rs = true;				//Command is valid
                Lamp = w;         //Set the lamp intensity
                break;
              } // Case
            } // switch(w)
          } //if
          break;
        } // case 'L'
			  
        case 'J':  {                     //Speaker on/off
          w = inBuf[1];
          switch (w) {
            case '0':
            case '1': {
              spkr = w;
              rs = true;          //Command is valid
            } //case '0','1'
          } //switch w
          break;
		    } // case 'J'
			
        case 'P': {						//Select current preset
          if (isDigit(inBuf[1])) {
            CurrentPreset = inBuf[1];
            rs = true;				//Command is valid
          }
          break;
        } // case 'P'
			 
        case 'R':	{					//Set receive freq for current preset
				  // More input validation is required for this routine
          Serial.print('"');
          Serial.print(inBuf);
          Serial.println('"');
          v = strlen(inBuf) - 1;		//Number of digits to copy
          if (v == 6) {				//Must be 6
            rs = true;				//Command is valid
            strncpy(Channel[(CurrentPreset - '0')].RxFreq, inBuf + 1, v);
          }
          break;
        } // case 'R'
			
        case 'T':	{					//Set transmit freq for current preset
				// More input validation is required for this routine
          v = strlen(inBuf) - 1;		//Number of digits to copy
          if (v == 6) {				//Must be 6
            rs = true;				//Command is valid
            strncpy(Channel[(CurrentPreset) - '0'].TxFreq, inBuf + 1, v);
          }
          break;
        } // case 'T'
			
        case 'M':	{					//Set modulation for Tx & Rx
          ch = inBuf[1];				//Get the mode
          if ((ch == '0') || (ch == '1')) {
            rs = true;				//Command is valid
            CurrentMode = ch;
            Channel[(CurrentPreset) - '0'].Mode = ch;
          }
          break;
        } // case 'M'
			
        case 'N':	{					//Set the Tx modulation mode
          ch = inBuf[1];				//Get the mode
          if ((ch == '0') || (ch == '1')) {
            rs = true;				//Command is valid
            CurrentTxMode = ch;
            Channel[CurrentPreset - '0'].TxMode = ch;
          }
          break;
        } // case 'N'
			
        case 'X':	{					//Select Plain Text (PT) or Cipher Text (CT)
          ch = inBuf[1];				//Get the mode
          if ((ch == '0') || (ch == '1')) {
            rs = true;
            cipherMode = ch;		//'0' = plain text
            Channel[CurrentPreset - '0'].cipher = ch;
          }
          break;
        } // case 'X'
			
        // Store entered date in EEPROM for current channel
        // Current setting of Rx mode, TxMode and cipherMode are
        // copied into the current preset, overwriting the old values.
        case 'Q':	{					//Store function
          rs = true;
          v = (CurrentPreset - '0');	//Get index into structure
          Channel[v].power = CurrentPwr;
          Channel[v].Mode = CurrentMode;
          Channel[v].TxMode = CurrentTxMode;
          storeConfig();
          break;
        }	// case 'Q'
			
        case 'C':	{					//Set/un-set current preset as member of scan list
          ch = inBuf[1];				//Get the flag
          if ((ch == '0') || (ch == '1')) {
            rs = true;
            Channel[CurrentPreset - '0'].scanList = ch;	//Set/un-set
          }
          break;
        }	// case 'C'

        case '#':	{					//Set power level for current preset
        // More input validation is required as not all freq/power/modulation combos supported
          ch = inBuf[1];				//Get the power level
          if ((ch == '0') || (ch == '1') || (ch == '2')) {
            rs = true;
            CurrentPwr = ch;		//Set current power
            Channel[CurrentPreset - '0'].power = ch;	//Store in preset
          }
          break;
        }	// case '#'

        case 'S':	{					//Enter/exit scan mode
          ch = inBuf[1];				//Get the mode
          if ((ch == '0') || (ch == '1')) {
            rs = true;
            scanMode = ch;			//Place into or exit from scan mode
          }
          break;
        }	//	case 'S'

        case '*':	{					//Beacon mode
          ch = inBuf[1];				//Get the mode
          if ((ch == '0') || (ch == '1')) {
            rs = true;
            beaconMode = keyed = ch;			//Place into or exit from scan mode
          }
          break;
        }	//	case '*'

        case 'K': {						//Perform self-calibration (always returns success)
          rs = true;
          break;
        } //Case 'K'

        case '+':	{					//Enter keypad mode (always return success)
          rs = true;
          break;
        }	// case '+'

        case 'B':	{					//Place radio in transmit mode
          rs = true;
          keyed = '1';				//Indicate Tx
          break;
        } // case 'B'

        case 'E':	{					//Place radio in receive mode
          rs = true;
          keyed = '0';				//Indicate Rx
          break;
        } // case 'E'
			
        case 'e':	{					//Set and store speaker and backlight (current and saved)
          ch = inBuf[1];				//Decide if speaker or lamp is being set
          if (ch == '0')	{			//"00" or "01" mean speaker
            ch = inBuf[2];			//"0" speaker disabled, "1" speaker enabled
            switch (ch)	{
              case '0':
              case '1':	{
                rs = true;
                spkr = NVspkr = ch;	//set speaker state
                storeConfig();		//NV parameters modified
                break;
              }	// case '0', '1'
            } // switch (ch) inBuf[2]
          } else if (ch == '1'){		//"10", "11", "12" and "13" are lamp
            ch = inBuf[2];			//"0" - "3" are intensity
            switch (ch) {
              case '0':
              case '1':
              case '2':
              case '3':	{
                rs = true;
                Lamp = NVLamp = ch;	//set lamp intensity
                storeConfig();		//NV parameters modified
                break;
              }	//	case '0' - '3'
            }	// switch (ch) inBuf[2]
          } // if
          break;
        }	// case 'e'

        case '?': {
          c0 = inBuf[1];               // Get sub-command
          c1 = inBuf[2];

          if (c0 == '0') {
            if (c1 == '0') {			   // PL option and status
              rs = true;
              strcat(outBuf, "A0P0067067V00R00001");	// A real URC200 response
            } else if (c1 == '1') {      // Synth lock/unlock (always respond as locked)
              rs = true;
              strcpy (outBuf, "A1");
            } else if (c1 == '2') {      // Channel scan detect - respond with detected channel
              rs = true;
              strcpy(outBuf, "QN");     // For now, say no channel detected
            } else if (c1 == '3') {      // Return received signal strength (000-255) (random)
              rs = true;
              v = random(0, 256);       // Random number between 0 and 255
              // Can re-use inBuf at this point
              memset(inBuf, 0, sizeof(inBuf));
              sprintf(inBuf, "%03d", v);  // Create string representation padded with zero's
              outBuf[0] = 'N';          // Response string begins with this
              strncat(outBuf, inBuf, strlen(inBuf));  // Followed by the value
            } else if (c1 == '4') {      // Return calibration status (always say Tuned)
              rs = true;
              strcpy(outBuf, "H1");
            } else if (c1 == '5') {      // Power supply status (returns a static string)
              rs = true;
              strcpy(outBuf, "V518125507121278650"); // From real URC200
            } else if (c1 == '6') {      // Return VFWD value (static value "000")
              rs = true;
              strcpy(outBuf, "Z000");
            } else if (c1 == '7') {      // Return VRFD value (static value "005")
              rs = true;
              strcpy(outBuf, "I005");
            } else if (c1 == '8') {      // Return the sw version (static value "VC 98-P41135F Ver01 Jun 03 1999 07:49:10")
              rs = true;
              strcpy(outBuf, "VC 98-P41135F Ver01 Jun 03 1999 07:49:10");
            } else if (c1 == '9') {      // Return squelch pot setting
              rs = true;
              // Can re-use inBuf at this point
              memset(inBuf, 0, sizeof(inBuf));
              sprintf(inBuf, "%03d", SQLlevel);  // Create string representation padded with zero's
              outBuf[0] = '$';          // Response string begins with this
              strncat(outBuf, inBuf, strlen(inBuf));  // Followed by the value 
            }
			   } else if (c0 == '1') {
			      if (c1 == '0') {             // Return current preset status
              rs = true;
              v = CurrentPreset - '0';   // Get index into channel structure
              // Re-use inBuf as temp storage
              memset(inBuf, 0, sizeof(inBuf));
              
              sprintf(outBuf, "T");      // Add transmit frequency
              strncat(outBuf, Channel[v].TxFreq, 7);
              
              strncat(outBuf, "R", 1);   // Now Receive Freq
              strncat(outBuf, Channel[v].RxFreq, 7);
              
              strncat(outBuf, "M", 1);   // TxRx Freq
              sprintf(inBuf, "%c", Channel[v].Mode);
              strncat(outBuf, inBuf, strlen(inBuf));
              
              strncat(outBuf, "N", 1);   // Tx freq is different from Rx
              sprintf(inBuf, "%c", Channel[v].TxMode);
              strncat(outBuf, inBuf, strlen(inBuf));
              
              strncat(outBuf, "C", 1);  // Is channel in current scan list
              sprintf(inBuf, "%c", Channel[v].scanList);
              strncat(outBuf, inBuf, strlen(inBuf));
              
              strncat(outBuf, "P", 1);  // Preset number
              sprintf(inBuf, "%c", CurrentPreset);
              strncat(outBuf, inBuf, strlen(inBuf));
              
              strncat(outBuf, "#", 1);  // Output power
              sprintf(inBuf, "%c", Channel[v].power);
              strncat(outBuf, inBuf, strlen(inBuf));
            } else if (c1 == '1') {      // Return general status
              rs = true;
              v = CurrentPreset - '0';   // Get index into channel structure
              // Re-use inBuf as temp storage
              memset(inBuf, 0, sizeof(inBuf));

              sprintf(outBuf, "X%cJ", cipherMode);     // Start with current cipher mode

              inBuf[0] = spkr;
              strncat(outBuf, inBuf, 1);               // Copy speaker state into output
              
              strncat(outBuf, "L", 1);                 // Now do lamp
              inBuf[0] = Lamp;
              strncat(outBuf, inBuf, 1);

              strncat(outBuf, "d1", 2);                // Indicate installed  options
              strncat(outBuf, "F0", 2);                // Never go overtemp
            } else if (c1 == '2') {      // Return general mode status
              rs = true;
              if (beaconMode == '1') {
                sprintf(outBuf, "*1");
              } else if (keyed == '1') {
                sprintf(outBuf, "U1");
              } else {
                sprintf(outBuf, "U0");
            }
          } else if (c1 == '3') {      // Return squelch status (receiver squelched/unsquelched)
            rs = true;
            if (SQLstate)
              sprintf(outBuf, "[1");
            else
               sprintf(outBuf, "[0");   
				  } else if (c1 == '7') {      // Return Tuning Filter Value (static value "}063")
            rs = true;
            sprintf(outBuf, "}063");
          } else if (c0 == '7') {
			      if (c1 == '1') {             // Return Flat Slope Status (static value "f1")
              rs = true;
              sprintf(outBuf, "f1");
            } else if (ch == '3') {      // Return startup enabled values
				  }
        }
      }
      // The remaining calibration commands are silently ignored
      break;
      } // case '?'
    }   // Switch (ch)
  } //while

  //Display the output buffer on the display
  //LCD.print(inBuf, 5, 219);
  LCD.print(outBuf, 162, 219);
   
  ch = strlen(outBuf);                     //Re-purpose ch to determine output buffer length
  if (ch)
  Serial.write(outBuf, ch);             //Send the response string
  if (!rs) 
    Serial.write(NAK);
  else                                     // Can add check to see if we are coming out of local-mode to remote mode
                                            // and send HT if so
    Serial.write(ACK);
  return rs;                               //Pass return status back to the caller
} // Function

//Draw the various graphic elements on the screen along with their contents
void drawControls(void)
{
  //Display the preset Rx/Tx frequencies
  showChannel();
  showSqLevel();    //Show current squelch level set by remote
  showTxMode();     //Show current Tx mode AM/FM
  showBeacon();     //Indicate if radio is in beacon mode
  showSpkr();       //Show speaker enabled/disabled
  showOnScan();     //Show whether current channel is on scan list
  showMode();       //Show current Rx mode (AM/FM)
  showRxTx();       //Show Tx/Rx state
  showPwr();        //Show current transmit power level
  showCTPT();       //Show Plain/Cipher Text mode
  showKey();        //Indicate tranceiver RF state
}

//Indicate receiver squelched or not
//Draw a box to show current squelch state (SQL/UNSQL)
//Labelled SQL in above diagram
//and level
//Red text on black background = Audio UnSquelched or squelch broken by received signal
//Green text on black background = Audio is squelched
void showSqLevel(void)
{
  int fc = LCD.getColor();    //Save current colors
  int bc = LCD.getBackColor();
  int fnt = LCD.getFont();

  LCD.setFont(BigFont);
  LCD.setColor(VGA_BLACK);
  LCD.fillRect( 163, 60, 238, 98);   //Blank the drawing area
  
  LCD.setColor(VGA_GREEN);
  LCD.drawRect(163, 60, 238, 98);    //Draw control outline
  LCD.setBackColor(VGA_TRANSPARENT);
  
  if (SQLstate) {
    LCD.setColor(VGA_GREEN);
    LCD.print("S", 165, 73);
  } else {
    LCD.setColor(VGA_RED);
    LCD.print("S", 165, 73);
  }
  
  LCD.setColor(VGA_GREEN);
  LCD.printNumI(SQLlevel, 185, 73, 3, '0');
	 
  LCD.setBackColor(bc);
  LCD.setColor(fc);
  LCD.setFont(fnt);
}

//Show Tx mode (may be different from Rx)
void showTxMode(void)
{
  int fc = LCD.getColor();
  int bc = LCD.getBackColor();
  int fnt = LCD.getFont();
  
  LCD.setColor(VGA_BLACK);
  LCD.fillRect(243, 103, 319, 138);   //Blank drawing area
  LCD.setColor(VGA_GREEN);
  LCD.drawRect(243, 103, 319, 138);
  LCD.setBackColor(VGA_TRANSPARENT);
  LCD.setColor(VGA_RED);
  LCD.setFont(BigFont);
  
  if (CurrentTxMode == '0') { //AM
    LCD.print("T:AM", 248, 113);
  } else {  //FM
    LCD.print("T:FM", 248, 113);
  }

  LCD.setFont(fnt);
  LCD.setBackColor(bc);
  LCD.setColor(fc);
}

//Indicate speaker enabled/disabled    
void showSpkr(void)
{
  int fc = LCD.getColor();
  int bc = LCD.getBackColor();
  int fnt = LCD.getFont();

  LCD.setFont(BigFont);
  LCD.setColor(VGA_BLACK);
  LCD.fillRect(1, 60, 83, 98);     //Blank drawing area
  LCD.setColor(VGA_GREEN);
  LCD.setBackColor(VGA_TRANSPARENT);
  LCD.drawRect(1, 60, 83, 98);     //Draw control outline

  if (spkr == '1') { //speaker enabled
    LCD.setColor(VGA_RED);
  } else {  //speaker disabled
    LCD.setColor(VGA_GREEN);
  }

  LCD.print("SPKR", 10, 73);
  
  LCD.setFont(fnt);
  LCD.setColor(fc);
  LCD.setBackColor(bc);
}

//Indicate freq displayed is Rx or Tx
void showRxTx(void)
{
  int fc = LCD.getColor();
  int bc = LCD.getBackColor();
  int fnt = LCD.getFont();

  LCD.setFont(BigFont);
  LCD.setColor(VGA_BLACK);
  LCD.fillRect(1, 103, 83, 138);       //Blank the drawing area
  LCD.setColor(VGA_GREEN);
  LCD.setBackColor(VGA_TRANSPARENT);
  LCD.drawRect(1, 103, 83, 138);       //Draw control outline

  if (keyed == '0') {  //Rx mode
     LCD.setColor(VGA_GREEN);
     LCD.print("RECV", 10, 112);
  }
  else {
     LCD.setColor(VGA_RED);
     LCD.print("XMIT", 10, 112);
  }
  
  LCD.setFont(fnt);
  LCD.setColor(fc);
  LCD.setBackColor(bc);
}

//Indicate if Tx active
void showKey(void)
{
   int fc = LCD.getColor();
   int bc = LCD.getBackColor();
   
   if (keyed == '0') {
      LCD.setColor(VGA_GREEN);
      LCD.fillRect(1,142,319, 146);
   } else if (keyed == '1') {
      LCD.setColor(VGA_RED);
      LCD.fillRect(1,142,319, 146);
   } else {
    //error state
   }
   
   LCD.setBackColor(bc);
   LCD.setColor(fc);
}

//Indicate plain/cipher
void showCTPT(void)
{
  int fc = LCD.getColor();
  int bc = LCD.getBackColor();
  int fnt = LCD.getFont();
  //1, 30, 38, 58
  LCD.setFont(BigFont);
  LCD.setColor(VGA_BLACK);
  LCD.fillRect(86, 30, 124, 58);     //Blank the drawing area
  LCD.setColor(VGA_GREEN);
  LCD.drawRect(86, 30, 124, 58);     //Draw control outline
  
  if (cipherMode == '0') {           //Plain Text
     LCD.print("PT", 88, 37);
  } else {                           //Cipher Text
     LCD.print("CT", 88, 37);
  }
  
  LCD.setFont(fnt);
  LCD.setColor(fc);
  LCD.setBackColor(bc);
}

//Indicate whether or not radio is in SCAN mode
void showScan(void)
{
  int fc = LCD.getColor();
  int bc = LCD.getBackColor();
  int fnt = LCD.getFont();
  
  LCD.setFont(BigFont);
  LCD.setColor(VGA_BLACK);
  LCD.fillRect(123, 41, 158, 58);     //Blank the drawing area
  LCD.setColor(VGA_GREEN);
  LCD.drawRect(123, 41, 158, 58);     //Draw control outline
  
  if (scanMode == '1') {
     LCD.setBackColor(VGA_LIME);
	 LCD.setColor(VGA_BLACK);
  } else {
     LCD.setBackColor(VGA_BLACK);
	 LCD.setColor(VGA_GRAY);
  }
  
  LCD.print("SCAN", 125, 45);

  LCD.setFont(fnt);
  LCD.setColor(fc);
  LCD.setBackColor(bc);  
}

//Indicate if current channel on scan list
void showOnScan(void)
{
  int fc = LCD.getColor();
  int bc = LCD.getBackColor();
  int fnt = LCD.getFont();
  int ch = CurrentPreset - '0';          //Determine active channel
  int in = Channel[ch].scanList - '0';   //Determine if chan is on scan list

  LCD.setFont(BigFont);
  LCD.setColor(VGA_BLACK);
  LCD.setBackColor(VGA_TRANSPARENT);
  
  LCD.fillRect(87, 60, 158, 98);   //Blank the drawing area
  LCD.setColor(VGA_GREEN);
  LCD.drawRect(87, 60, 158, 98);   //Draw control outline
  
  if (in)
     LCD.print(" IN ", 90, 70);
  
  LCD.setFont(fnt);
  LCD.setColor(fc);
  LCD.setBackColor(bc);
}

//Indicate whether or not radio is in BEACON mode
//Labelled BCN in above diagram
//Filled black (not really visible, which is good) = normal mode
//Filled red with black 'BCN' text = beacon mode activated
//When activated the radio state is changed to keyed and Tx is indicated
void showBeacon(void)
{
  int fc = LCD.getColor();
  int bc = LCD.getBackColor();
  int fnt = LCD.getFont();

  LCD.setFont(BigFont);
  LCD.setColor(VGA_BLACK);
  LCD.setBackColor(VGA_TRANSPARENT);
  
  LCD.fillRect(1, 30, 38, 58);   //Blank the drawing area
  LCD.setColor(VGA_GREEN);
  LCD.drawRect(1, 30, 38, 58);
  
  if (beaconMode == '1') {
   LCD.setColor(VGA_BLACK);
	 LCD.setBackColor(VGA_RED);
	 LCD.print("B", 5, 38);
  }

  LCD.setFont(fnt);
  LCD.setColor(fc);
  LCD.setBackColor(bc);      
}

//Show current power output              
void showPwr(void)
{
  int fc = LCD.getColor();
  int bc = LCD.getBackColor();
  int fnt = LCD.getFont();

  LCD.setFont(BigFont);
  LCD.setColor(VGA_BLACK);
  LCD.setBackColor(VGA_TRANSPARENT);
  
  LCD.fillRect(87, 103, 158,138);   //Blank the drawing area
  
  LCD.setColor(VGA_GREEN);
  LCD.drawRect(87, 103, 158,138);

  switch (CurrentPwr) {
    case '0': {
       LCD.print("LO ", 95, 112);
       break;
    }
    case '1': {
       LCD.print("MED", 95, 112);
       break;
    }
    case '2': {
       LCD.print(" HI", 95, 112);
       break;
    }
  }

  LCD.setFont(fnt);
  LCD.setColor(fc);
  LCD.setBackColor(bc); 
}

//Show current Rx mode (AM/FM)
//Labelled 'FM AM' in above diagram next to 'Rx Mode'
void showMode(void)
{
  int fc = LCD.getColor();
  int bc = LCD.getBackColor();
  int fnt = LCD.getFont();
  LCD.setFont(BigFont);

  LCD.setColor(VGA_BLACK);
  LCD.fillRect(163, 101, 238, 138);   //Blank the drawing area
  
  LCD.setColor(VGA_GREEN);            //Rx label in green
  LCD.setBackColor(VGA_TRANSPARENT);
  LCD.drawRect(163, 101, 238, 138);
  
  if (CurrentMode == '0')
     LCD.print("R:AM", 167, 113);    //Will show AM or FM
  else
     LCD.print("R:FM", 167, 113);    //Will show AM or FM
  
  LCD.setFont(fnt);
  LCD.setColor(fc);
  LCD.setBackColor(bc);
}

//Draw the labels for the various controls
void drawLabels(void)
{
  lblInBuf();         //Box to show command sent from host
  lblOutBuf();        //Box to show our response (if any)
  //lblSqLevel();       //Draw a box showing current squelch level
  //lblSql();           //Draw a box to show current squelch state (SQL/UNSQL)
  //lblPwr();           //Draw a box to show current power level
  //lblSpkr();          //Draw a box to show current spkr state (enabled/disabled)
  //lblMode();          //Draw a box showing the current Rx mode (AM/FM)
  //lblTxMode();        //Draw a box showing the current Tx mode (AM/FM)
  //lblRxTx();          //Draw a box indicating displayed freq is Rx or Tx
  //lblKey();           //Draw a round button showing RF state (green = RX, red = TX)
  //lblCTPT();          //Draw a box to show cipher state
  //lblScan();          //Draw a box to show scan mode active/inactive
  //lblOnScan();        //Draw a box showing whether or not the current channel is in the scan list
  //lblBeacon();        //Draw a box to show beacon mode (active/inactive)
  
}

// Original (too cramped)
//------------------------------------------------------------------------------
// +------+ +-----+ +-----+ +----------+ +------+ +-----+
// | KEY  | | SQL | | lvl | | Tx Mode  | |AM/FM | | BCN |
// +------+ +-----+ +-----+ +----------+ +------+ +-----+
// +------+ +-------------+ +----------+ +------+
// | SPKR | |  SCAN LIST  | | Rx Mode  | |AM/FM |
// +------+ +-------------+ +----------+ +------+
// +------+ +-------------+ +----------+ +------+
// |  TX  | | LO  MED  HI | |  CT  PT  | | SCAN |
// +------+ +-------------+ +----------+ +------+
// +-------------------------------------------------------------------------+  //
// |                                                                         |  //
// |                          PRESET : MHz : kHz                             |  //
// |                                                                         |  //
// +-------------------------------------------------------------------------+  //
// +-----------------------------------+ +-----------------------------------+  //
// |          inBuf                    | |          outBuf                   |  //
// +-----------------------------------+ +-----------------------------------+  //
//------------------------------------------------------------------------------//

// 2nd gen 
//------------------------------------------------------------------------------
// 
// 
// +-----+            +------+  +------+ +------+  +------+ +------+  +------+
// | BCN |            | PT/CT|  | SCAN | |  DN  |  |  UP  | |  DN  |  |  UP  |
// +-----+            +------+  +------+ +------+  +------+ +------+  +------+
// +----------------+ +----------------+ +----------------+ +----------------+
// |                | |       IN       | |                | |                |
// |      SPKR      | |      SCAN      | | SQL: Level     | | SQL: Pot       |
// |                | |      LIST      | |                | |                |
// +----------------+ +----------------+ +----------------+ +----------------+
// +----------------+ +----------------+ +----------------+ +----------------+
// |                | |                | |                | |                |
// |  RECV/XMIT     | |      PWR       | | Rx: AM/FM      | | Tx: AM/FM      |
// |                | |                | |                | |                |
// +----------------+ +----------------+ +----------------+ +----------------+
// +-------------------------------------------------------------------------+
// +-------------------------------------------------------------------------+
// +-------------------------------------------------------------------------+  //
// |                                                                         |  //
// |                          PRESET : MHz : kHz                             |  //
// |                                                                         |  //
// +-------------------------------------------------------------------------+  //
// +-----------------------------------+ +-----------------------------------+  //
// |          inBuf                    | |          outBuf                   |  //
// +-----------------------------------+ +-----------------------------------+  //
//------------------------------------------------------------------------------//


//Draw a round button showing RF state (green = RX, red = TX)
//Labelled 'KEY' in above diagram
//Filled red = Tx in operation
//Filled green = Tx silent (Rx mode)
void lblKey(void)
{

}

//Draw a box to show current squelch state (SQL/UNSQL)
//Labelled SQL in above diagram
//Red text on black background = Audio UnSquelched or squelch broken by received signal
//Green text on black background = Audio is squelched
void lblSql(void)
{

}

//Draw a box showing current squelch level
//Labelled 'Level' in above diagram
//Value is 0 to 255
void lblSqLevel(void)
{

}

//Draw a box showing the current Tx mode (AM/FM)
//Labelled 'FM AM' in above diagram next to 'Tx Mode'
//Draw 2 boxes, 1 static text "Tx:", 2 box for "AM" or "FM"
void lblTxMode(void)
{

}

//Draw a box to show current spkr state (enabled/disabled)
//Labelled SPKR in above diagram
//Label is green text on black for speaker enabled
//Label is red text on black for speaker enabled
void lblSpkr(void)
{
 
}

//Draw a box showing whether or not the current channel is in the scan list
//Labelled 'SCAN LIST' in ablove diagram
//Green text on black background showing 'SCAN' = channel is on scan list
//Blank = channel not on scan list
void lblOnScan(void)
{
  
}

//Draw a box showing the current Rx mode (AM/FM)
//Labelled 'FM AM' in above diagram next to 'Rx Mode'
void lblMode(void)
{


}

//Draw a box indicating displayed freq is Rx or Tx
//Labelled 'TX' in above diagram
//Show TX for Tx frequency shown
//Show RX for Rx frequency shown
void lblRxTx(void)
{

}

//Draw a box to show current power level
//Labelled 'LO MED HI' in above diagram
//Text is 12 characters with only the current power showing
void lblPwr(void)
{

}

//Draw a box to show cipher state
//Labelled 'CT  PT' in above diagram
//The states are mutually exclusive
void lblCTPT(void)
{

}

//Draw a box to show scan mode active/inactive
//Labelled 'SCAN' in above diagram
//Grey (White) text on black background means radio is not in scan mode
//Black text on green background means radio is in scan mode
void lblScan(void)
{

}

//Draw a box to show beacon mode (active/inactive)
//Labelled BCN in above diagram
//Filled black (not really visible, which is good) = normal mode
//Filled red with black 'BCN' text = beacon mode activated
//When activated the radio state is changed to keyed and Tx is indicated
void lblBeacon(void)
{

}

//Draw a box for displaying the Serial input buffer
//Labelled 'inBuf' in above diagram
void lblInBuf(void)
{
   LCD.drawRect(1, 211, 158, 239);
}

//Draw a box for displaying the Serial output buffer
//Labelled 'outBuf' in above diagram
void lblOutBuf(void)
{           
   LCD.drawRect(160, 211, 319, 239);
}
