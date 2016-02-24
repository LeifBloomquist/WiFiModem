/*
Commodore 64 - MicroView - Wi-Fi Cart
Copyright 2015-2016 Leif Bloomquist and Alex Burger

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Written with assistance and code from Greg Alekel and Payton Byrd */


/* TODO
 *  -at&f&c1 causes DCD to go high when &f is processed, then low again when &c1 is processed.
 *  
 *  
 */

#define MICROVIEW        // define to include MicroView display library and features

//#define DEBUG

// Defining HAYES enables Hayes commands and disables the 1) and 2) menu options for telnet and incoming connections.
// This is required to ensure the compiled code is <= 30,720 bytes 
// Free memory at AT command prompt should be around 300.  At 200, ATI output is garbled.
#define HAYES     // Also define in WiFlyHQ.cpp!

#ifdef MICROVIEW
#include <MicroView.h>
#endif
#include <elapsedMillis.h>
#include <SoftwareSerial.h>
#include <WiFlyHQ.h>
#include <EEPROM.h>
#include <digitalWriteFast.h>
#include "petscii.h"

;  // Keep this here to pacify the Arduino pre-processor

#define VERSION "0.12b2"

unsigned int BAUD_RATE=2400;
unsigned int WiFly_BAUD_RATE=2400;

// Configuration 0v3: Wifi Hardware, C64 Software.

// Wifi
// RxD is D0  (Hardware Serial)
// TxD is D1  (Hardware Serial)

#define WIFI_RTS  2
#define WIFI_CTS  3

// Additional RS-232 lines to C64 User Port
#define C64_RTS  A5
#define C64_DTR  A4
#define C64_RI   A3
#define C64_DCD  A2
#define C64_CTS  A1
#define C64_DSR  A0
#define C64_TxD  6
#define C64_RxD  5

SoftwareSerial C64Serial(C64_RxD, C64_TxD);
HardwareSerial& WifiSerial = Serial;
WiFly wifly;

// Telnet Stuff
#define NVT_SE 240
#define NVT_NOP 241
#define NVT_DATAMARK 242
#define NVT_BRK 243
#define NVT_IP 244
#define NVT_AO 245
#define NVT_AYT 246
#define NVT_EC 247
#define NVT_GA 249
#define NVT_SB 250
#define NVT_WILL 251
#define NVT_WONT 252
#define NVT_DO 253
#define NVT_DONT 254
#define NVT_IAC 255

#define NVT_OPT_TRANSMIT_BINARY 0
#define NVT_OPT_ECHO 1
#define NVT_OPT_SUPPRESS_GO_AHEAD 3
#define NVT_OPT_STATUS 5
#define NVT_OPT_RCTE 7
#define NVT_OPT_TIMING_MARK 6
#define NVT_OPT_NAOCRD 10
#define NVT_OPT_TERMINAL_TYPE 24
#define NVT_OPT_NAWS 31
#define NVT_OPT_TERMINAL_SPEED 32
#define NVT_OPT_LINEMODE 34
#define NVT_OPT_X_DISPLAY_LOCATION 35
#define NVT_OPT_ENVIRON 36
#define NVT_OPT_NEW_ENVIRON 39

String lastHost = "";
int lastPort = 23;

char escapeCount = 0;
char wifiEscapeCount = 0;
char lastC64input = 0;
unsigned long escapeTimer = 0;
unsigned long wifiEscapeTimer = 0;
boolean escapeReceived = false;
#define ESCAPE_GUARD_TIME 1000

char autoConnectHost = 0;
//boolean autoConnectedAtBootAlready = 0;           // We only want to auto-connect once..
#define ADDR_HOST_SIZE              40
#define ADDR_ANSWER_MESSAGE_SIZE    75              // Currently limited by the AT command line buffer size which is 80..
#define ADDR_HOST_ENTRIES           9
    
// EEPROM Addresses
// Microview has 1K
#define ADDR_PETSCII       0
//#define ADDR_AUTOSTART     1
#define ADDR_BAUD_LO       2
#define ADDR_BAUD_HI       3
#define ADDR_MODEM_ECHO         10
#define ADDR_MODEM_FLOW         11
#define ADDR_MODEM_VERBOSE      12
#define ADDR_MODEM_QUIET        13
#define ADDR_MODEM_S0_AUTOANS   14
#define ADDR_MODEM_S2_ESCAPE    15
//#define ADDR_MODEM_DCD_INVERTED 16
#define ADDR_MODEM_DCD          17
#define ADDR_MODEM_X_RESULT     18
#define ADDR_MODEM_SUP_ERRORS   19
#define ADDR_MODEM_DSR          20

#define ADDR_HOST_AUTO          99     // Autostart host number
#define ADDR_HOSTS              100    // to 460 with ADDR_HOST_SIZE = 40 and ADDR_HOST_ENTRIES = 9
#define STATIC_PB_ENTRIES       2
//#define ADDR_xxxxx            300
#define ADDR_ANSWER_MESSAGE     800    // to 874 with ADDR_ANSWER_MESSAGE_SIZE = 75

// Hayes variables
#ifdef HAYES
boolean Modem_isCommandMode = true;
boolean Modem_isRinging = false;
boolean Modem_EchoOn = true;
boolean Modem_VerboseResponses = true;
boolean Modem_QuietMode = false;
boolean Modem_S0_AutoAnswer = false;
byte    Modem_X_Result = 0;
boolean Modem_suppressErrors = false;

#define COMMAND_BUFFER_SIZE  81
char Modem_LastCommandBuffer[COMMAND_BUFFER_SIZE];
char Modem_CommandBuffer[COMMAND_BUFFER_SIZE];
char Modem_LastCommandChar;
boolean Modem_AT_Detected = false;
#endif    // HAYES
char    Modem_S2_EscapeCharacter = '+';
boolean Modem_isConnected = false;

// Misc Values
#define TIMEDOUT  -1
boolean baudMismatch = (BAUD_RATE != WiFly_BAUD_RATE ? 1 : 0);
boolean Modem_flowControl = false;   // for &K setting.
boolean Modem_isDcdInverted = true;
boolean Modem_DCDFollowsRemoteCarrier = false;    // &C
byte    Modem_dataSetReady = 0;         // &S
int WiFlyLocalPort = 0;
boolean Modem_isCtsRtsInverted = true;           // Normally true on the C64.  False for commodoreserver 38,400.
boolean isFirstChar = true;
boolean isTelnet = false;
boolean telnetBinaryMode = false;
#ifdef HAYES
boolean petscii_mode_guess = false;
boolean commodoreServer38k = false;
#endif
                                           //int max_buffer_size_reached = 0;            // For debugging 1200 baud buffer

/* PETSCII state.  Always use ASCII for Hayes.  
   To set SSID, user must use ASCII mode.
  */
#ifndef HAYES
boolean petscii_mode = EEPROM.read(ADDR_PETSCII);
#endif

// Autoconnect Options
#define AUTO_NONE     0
#define AUTO_HAYES    1
#define AUTO_CSERVER  2
#define AUTO_QLINK    3
#define AUTO_CUSTOM   100   // TODO

// ----------------------------------------------------------
// Arduino Setup Function

int main(void)
{
    init();
#ifdef MICROVIEW    
    uView.begin(); // begin of MicroView
    uView.setFontType(0);
    uView.clear(ALL); // erase hardware memory inside the OLED controller
#else // MICROVIEW
    delay(3000);   // Five WiFly time to boot
#endif // MICROVIEW

    // Always setup pins for flow control
    pinModeFast(C64_RTS, INPUT);
    pinModeFast(C64_CTS, OUTPUT);

    pinModeFast(WIFI_RTS, INPUT);
    pinModeFast(WIFI_CTS, OUTPUT);

    // Force CTS to defaults on both sides to start, so data can get through
    digitalWriteFast(WIFI_CTS, LOW);
    digitalWriteFast(C64_CTS, (Modem_isCtsRtsInverted ? HIGH : LOW));

    pinModeFast(C64_DCD, OUTPUT);
    Modem_DCDFollowsRemoteCarrier = EEPROM.read(ADDR_MODEM_DCD);

    if (Modem_DCDFollowsRemoteCarrier) {
        digitalWriteFast(C64_DCD, Modem_ToggleCarrier(false));
    }
    else {
        digitalWriteFast(C64_DCD, Modem_ToggleCarrier(true));
    }

    Modem_dataSetReady = EEPROM.read(ADDR_MODEM_DSR);
    if (Modem_dataSetReady == 2)
        pinModeFast(C64_DSR, INPUT);    // Set as input to make sure it doesn't interfere with UP9600.
                                        // Some computers have issues with 100 ohm resistor pack

    BAUD_RATE = (EEPROM.read(ADDR_BAUD_LO) * 256 + EEPROM.read(ADDR_BAUD_HI));

    if (BAUD_RATE != 1200 && BAUD_RATE != 2400 && BAUD_RATE != 4800 && BAUD_RATE != 9600 && BAUD_RATE != 19200 && BAUD_RATE != 38400)
        BAUD_RATE = 2400;

    //
    // Baud rate detection
    //
    pinMode (C64_RxD, INPUT);
    digitalWrite (C64_RxD, HIGH);

    Display(F("Baud\nDetection"));

    long detectedBaudRate = detRate(C64_RxD);  // Function finds a standard baudrate of either
    // 1200,2400,4800,9600,14400,19200,28800,38400,57600,115200
    // by having sending circuit send "U" characters.
    // Returns 0 if none or under 1200 baud

    //char temp[20];
    //sprintf(temp, "Baud\ndetected:\n%ld", detectedBaudRate);
    //Display(temp);

    if (detectedBaudRate == 1200 || detectedBaudRate == 2400 || detectedBaudRate == 4800 || detectedBaudRate == 9600 || detectedBaudRate == 19200 || detectedBaudRate == 38400) {
        char temp[6];
        sprintf_P(temp, PSTR("%ld"), detectedBaudRate);
        Display(temp);
        delay(3000);

        BAUD_RATE = detectedBaudRate;

        byte a = BAUD_RATE / 256;
        byte b = BAUD_RATE % 256;

        updateEEPROMByte(ADDR_BAUD_LO,a);
        updateEEPROMByte(ADDR_BAUD_HI,b);
    }

    //
    // Baud rate detection end
    //

    C64Serial.begin(BAUD_RATE);
    WifiSerial.begin(WiFly_BAUD_RATE);

    C64Serial.setTimeout(1000);

    baudMismatch = (BAUD_RATE != WiFly_BAUD_RATE ? 1 : 0);

    //WifiSerial.begin(WiFly_BAUD_RATE);
    
    C64Serial.println();
#ifdef HAYES
    DisplayBoth(F("WI-FI INIT..."));
#else
    DisplayBoth(F("Wi-Fi Init..."));
#endif

#ifdef DEBUG
    boolean ok = wifly.begin(&WifiSerial, &C64Serial);
#else
    boolean ok = wifly.begin(&WifiSerial);
#endif

    if (ok)
    {
#ifdef HAYES
        DisplayBoth(F("WI-FI OK!"));
#else
        DisplayBoth(F("Wi-Fi OK!"));
#endif
    }
    else
    {
#ifdef HAYES
        DisplayBoth(F("WI-FI FAILED!"));
#else
        DisplayBoth(F("Wi-Fi Failed!"));
#endif
        RawTerminalMode();
    }

    configureWiFly();

    if (BAUD_RATE != 2400) {
        delay(1000);
        if (BAUD_RATE == 1200)
            setBaudWiFi(2400);
        else
            setBaudWiFi(BAUD_RATE);
    }
    baudMismatch = (BAUD_RATE != WiFly_BAUD_RATE ? 1 : 0);
  
    wifly.close();

    autoConnectHost = EEPROM.read(ADDR_HOST_AUTO);
#ifndef HAYES        
    Modem_flowControl = EEPROM.read(ADDR_MODEM_FLOW);
    HandleAutoStart();
#endif  // HAYES
   
    //C64Println();
#ifdef HAYES
    //C64Println(F("\r\nCommodore Wi-Fi Modem Hayes Emulation"));
    C64Println(F("\r\COMMODORE WI-FI MODEM HAYES EMULATION"));
    //ShowPETSCIIMode();
    C64Println();
    ShowInfo(true);
    HayesEmulationMode();
#else
    C64Println(F("\r\nCommodore Wi-Fi Modem"));
    C64Println();
    ShowInfo(true);

    while (1)
    {
        Display(F("READY."));

        ShowPETSCIIMode();
        C64Print(F("1. Telnet to host or BBS\r\n"
                     "2. Phone Book\r\n"
                     "3. Wait for incoming connection\r\n"
                     "4. Configuration\r\n"
                     "\r\n"
                     "Select: "));

        int option = ReadByte(C64Serial);
        C64Serial.println((char)option);

        switch (option)
        {
        case '1':
            DoTelnet();
            break;

        case '2':
            PhoneBook();
            break;

        case '3':
            Incoming();
            break;
   
        case '4':
                Configuration();
            break;
 
        case '\n':
        case '\r':
        case ' ':
            break;

        case 8:
            SetPETSCIIMode(false);
            break;

        case 20:
            SetPETSCIIMode(true);
            break;

        default:
            C64Println(F("Unknown option, try again"));
            break;
        }
    }
#endif // HAYES
}

#ifndef HAYES
void Configuration()
{
    while (true)
    {
        char temp[30];
        C64Print(F("\r\n"
                     "Configuration Menu\r\n"
                     "\r\n"
                     "1. Display Current Configuration\r\n"
                     "2. Change SSID\r\n"));

        sprintf_P(temp,PSTR("3. %s flow control"),Modem_flowControl == true ? "Disable" : "Enable");        
        C64Println(temp);      
        sprintf_P(temp,PSTR("4. %s DCD always on"),Modem_DCDFollowsRemoteCarrier == false ? "Disable" : "Enable");        
        C64Println(temp);      
        C64Print(F("5. Direct Terminal Mode (Debug)\r\n"
                     "6. Return to Main Menu\r\n"
                     "\r\nSelect: "));

        int option = ReadByte(C64Serial);
        C64Serial.println((char)option);

        switch (option)
        {
        case '1': 
            ShowInfo(false);
            delay(1000);        // Sometimes menu doesn't appear properly after 
                                // showInfo().  May only be at high speeds such as 38400.  Trying this..
            break;

        case '2': ChangeSSID();
            break;

        case '3':
            Modem_flowControl = !Modem_flowControl;
            updateEEPROMByte(ADDR_MODEM_FLOW,Modem_flowControl);
            break;

        case '4':
            Modem_DCDFollowsRemoteCarrier = !Modem_DCDFollowsRemoteCarrier;

            if (Modem_DCDFollowsRemoteCarrier) {
              digitalWriteFast(C64_DCD, Modem_ToggleCarrier(false));
            }
            else {
              digitalWriteFast(C64_DCD, Modem_ToggleCarrier(true));
            }
            
            updateEEPROMByte(ADDR_MODEM_DCD,Modem_DCDFollowsRemoteCarrier);
            break;

        case '5': RawTerminalMode();
            return;

        case '6': return;

        case 8:
            SetPETSCIIMode(false);
            continue;

        case 20:
            SetPETSCIIMode(true);
            continue;

        case '\n':
        case '\r':
        case ' ':
            continue;

        default: C64Println(F("Unknown option, try again"));
            continue;
        }
    }
}

void ChangeSSID()
{
    int mode = -1;

    while (true)
    {
        C64Print(F("\r\n"
                     "Change SSID\r\n"
                     "\r\n"
                     "1. WEP\r\n"
                     "2. WPA / WPA2\r\n"
                     "3. Return to Configuration Menu\r\n"
                     "\r\n"
                     "Select: "));

        int option = ReadByte(C64Serial);
        C64Serial.println((char)option);

        switch (option)
        {
        case '1': mode = WIFLY_MODE_WEP;
            break;

        case '2': mode = WIFLY_MODE_WPA;
            break;

        case '3': return;

        case 8:
            SetPETSCIIMode(false);
            continue;

        case 20:
            SetPETSCIIMode(true);
            continue;

        case '\n':
        case '\r':
        case ' ':
            continue;

        default: C64Println(F("Unknown option, try again"));
            continue;
        }

        C64Println();
        String input;

        switch (mode)
        {
        case WIFLY_MODE_WEP:
            C64Print(F("Key:"));
            input = GetInput();
            wifly.setKey(input.c_str());
            break;

        case WIFLY_MODE_WPA:
            C64Print(F("Passphrase:"));
            input = GetInput();
            wifly.setPassphrase(input.c_str());
            break;

        default:  // Should never happen
            continue;
        }

        C64Println();
        C64Print(F("SSID:"));
        input = GetInput();
        wifly.setSSID(input.c_str());   // Note return value appears to be inverted, not sure why
        wifly.save();
        wifly.leave();

        if (wifly.join(20000))    // 20 second timeout
        {
            C64Println(F("\r\nSSID Successfully changed"));
            return;
        }
        else
        {
            C64Println(F("\r\nError joining network"));
            continue;
        }
    }
}
void PhoneBook()
{
  while(true)
    {
        char address[ADDR_HOST_SIZE];
        char numString[2];

        C64Println();
        DisplayPhoneBook();
        //C64Print(F("\r\nSelect: #, m to modify, a to set\r\n""auto-start, 0 to go back: "));
        C64Print(F("\r\nSelect: #, m to modify, c to clear all\r\na to set auto-start, 0 to go back: "));

        char addressChar = ReadByte(C64Serial);
        C64Serial.println((char)addressChar);
        
        if (addressChar == 'm' || addressChar == 'M')
        {
            C64Print(F("\r\nEntry # to modify? (0 to abort): "));

            char addressChar = ReadByte(C64Serial);
  
            numString[0] = addressChar;
            numString[1] = '\0';
            int phoneBookNumber = atoi(numString);
            if (phoneBookNumber >=0 && phoneBookNumber <= ADDR_HOST_ENTRIES)
            {
                C64Serial.print(phoneBookNumber);
                switch (phoneBookNumber) {
                    case 0:
                    break;

                    default:
                    C64Print(F("\r\nEnter address: "));
                    String hostName = GetInput();
                    if (hostName.length() > 0)
                    {
                        updateEEPROMPhoneBook(ADDR_HOSTS + ((phoneBookNumber-1) * ADDR_HOST_SIZE), hostName);
                    }
                    else
                        updateEEPROMPhoneBook(ADDR_HOSTS + ((phoneBookNumber-1) * ADDR_HOST_SIZE), "");
                    
                }
            }
        }
        else if (addressChar == 'c' || addressChar == 'C')
        {
            C64Print(F("\r\nAre you sure (y/n)? "));

            char addressChar = ReadByte(C64Serial);

            if (addressChar == 'y' || addressChar == 'Y')
            {              
                for (int i = 0; i < ADDR_HOST_ENTRIES; i++)
                {
                    updateEEPROMPhoneBook(ADDR_HOSTS + (i * ADDR_HOST_SIZE), "\0");
                }
            }
        }
        else if (addressChar == 'a' || addressChar == 'A')
        {
            C64Print(F("\r\nEntry # to set to auto-start?\r\n""(0 to disable): "));

            char addressChar = ReadByte(C64Serial);
  
            numString[0] = addressChar;
            numString[1] = '\0';
            int phoneBookNumber = atoi(numString);
            if (phoneBookNumber >=0 && phoneBookNumber <= ADDR_HOST_ENTRIES)
            {
                C64Serial.print(phoneBookNumber);
                updateEEPROMByte(ADDR_HOST_AUTO, phoneBookNumber);   
            }
          
        }
        else 
        {
            numString[0] = addressChar;
            numString[1] = '\0';
            int phoneBookNumber = atoi(numString);

            if (phoneBookNumber >=0 && phoneBookNumber <= ADDR_HOST_ENTRIES)
            {
                switch (phoneBookNumber) {
                    case 0:
                    return;
                    
                    default:
                    {
                        strncpy(address,readEEPROMPhoneBook(ADDR_HOSTS + ((phoneBookNumber-1) * ADDR_HOST_SIZE)).c_str(),ADDR_HOST_SIZE);
                        removeSpaces(address);
                        Modem_Dialout(address);
                    }

                    break;
                }

            }
        }    
    }
}

#endif  // HAYES

// ----------------------------------------------------------
// MicroView Display helpers

void Display(String message)
{
#ifdef MICROVIEW
    uView.clear(PAGE); // erase the memory buffer, when next uView.display() is called, the OLED will be cleared.
    uView.setCursor(0, 0);
    uView.println(message);
    uView.display();
#endif
}

// Pointer version.  Does not work with F("") or PSTR("").  Use with sprintf and sprintf_P
void DisplayP(const char *message)
{
#ifdef MICROVIEW
    uView.clear(PAGE); // erase the memory buffer, when next uView.display() is called, the OLED will be cleared.
    uView.setCursor(0, 0);
    uView.println(message);
    uView.display();
#endif
}

void DisplayBoth(String message)
{
    C64Println(message);
    message.replace(' ', '\n');
    Display(message);
}

// Pointer version.  Does not work with F("") or PSTR("").  Use with sprintf and sprintf_P
void DisplayBothP(const char *message)
{
    String temp = message;
    C64PrintlnP(temp.c_str());
    temp.replace(' ', '\n');
    DisplayP(temp.c_str());
}


// ----------------------------------------------------------
// Wrappers that always print correctly for ASCII/PETSCII

void C64Print(String message)
{
#ifdef HAYES
    C64Serial.print(message);
#else
    if (petscii_mode)
    {
        C64Serial.print(petscii::ToPETSCII(message.c_str()));
    }
    else
    {
        C64Serial.print(message);
    }
#endif
}

void C64Println(String message)
{
    C64Print(message);
    C64Serial.println();
}

// Pointer version.  Does not work with F("") or PSTR("").  Use with sprintf and sprintf_P
void C64PrintP(const char *message)
{
#ifdef HAYES
    C64Serial.print(message);
#else
    if (petscii_mode)
    {
        C64Serial.print(petscii::ToPETSCII(message));
    }
    else
    {
        C64Serial.print(message);
    }
#endif
}

// Pointer version.  Does not work with F("") or PSTR("").  Use with sprintf and sprintf_P
void C64PrintlnP(const char *message)
{
    C64PrintP(message);
    C64Serial.println();
}

void C64Println()
{
    C64Serial.println();
}

#ifndef HAYES
void ShowPETSCIIMode()
{
    C64Println();

    if (petscii_mode)
    {
        const char message[] PROGMEM =
        {
            petscii::CG_RED, 'p',
            petscii::CG_ORG, 'e',
            petscii::CG_YEL, 't',
            petscii::CG_GRN, 's',
            petscii::CG_LBL, 'c',
            petscii::CG_CYN, 'i',
            petscii::CG_PUR, 'i',
            petscii::CG_WHT, ' ', 'm', 'O', 'D', 'E', '!',
            petscii::CG_GR3, '\0'
        };
        C64Serial.print(message);
      
    }
    else
    {
        C64Serial.print(F("ASCII Mode"));
    }

//#ifndef HAYES
    C64Println(F(" (Del to switch)"));
//#endif  // HAYES

    C64Println();
}

void SetPETSCIIMode(boolean mode)
{
    petscii_mode = mode;
    updateEEPROMByte(ADDR_PETSCII,petscii_mode);
}
#endif  // HAYES


// ----------------------------------------------------------
// User Input Handling

boolean IsBackSpace(char c)
{
    if ((c == 8) || (c == 20) || (c == 127))
    {
        return true;
    }
    else
    {
        return false;
    }
}

#ifndef HAYES
String GetInput()
{
    String temp = GetInput_Raw();
    temp.trim();

    if (petscii_mode)  // Input came in PETSCII form
    {
        return petscii::ToASCII(temp.c_str());
    }

    return temp;
}

String GetInput_Raw()
{
    char temp[50];

    int max_length = sizeof(temp);

    int i = 0; // Input buffer pointer
    char key;

    while (true)
    {
        key = ReadByte(C64Serial);  // Read in one character

        if (!IsBackSpace(key))  // Handle character, if not backspace
        {
            temp[i] = key;
            C64Serial.write(key);    // Echo key press back to the user

            if (((int)key == 13) || (i >= (max_length - 1)))   // The 13 represents enter key.
            {
                temp[i] = 0; // Terminate the string with 0.
                return String(temp);
            }
            i++;
        }
        else     // Backspace
        {
            if (i > 0)
            {
                C64Serial.write(key);
                i--;
            }
        }

        // Make sure didn't go negative
        if (i < 0) i = 0;
    }
}
#endif        // HAYES

// ----------------------------------------------------------
// Show Configuration

void ShowInfo(boolean powerup)
{
    char mac[20];
    char ip[20];
    char ssid[20];
    C64Println();
    //C64Serial.print(freeRam());
    //C64Println();
       
    wifly.getPort();    // Sometimes the first time contains garbage..
    wifly.getMAC(mac, 20);
    wifly.getIP(ip, 20);
    wifly.getSSID(ssid, 20);
    WiFlyLocalPort = wifly.getPort();   // Port WiFly listens on.  0 = disabled.

#ifdef HAYES
    //C64Println();
    C64Print(F("MAC ADDRESS: "));    C64Println(mac);
    C64Print(F("IP ADDRESS:  "));    C64Println(ip);
    C64Print(F("WI-FI SSID:  "));    C64Println(ssid);
    C64Print(F("FIRMWARE:    "));    C64Println(VERSION);
    C64Print(F("LISTEN PORT: "));    C64Serial.print(WiFlyLocalPort); C64Serial.println();

    if (!powerup) {
        char at_settings[40];
        //sprintf_P(at_settings, PSTR("ATE%dQ%dV%d&C%d&K%dS0=%d"), Modem_EchoOn, Modem_QuietMode, Modem_VerboseResponses, Modem_DCDFollowsRemoteCarrier, Modem_flowControl, Modem_S0_AutoAnswer);
        sprintf_P(at_settings, PSTR("\r\n E%dQ%dV%d&C%dX%d&K%d&S%d\r\n S0=%d S2=%d S99=%d"),
            Modem_EchoOn,Modem_QuietMode,Modem_VerboseResponses,
            Modem_DCDFollowsRemoteCarrier, Modem_X_Result,Modem_flowControl,
            Modem_dataSetReady,Modem_S0_AutoAnswer,(int)Modem_S2_EscapeCharacter, 
            Modem_suppressErrors);
        C64Print(F("CURRENT INIT:"));    C64Println(at_settings);
        //sprintf_P(at_settings, PSTR("ATE%dQ%dV%d&C%d&K%dS0=%d"),EEPROM.read(ADDR_MODEM_ECHO),EEPROM.read(ADDR_MODEM_QUIET),EEPROM.read(ADDR_MODEM_VERBOSE),EEPROM.read(ADDR_MODEM_DCD),EEPROM.read(ADDR_MODEM_FLOW),EEPROM.read(ADDR_MODEM_S0_AUTOANS));
        sprintf_P(at_settings, PSTR("\r\n E%dQ%dV%d&C%dX%d&K%d&S%d\r\n S0=%d S2=%d S99=%d"),
            EEPROM.read(ADDR_MODEM_ECHO),EEPROM.read(ADDR_MODEM_QUIET),EEPROM.read(ADDR_MODEM_VERBOSE),
            EEPROM.read(ADDR_MODEM_DCD), EEPROM.read(ADDR_MODEM_X_RESULT),EEPROM.read(ADDR_MODEM_FLOW),
            EEPROM.read(ADDR_MODEM_DSR), EEPROM.read(ADDR_MODEM_S0_AUTOANS),EEPROM.read(ADDR_MODEM_S2_ESCAPE),
            EEPROM.read(ADDR_MODEM_SUP_ERRORS));
        C64Print(F("SAVED INIT:  "));    C64Println(at_settings);
    }
#else
    //C64Println();
    C64Print(F("MAC Address: "));    C64Println(mac);
    C64Print(F("IP Address:  "));    C64Println(ip);
    C64Print(F("Wi-Fi SSID:  "));    C64Println(ssid);
    C64Print(F("Firmware:    "));    C64Println(VERSION);
    C64Print(F("Listen port: "));    C64Serial.print(WiFlyLocalPort); C64Serial.println();
#endif

#ifndef HAYES
#ifdef MICROVIEW    
    if (powerup)
    {
        char temp[40];

        sprintf_P(temp, PSTR("Firmware\n\n%s"), VERSION);
        Display(temp);
        delay(1000);

        sprintf_P(temp, PSTR("Baud Rate\n\n%u"), BAUD_RATE);
        Display(temp);
        delay(1000);

        sprintf_P(temp, PSTR("IP Address \n%s"), ip);
        Display(temp);
        delay(1000);

        sprintf_P(temp, PSTR("SSID\n\n%s"), ssid);
        Display(temp);
        delay(1000);
    }
#endif  // MICROVIEW    
#endif  // HAYES
}

#ifndef HAYES
// ----------------------------------------------------------
// Simple Incoming connection handling

void Incoming()
{
    int localport = WiFlyLocalPort;

    C64Print(F("\r\nIncoming port ("));
    C64Serial.print(localport);
    C64Print(F("): "));

    String strport = GetInput();

    if (strport.length() > 0)
    {
        localport = strport.toInt();
        if (setLocalPort(localport))
            while(1);            
    }

    WiFlyLocalPort = localport;  

    while (1)
    {
        /* Force close any connections that were made before we started listening, as
        * the WiFly is always listening and accepting connections if a local port 
        * is defined.  */
        wifly.closeForce();

        C64Print(F("\r\nWaiting for connection on port "));
        C64Serial.println(WiFlyLocalPort);

        // Idle here until connected or cancelled
        while (!wifly.isConnected())
        {
            if (C64Serial.available() > 0)  // Key hit
            {
                C64Serial.read();  // Eat Character
                C64Println(F("Cancelled"));
                return;
            }
        }

        C64Println(F("Incoming Connection"));
        wifly.println(F("CONNECTING..."));
        //CheckTelnet();
        TerminalMode();
    }
}

// ----------------------------------------------------------
// Telnet Handling
void DoTelnet()
{
    int port = lastPort;

    C64Print(F("\r\nTelnet host ("));
    C64Print(lastHost);
    C64Print(F("): "));
    String hostName = GetInput();

    if (hostName.length() > 0)
    {
        port = getPort();

        lastHost = hostName;
        lastPort = port;

        Connect(hostName, port, false);
    }
    else
    {
        if (lastHost.length() > 0)
        {
            port = getPort();

            lastPort=port;
            Connect(lastHost, port, false);
        }
        else
        {
            return;
        }
    }
}

int getPort(void)
{
    C64Print(F("\r\nPort ("));
    C64Serial.print(lastPort);
    C64Print(F("): "));
    
    String strport = GetInput();

    if (strport.length() > 0)
    {
        return(strport.toInt());
    }
    else
    {
        return(lastPort);
    }
}
#endif // HAYES

void Connect(String host, int port, boolean raw)
{
    if (host == F("5551212")) {
        host = F("qlink.lyonlabs.org");
        port = 5190;
    }
    else if (host == F("5551213")) {
        host = F("q-link.net");
        port = 5190;
    }
#ifdef HAYES
    else if (host == F("CS38")) {
        host = F("www.commodoreserver.com");
        port = 1541;
        commodoreServer38k = true;
        
        if (BAUD_RATE != 38400)
        {
            BAUD_RATE = 38400;
            C64Serial.flush();
            WifiSerial.flush();
            delay(1000);
            setBaudWiFi(BAUD_RATE);
            C64Serial.flush();
            C64Serial.end();
            C64Serial.begin(BAUD_RATE);
            C64Serial.setTimeout(1000);
            configureWiFly();
        }
        Modem_isCtsRtsInverted = false;
        Modem_flowControl = true;
        delay(1000);
    }
#endif    // HAYES

    char temp[50];
    //sprintf_P(temp, PSTR("\r\nConnecting to %s"), host.c_str());
    snprintf_P(temp, (size_t)sizeof(temp), PSTR("\r\nConnecting to %s"), host.c_str());
#ifdef HAYES
    DisplayP(temp);
#else
    DisplayBothP(temp);
#endif

    /* Do a DNS lookup to get the IP address.  Lookup has a 5 second timeout. */
    char ip[16];
    if (!wifly.getHostByName(host.c_str(), ip, sizeof(ip))) {
       DisplayBoth(F("Could not resolve DNS.  Connect Failed!"));
       delay(1000);
#ifdef HAYES
       if ((Modem_X_Result == 2) || (Modem_X_Result >= 4))  // >=2 but not 3 = Send NO DIALTONE           
           Modem_PrintResponse(6, F("NO DIALTONE"));
       else
           //Modem_Disconnect(true);
           Modem_PrintResponse(3, F("NO CARRIER"));
#endif
       return;
    }

    boolean ok = wifly.open(ip, port);

    if (ok)
    {
        //sprintf_P(temp, PSTR("Connected to %s"), host.c_str());
        snprintf_P(temp, (size_t)sizeof(temp), PSTR("Connected to %s"), host.c_str());
#ifdef HAYES
        DisplayP(temp);
#else
        DisplayBothP(temp);
#endif
//        if (Modem_DCDFollowsRemoteCarrier)
//            digitalWriteFast(C64_DCD, Modem_ToggleCarrier(true));
    }
    else
    {
#ifdef HAYES
        Display(F("Connect Failed!"));
        delay(1000);
        if (Modem_X_Result >= 3)
            Modem_PrintResponse(7, F("BUSY"));
        else
            //Modem_Disconnect(true);
            Modem_PrintResponse(3, F("NO CARRIER"));
#else
        DisplayBoth(F("Connect Failed!"));
#endif
//        if (Modem_DCDFollowsRemoteCarrier)
//            digitalWriteFast(C64_DCD, Modem_ToggleCarrier(false));
        return;
    }

    /*
    // Display free RAM.  When this was ~184, Microview wouldn't display..
    char temp3[10];
    itoa(freeRam(), temp3, 10);
    DisplayP(temp3);
    delay (2000);*/

#ifdef HAYES
    Modem_Connected(false);
#else
    /*if (!raw)
    {
        CheckTelnet();
    }*/

    if (Modem_DCDFollowsRemoteCarrier)
        digitalWriteFast(C64_DCD, Modem_ToggleCarrier(true));
    TerminalMode();
#endif
}

#ifndef HAYES
void TerminalMode()
{
    int data;
    char buffer[10];
    int buffer_index = 0;
    int buffer_bytes = 0;
    isFirstChar = true;
    //int max_buffer_size_reached = 0;

    //while (wifly.available() != -1) // -1 means closed
    while (wifly.available() != -1) // -1 means closed
    {
        while (wifly.available() > 0)
        {
            int data = wifly.read();

            // If first character back from remote side is NVT_IAC, we have a telnet connection.
            if (isFirstChar) {
                if (data == NVT_IAC)
                {
                    isTelnet = true;
                    CheckTelnetInline();
                }
                else
                {
                    isTelnet = false;
                }
                isFirstChar = false;
            }
            else
            {
                if (data == NVT_IAC && isTelnet)
                {
                    if (baudMismatch)
                    {
                        digitalWriteFast(WIFI_CTS, LOW);        // re-enable data
                        if (CheckTelnetInline())
                            buffer[buffer_index++] = NVT_IAC;
                        digitalWriteFast(WIFI_CTS, HIGH);     // ..stop data from Wi-Fi
                    }
                    else
                    {
                        if (CheckTelnetInline())
                            C64Serial.write(NVT_IAC);
                    }

                }
                else
                {
                    if (baudMismatch)
                    {
                        digitalWriteFast(WIFI_CTS, HIGH);     // ..stop data from Wi-Fi
                        buffer[buffer_index++] = data;
                    }
                    else
                    {
                        DoFlowControlModemToC64();
                        C64Serial.write(data);
                    }
                }
            }
        }
        if (baudMismatch)
        {
            // Dump the buffer to the C64 before clearing WIFI_CTS
            if (buffer_index > 8)
                buffer_index = 8;
            for (int i = 0; i < buffer_index; i++) {
                C64Serial.write(buffer[i]);
            }
            buffer_index = 0;

            digitalWriteFast(WIFI_CTS, LOW);
        }

        while (C64Serial.available() > 0)
        {
            processC64Inbound();
        }

        // Alternate check for open/closed state
        if (!wifly.isConnected() || escapeReceived)
        {
            escapeReceived = 0;
            break;
        }
    }
    wifly.close();

#ifdef HAYES          
    Modem_Disconnect(true);
#else
    DisplayBoth(F("Connection closed"));
#endif
    if (Modem_DCDFollowsRemoteCarrier)
        digitalWriteFast(C64_DCD, Modem_ToggleCarrier(false));
}
#endif  // HAYES

// Raw Terminal Mode.  There is no escape.
void RawTerminalMode()
{
    char temp[50];

    bool changed = false;
    long rnxv_chars = 0;
    long c64_chars = 0;
    long c64_rts_count = 0;      //  !!!! Note, this isn't actually incremented

    C64Println(F("*** Terminal Mode (Debug) ***"));

    while (true)
    {
        while (wifly.available() > 0)
        {
            rnxv_chars++;
            DoFlowControlModemToC64();

            C64Serial.write(wifly.read());
            changed = true;
        }

        while (C64Serial.available() > 0)
        {
            c64_chars++;
            wifly.write(C64Serial.read());
            changed = true;
        }

        if (changed)
        {
            if (Modem_flowControl || baudMismatch)
            {
                sprintf_P(temp, PSTR("Wifi:%ld\n\nC64: %ld\n\nRTS: %ld"), rnxv_chars, c64_chars, c64_rts_count);
            }
            else
            {
                sprintf_P(temp, PSTR("Wifi:%ld\n\nC64: %ld"), rnxv_chars, c64_chars);
            }
            Display(temp);
        }
    }
}

inline void DoFlowControlModemToC64()
{
    if (Modem_flowControl)
    {
        // Check that C64 is ready to receive
        //while (digitalReadFast(C64_RTS == LOW))   // If not...  C64 RTS and CTS are inverted.
        while (digitalReadFast(C64_RTS) == (Modem_isCtsRtsInverted ? LOW : HIGH))
        {
            digitalWriteFast(WIFI_CTS, HIGH);     // ..stop data from Wi-Fi and wait
        }
        digitalWriteFast(WIFI_CTS, LOW);
    }
}
inline void DoFlowControlC64ToModem()
{
    if (Modem_flowControl)
    {
        // Check that C64 is ready to receive
        while (digitalReadFast(WIFI_RTS) == HIGH)   // If not...
        {
            digitalWriteFast(C64_CTS, (Modem_isCtsRtsInverted ? LOW : HIGH));  // ..stop data from C64 and wait
            //digitalWriteFast(C64_CTS, LOW);     // ..stop data from C64 and wait
        }
        digitalWriteFast(C64_CTS, (Modem_isCtsRtsInverted ? HIGH : LOW));
        //digitalWriteFast(C64_CTS, HIGH);
    }
}

// Inquire host for telnet parameters / negotiate telnet parameters with host
// Returns true if in binary mode and modem should pass on a 255 byte from remote host to C64
boolean CheckTelnetInline()     
{
    int inpint, verbint, optint;                        //    telnet parameters as integers
       
    // First time through
    if (isFirstChar)
        SendTelnetParameters();                         // Start off with negotiating
    
    verbint = ReadByte(wifly);                          // receive negotiation verb character
    
    if (verbint == NVT_IAC && telnetBinaryMode)
    {
        return true;                                    // Received two NVT_IAC's so treat as single 255 data
    }    
    
    switch (verbint) {                                  // evaluate negotiation verb character
    case NVT_WILL:                                      // if negotiation verb character is 251 (will)or
    case NVT_DO:                                        // if negotiation verb character is 253 (do) or
        optint = ReadByte(wifly);                       // receive negotiation option character

        switch (optint) {

        case NVT_OPT_SUPPRESS_GO_AHEAD:                 // if negotiation option character is 3 (suppress - go - ahead)
            SendTelnetDoWill(verbint, optint);
            break;

        case NVT_OPT_TRANSMIT_BINARY:                   // if negotiation option character is 0 (binary data)
            SendTelnetDoWill(verbint, optint);
            telnetBinaryMode = true;
            break;

        default:                                        // if negotiation option character is none of the above(all others)
            SendTelnetDontWont(verbint, optint);
            break;                                      //  break the routine
        }
        break;
    case NVT_WONT:                                      // if negotiation verb character is 252 (wont)or
    case NVT_DONT:                                      // if negotiation verb character is 254 (dont)
        optint = ReadByte(wifly);                       // receive negotiation option character

        switch (optint) {

        case NVT_OPT_TRANSMIT_BINARY:                   // if negotiation option character is 0 (binary data)
            SendTelnetDontWont(verbint, optint);
            telnetBinaryMode = false;
            break;

        default:                                        // if negotiation option character is none of the above(all others)
            SendTelnetDontWont(verbint, optint);
            break;                                      //  break the routine
        }
        break;
    case NVT_IAC:                                       // Ignore second IAC/255 if we are in BINARY mode
    default:
        ;
    }
    return false;
}

void SendTelnetDoWill(int verbint, int optint)
{
    wifly.write(NVT_IAC);                               // send character 255 (start negotiation)
    wifly.write(verbint == NVT_DO ? NVT_DO : NVT_WILL); // send character 253  (do) if negotiation verb character was 253 (do) else send character 251 (will)
    wifly.write(optint);
}

void SendTelnetDontWont(int verbint, int optint)
{
    wifly.write(NVT_IAC);                               // send character 255   (start negotiation)
    wifly.write(verbint == NVT_DO ? NVT_WONT : NVT_DONT);    // send character 252   (wont) if negotiation verb character was 253 (do) else send character254 (dont)
    wifly.write(optint);
}

void SendTelnetParameters()
{
    wifly.write(NVT_IAC);                               // send character 255 (start negotiation) 
    wifly.write(NVT_DONT);                              // send character 254 (dont)
    wifly.write(34);                                    // linemode

    wifly.write(NVT_IAC);                               // send character 255 (start negotiation)
    wifly.write(NVT_DONT);                              // send character 253 (do)
    wifly.write(1);                                     // echo
}


// ----------------------------------------------------------
// Helper functions for read/peek

int ReadByte(Stream& in)
{
    while (in.available() == 0) {}
    return in.read();
}

// Peek at next byte.  Returns byte (as a int, via Stream::peek()) or TIMEDOUT (-1) if timed out

int PeekByte(Stream& in, unsigned int timeout)
{
    elapsedMillis timeElapsed = 0;

    while (in.available() == 0)
    {
        if (timeElapsed > timeout)
        {
            return TIMEDOUT;
        }
    }
    return in.peek();
}

#ifndef HAYES
void HandleAutoStart()
{
    // Display chosen action

    if (autoConnectHost >=1 && autoConnectHost <= ADDR_HOST_ENTRIES)
    {
        // Phonebook dial
        char address[ADDR_HOST_SIZE];
        
        strncpy(address,readEEPROMPhoneBook(ADDR_HOSTS + ((autoConnectHost-1) * ADDR_HOST_SIZE)).c_str(),ADDR_HOST_SIZE);

        C64Print(F("Auto-connecting to:\r\n"));
        C64Println(address);
        
        C64Println(F("\r\nPress any key to cancel..."));
        // Wait for user to cancel

        int option = PeekByte(C64Serial, 2000);

        if (option != TIMEDOUT)   // Key pressed
        {
            ReadByte(C64Serial);    // eat character
            return;
        }
        
        Modem_Dialout(address);
    }
}
#endif  // HAYES

#ifdef HAYES
// ----------------------------------------------------------
// Hayes Emulation
// Portions of this code are adapted from Payton Byrd's Hayesduino - thanks!

void HayesEmulationMode()
{
    Modem_DSR_Set();

    pinModeFast(C64_RI, OUTPUT);
    digitalWriteFast(C64_RI, LOW);

    pinModeFast(C64_DTR, INPUT);
    //pinModeFast(C64_DCD, OUTPUT);     // Moved to top of main()
    pinModeFast(C64_RTS, INPUT);

    /* Already done in main()
    if (Modem_DCDFollowsRemoteCarrier == true)
      digitalWriteFast(C64_DCD, Modem_ToggleCarrier(false));*/
   
    Modem_LoadDefaults(true);

    Modem_LoadSavedSettings();

    Modem_ResetCommandBuffer();

    delay (1000);            //  Sometimes text after ShowInfo at boot doesn't appear.  Trying this..
    C64Println();
    
    if (autoConnectHost >=1 && autoConnectHost <= ADDR_HOST_ENTRIES)
    {
        // Phonebook dial
        char address[ADDR_HOST_SIZE];
        
        strncpy(address,readEEPROMPhoneBook(ADDR_HOSTS + ((autoConnectHost-1) * ADDR_HOST_SIZE)).c_str(),ADDR_HOST_SIZE);

        C64Print(F("Auto-connecting to:\r\n"));
        C64Println(address);
        
        C64Println(F("\r\nPress any key to cancel..."));
        // Wait for user to cancel

        int option = PeekByte(C64Serial, 2000);

        if (option != TIMEDOUT)   // Key pressed
        {
            ReadByte(C64Serial);    // eat character
            Display(F("OK"));
            C64Serial.print(F("OK"));
            C64Serial.println();
        }
        else
            Modem_Dialout(address);
    }
    else
    {
        Display(F("OK"));
        C64Serial.print(F("OK"));
        C64Serial.println();
    }

    Modem_Loop();    
}


inline void Modem_PrintOK()
{
    Modem_PrintResponse(0, F("OK"));
}

inline void Modem_PrintERROR()
{
    Modem_PrintResponse(4, F("ERROR"));
}

/* Modem response codes should be in ASCII.  Do not translate to PETSCII. */
void Modem_PrintResponse(byte code, const __FlashStringHelper * msg)
{
    //C64Println();  // Moved to top of Modem_ProcessCommandBuffer() so that 
                     // commands such as ati, at&pb? etc also get get an extra \r\n
    
    /* ASCII upper = 0x41-0x5a
       ASCII lower = 0x61-0x7a
       PETSCII upper = 0xc1-0xda (second uppercase area)
       PETSCII lower = 0x41-0x5a

       AT from C64 is 0xc1,0xd4.  A real modem will shift the response to match (-0x41 + 0xc1)
       so that if a C64 sends At, the modem will responsd with Ok.  Other examples:
       AT - OK
       aT - oK
       At - Ok
       AT - OK
       AtDt5551212 - No caRrier   Note: Not sure why R is capitalized.  Not implementing here..
       ATdt5551212 - NO CARRIER   Note:  If AT is all upper, result is all upper
       atDT5551212 - no carrier   Note:  If AT is all lower, result is all lower
       */
   
    if (!Modem_QuietMode)
    {
        if (Modem_VerboseResponses)
        {
            if (petscii_mode_guess)
            {
                // Start with \r\n as seen with real modems.
                C64Serial.print((char)0x8d);
                C64Serial.print((char)0x8a);

                // If AT is PETSCII uppercase, the entire response is shifted -0x41 + 0xc1 (including newline and return!)
                // Newline/return = 0x8d / 0x8a

                const char PROGMEM *p = (const char PROGMEM *)msg;
                size_t n = 0;
                while (1) {
                    unsigned char c = pgm_read_byte(p++);
                    if (c == 0) break;
                    if (c < 0x30 || c > 0x39)
                        C64Serial.print((char)(c +-0x41 + 0xc1));
                    else
                        C64Serial.print((char)c);
                }

                C64Serial.print((char)0x8d);
                C64Serial.print((char)0x8a);
            }
            else {
                // Start with \r\n as seen with real modems.
                C64Println();
                C64Println(msg);
            }
        }
        else
        {
            if (petscii_mode_guess)
            {
                C64Serial.print(code);
                C64Serial.print((char)0x8d);
            }
            else {
                C64Serial.print(code);
                C64Serial.print("\r");
            }
        }
    }

    // Always show verbose version on OLED, underneath command
#ifdef MICROVIEW
    uView.println();
    uView.println(msg);
    uView.display();
#endif
}

void Modem_ResetCommandBuffer()
{
    memset(Modem_CommandBuffer, 0, COMMAND_BUFFER_SIZE);
    Modem_AT_Detected = false;
}


void Modem_LoadDefaults(boolean booting)
{
    Modem_isCommandMode = true;
    Modem_isConnected = false;
    Modem_isRinging = false;
    Modem_EchoOn = true;
    //Modem_SetEcho(true);
    Modem_VerboseResponses = true;
    Modem_QuietMode = false;
    Modem_S0_AutoAnswer = false;
    Modem_S2_EscapeCharacter = '+';
    //Modem_isDcdInverted = false;
    Modem_flowControl = false;
    Modem_DCDFollowsRemoteCarrier = false;
    Modem_suppressErrors = false;

    Modem_DCD_Set();

    if (!booting)
    {
        Modem_dataSetReady = 0;
        Modem_DSR_Set();
    }
}

void Modem_LoadSavedSettings(void)
{
    // Load saved settings
    Modem_EchoOn = EEPROM.read(ADDR_MODEM_ECHO);
    Modem_flowControl = EEPROM.read(ADDR_MODEM_FLOW);
    Modem_VerboseResponses = EEPROM.read(ADDR_MODEM_VERBOSE);
    Modem_QuietMode = EEPROM.read(ADDR_MODEM_QUIET);
    Modem_S0_AutoAnswer = EEPROM.read(ADDR_MODEM_S0_AUTOANS);
    Modem_S2_EscapeCharacter = EEPROM.read(ADDR_MODEM_S2_ESCAPE);
    //Modem_isDcdInverted = EEPROM.read(ADDR_MODEM_DCD_INVERTED);
    Modem_DCDFollowsRemoteCarrier = EEPROM.read(ADDR_MODEM_DCD);
    Modem_X_Result = EEPROM.read(ADDR_MODEM_X_RESULT);
    Modem_suppressErrors = EEPROM.read(ADDR_MODEM_SUP_ERRORS);
    Modem_dataSetReady = EEPROM.read(ADDR_MODEM_DSR);

    Modem_DCD_Set();
    Modem_DSR_Set();

}

// Only used for at&f and atz commands
void Modem_DCD_Set(void)
{
    if(Modem_DCDFollowsRemoteCarrier == false)
    {
        digitalWriteFast(C64_DCD, Modem_ToggleCarrier(true));
    }
    else {
        if (Modem_isConnected) {
          digitalWriteFast(C64_DCD, Modem_ToggleCarrier(true));
        }
        else {
          digitalWriteFast(C64_DCD, Modem_ToggleCarrier(false));
        }
    }    
}

void Modem_Disconnect(boolean printNoCarrier)
{
    //char temp[15];
    Modem_isCommandMode = true;
    Modem_isConnected = false;
    Modem_isRinging = false;   
    commodoreServer38k = false;

    // Erase MicroView screen as NO CARRIER may not fit after RING, CONNECT etc.
    uView.clear(PAGE); // erase the memory buffer, when next uView.display() is called, the OLED will be cleared.
    uView.setCursor(0, 0);

    if (wifly.available() == -1)
        wifly.close();
    else
        wifly.closeForce();      // Incoming connections need to be force closed.  close()
                                 // does not work because WiFlyHQ.cpp connected variable is
                                 // not set for incoming connections.

    if (printNoCarrier)
        Modem_PrintResponse(3, F("NO CARRIER"));

    if (Modem_DCDFollowsRemoteCarrier)
        digitalWriteFast(C64_DCD, Modem_ToggleCarrier(false));

    if (Modem_dataSetReady == 1)
        digitalWriteFast(C64_DSR, HIGH);
}

// Validate and handle AT sequence  (A/ was handled already)
void Modem_ProcessCommandBuffer()
{
    //boolean petscii_mode_guess = false;
    byte errors = 0;
    //boolean dialed_out = 0;
    // Phonebook dial
    char numString[2];
    char address[ADDR_HOST_SIZE];
    int phoneBookNumber;
    boolean suppressOkError = false;
    char atCommandFirstChars[] = "AEHIQVZ&*RSD\r\n";   // Used for AT commands that have variable length values such as ATS2=45, ATS2=123
    char tempAsciiValue[3];
    
    // Simple PETSCII/ASCII detection   
    if ((((unsigned char)Modem_CommandBuffer[0] == 0xc1) && ((unsigned char)Modem_CommandBuffer[1] == 0xd4)))
        petscii_mode_guess = true;
    else
        petscii_mode_guess = false;

    // Used for a/ and also for setting SSID, PASS, KEY as they require upper/lower
    strcpy(Modem_LastCommandBuffer, Modem_CommandBuffer);

    // Force uppercase for consistency 

    for (int i = 0; i < strlen(Modem_CommandBuffer); i++)
    {
        Modem_CommandBuffer[i] = toupper(charset_p_toascii_upper_only(Modem_CommandBuffer[i]));
    }

    Display(Modem_CommandBuffer);
    
    // Define auto-start phone book entry
    if (strncmp(Modem_CommandBuffer, ("AT&PBAUTO="), 10) == 0)
    {
        char numString[2];
        numString[0] = Modem_CommandBuffer[10];
        numString[1] = '\0';
        
        int phoneBookNumber = atoi(numString);
        if (phoneBookNumber >= 0 && phoneBookNumber <= ADDR_HOST_ENTRIES)
        {
            updateEEPROMByte(ADDR_HOST_AUTO, phoneBookNumber);
                                }
        else
            errors++;
    }    
    // Set listening TCP port
    else if (strncmp(Modem_CommandBuffer, ("AT&PORT="), 8) == 0)
    {
        char numString[6];
        
        int localport = atoi(&Modem_CommandBuffer[8]);
        if (localport >= 0 && localport <= 65535)
        {
            if (setLocalPort(localport))
                while(1);            
            WiFlyLocalPort = localport;
        }
        else
            errors++;
    }    
    // List phone book entries
    else if (strstr(Modem_CommandBuffer, ("AT&PB?")) != NULL)
    {
        C64Println();
        for (int i = 0; i < ADDR_HOST_ENTRIES; i++)
        {
            C64Serial.print(i+1);
            C64Print(F(":"));
            C64Println(readEEPROMPhoneBook(ADDR_HOSTS + (i * ADDR_HOST_SIZE)));
        }
        C64Println();
        C64Print(F("Autostart: "));
        C64Serial.print(EEPROM.read(ADDR_HOST_AUTO));
        C64Println();
    }
    // Clear phone book
    else if (strstr(Modem_CommandBuffer, ("AT&PBCLEAR")) != NULL)
    {
        for (int i = 0; i < ADDR_HOST_ENTRIES; i++)
        {
            updateEEPROMPhoneBook(ADDR_HOSTS + (i * ADDR_HOST_SIZE), "\0");
        }
    }
/*    else if (strstr(Modem_CommandBuffer, ("AT&PBCLEAR")) != NULL)
    {
        for (int i = 0; i < ADDR_HOST_ENTRIES - STATIC_PB_ENTRIES; i++)
        {
            updateEEPROMPhoneBook(ADDR_HOSTS + (i * ADDR_HOST_SIZE), "\0");
        }

        // To add static entries, update STATIC_PB_ENTRIES and add entries below increased x: ADDR_HOST_ENTRIES - x
        updateEEPROMPhoneBook(ADDR_HOSTS + ((ADDR_HOST_ENTRIES - 1) * ADDR_HOST_SIZE), F("WWW.COMMODORESERVER.COM:1541"));   // last entry
        updateEEPROMPhoneBook(ADDR_HOSTS + ((ADDR_HOST_ENTRIES - 2) * ADDR_HOST_SIZE), F("BBS.JAMMINGSIGNAL.COM:23"));       // second last entry
                
        updateEEPROMByte(ADDR_HOST_AUTO, 0);
        Modem_PrintOK();
    }*/
    // Add entry to phone book
    else if (strncmp(Modem_CommandBuffer, ("AT&PB"), 5) == 0)
    {
        char numString[2];
        numString[0] = Modem_CommandBuffer[5];
        numString[1] = '\0';
        
        int phoneBookNumber = atoi(numString);
        if (phoneBookNumber >= 1 && phoneBookNumber <= ADDR_HOST_ENTRIES && Modem_CommandBuffer[6] == '=')
        {
            updateEEPROMPhoneBook(ADDR_HOSTS + ((phoneBookNumber-1) * ADDR_HOST_SIZE), Modem_CommandBuffer + 7);
        }
        else
            errors++;
    }

    else if (strncmp(Modem_CommandBuffer, ("AT"), 2) == 0)
    {
        for (int i = 2; i < strlen(Modem_CommandBuffer) && i < COMMAND_BUFFER_SIZE - 3;)
        {
            switch (Modem_CommandBuffer[i++])
            {
            case 'Z':   // ATZ
                Modem_LoadSavedSettings();
                if (wifly.isSleeping())
                    wake();
                break;

            case 'I':   // ATI
                ShowInfo(false);
                break;

                /*case 'I':
                    switch (Modem_CommandBuffer[i++])
                    {
                    case '1':
                        ShowInfo(false);
                        break;

                    default:
                        i--;                        // User entered ATI
                    case '0':
                        C64Print(F("Commodore Wi-Fi Modem Hayes Emulation v"));
                        C64Println(VERSION);
                        break;
                    }
                    break;*/

            case 'A':   // ATA
                Modem_Answer();
                suppressOkError = true;
                break;

            case 'E':   // ATE
                switch (Modem_CommandBuffer[i++])
                {
                case '0':
                    Modem_EchoOn = false;
                    break;

                case '1':
                    Modem_EchoOn = true;
                    break;

                default:
                    errors++;
                }
                break;

            case 'H':       // ATH
                switch (Modem_CommandBuffer[i++])
                {
                case '0':
                    if (wifly.isSleeping())
                        wake();
                    else
                        Modem_Disconnect(false);
                    break;

                case '1':
                    if (!wifly.isSleeping())
                        if (!wifly.sleep())
                            errors++;
                    break;

                default:
                    i--;                        // User entered ATH
                    if (wifly.isSleeping())
                        wake();
                    else
                        Modem_Disconnect(false);
                    break;
                }
                break;

            case 'O':
                if (Modem_isConnected)
                {
                    Modem_isCommandMode = false;
                }
                else
                    errors++;
                break;

            case 'Q':
                switch (Modem_CommandBuffer[i++])
                {
                case '0':
                    Modem_QuietMode = false;
                    break;

                case '1':
                    Modem_QuietMode = true;
                    break;

                default:
                    errors++;
                }
                break;

            case 'S':   // ATS
                switch (Modem_CommandBuffer[i++])
                {
                case '0':
                    switch (Modem_CommandBuffer[i++])
                    {
                    case '=':
                        switch (Modem_CommandBuffer[i++])
                        {
                        case '0':
                            Modem_S0_AutoAnswer = 0;
                            break;

                        case '1':
                            Modem_S0_AutoAnswer = 1;
                            break;

                        default:
                            errors++;
                        }
                        break;
                    }
                    break;

                case '2':
                    switch (Modem_CommandBuffer[i++])
                    {
                    case '=':
                        char numString[3] = "";

                        // Find index of last character for this setting.  Expects 1-3 numbers (ats2=43, ats2=126 etc)
                        int j = i;
                        for (int p = 0; (p < strlen(atCommandFirstChars)) && (j <= i + 2); p++)
                        {
                            if (strchr(atCommandFirstChars, Modem_CommandBuffer[j]))
                                break;
                            j++;
                        }

                        strncpy(numString, Modem_CommandBuffer + i, j - i);
                        numString[3] = '\0';

                        Modem_S2_EscapeCharacter = atoi(numString);

                        i = j;
                        break;
                    }
                    break;

                case '9':
                    switch (Modem_CommandBuffer[i++])
                    {
                    case '9':
                        switch (Modem_CommandBuffer[i++])
                        {
                        case '=':
                            switch (Modem_CommandBuffer[i++])
                            {
                            case '0':
                                Modem_suppressErrors = 0;
                                break;

                            case '1':
                                Modem_suppressErrors = 1;
                                break;
                            }
                        }
                    }
                    break;
                }
                break;

            case 'V':   // ATV
                switch (Modem_CommandBuffer[i++])
                {
                case '0':
                    Modem_VerboseResponses = false;
                    break;

                case '1':
                    Modem_VerboseResponses = true;
                    break;

                default:
                    errors++;
                }
                break;


                /*
                X0 = 0-4
                X1 = 0-5, 10
                X2 = 0-6, 10
                X3 = 0-5, 7, 10
                X4 = 0-7, 10

                0 - OK
                1 - CONNECT
                2 - RING
                3 - NO CARRIER
                4 - ERROR
                5 - CONNECT 1200
                6 - NO DIALTONE
                7 - BUSY
                8 - NO ANSWER
                10 - CONNECT 2400
                11 - CONNECT 4800
                etc..
                */
            case 'X':   // ATX
                Modem_X_Result = (Modem_CommandBuffer[i++] - 48);
                if (Modem_X_Result < 0 || Modem_X_Result > 4)
                {
                    Modem_X_Result = 0;
                    errors++;
                }

                break;

            case '&':   // AT&
                switch (Modem_CommandBuffer[i++])
                {
                case 'C':
                    switch (Modem_CommandBuffer[i++])
                    {
                    case '0':
                        Modem_DCDFollowsRemoteCarrier = false;
                        digitalWriteFast(C64_DCD, Modem_ToggleCarrier(true));
                        break;

                    case '1':
                        Modem_DCDFollowsRemoteCarrier = true;
                        if (Modem_isConnected) {
                            digitalWriteFast(C64_DCD, Modem_ToggleCarrier(true));
                        }
                        else {
                            digitalWriteFast(C64_DCD, Modem_ToggleCarrier(false));
                        }
                        break;

                    default:
                        errors++;
                    }
                    break;

                case 'F':   // AT&F
                    Modem_LoadDefaults(false);
                    if (wifly.isSleeping())
                        wake();

                    break;

                case 'K':   // AT&K
                    switch (Modem_CommandBuffer[i++])
                    {
                    case '0':
                        Modem_flowControl = false;
                        break;

                    case '1':
                        Modem_flowControl = true;
                        break;

                    default:
                        errors++;
                    }
                    break;

                case 'R':   // AT&R
                    RawTerminalMode();
                    break;

                case 'S':   // AT&S
                    switch (Modem_CommandBuffer[i++])
                    {
                    case '0':
                        //Modem_dataSetReady = 0;
                        //break;
                    case '1':
                        //Modem_dataSetReady = 1;
                        //break;
                    case '2':
                        //Modem_dataSetReady = 2;
                        Modem_dataSetReady = Modem_CommandBuffer[i-1] - '0x30';
                        Modem_DSR_Set();
                        break;

                    default:
                        errors++;
                    }
                    break;

                case 'W':   // AT&W
                    updateEEPROMByte(ADDR_MODEM_ECHO, Modem_EchoOn);
                    updateEEPROMByte(ADDR_MODEM_FLOW, Modem_flowControl);
                    updateEEPROMByte(ADDR_MODEM_VERBOSE, Modem_VerboseResponses);
                    updateEEPROMByte(ADDR_MODEM_QUIET, Modem_QuietMode);
                    updateEEPROMByte(ADDR_MODEM_S0_AUTOANS, Modem_S0_AutoAnswer);
                    updateEEPROMByte(ADDR_MODEM_S2_ESCAPE, Modem_S2_EscapeCharacter);
                    updateEEPROMByte(ADDR_MODEM_DCD, Modem_DCDFollowsRemoteCarrier);
                    updateEEPROMByte(ADDR_MODEM_X_RESULT, Modem_X_Result);
                    updateEEPROMByte(ADDR_MODEM_SUP_ERRORS, Modem_suppressErrors);
                    updateEEPROMByte(ADDR_MODEM_DSR, Modem_dataSetReady);

                    if (!(wifly.save()))
                        errors++;
                    break;

                /*case '-':   // AT&-
                    C64Println();
                    C64Serial.print(freeRam());
                    C64Println();
                    break;*/
                }
                break;

            case '*':               // AT* Moving &ssid, &pass and &key to * costs 56 flash but saves 26 mimimum RAM.
                switch (Modem_CommandBuffer[i++])
                {
                case 'M':   // AT*M     Message sent to remote side when answering
                    switch (Modem_CommandBuffer[i++])
                    {
                    case '=':
                    {
                        int j = 0;
                        for (; j < ADDR_ANSWER_MESSAGE_SIZE-1; j++)
                        {
                            EEPROM.write(ADDR_ANSWER_MESSAGE + j, (Modem_LastCommandBuffer + i)[j]);
                        }                   

                        i = strlen(Modem_LastCommandBuffer);    // Consume the rest of the line.
                        break;
                    }
                    
                    default:
                        errors++;
                    }
                    break;

                case 'S':   // AT*S
                    switch (Modem_CommandBuffer[i++])
                    {
                    case '=':
                        wifly.setSSID(Modem_LastCommandBuffer + i);

                        wifly.leave();
                        if (!wifly.join(20000))    // 20 second timeout
                            errors++;

                        i = strlen(Modem_LastCommandBuffer);    // Consume the rest of the line.
                        break;

                    default:
                        errors++;
                    }
                    break;

                case 'P':   // AT*P
                    switch (Modem_CommandBuffer[i++])
                    {
                    case '=':
                            wifly.setPassphrase(Modem_LastCommandBuffer + i);

                        i = strlen(Modem_LastCommandBuffer);    // Consume the rest of the line.
                        break;

                    default:
                        errors++;
                    }
                    break;

                case 'K':   // AT*K
                    switch (Modem_CommandBuffer[i++])
                    {
                    case '=':
                            wifly.setKey(Modem_LastCommandBuffer + i);

                        i = strlen(Modem_LastCommandBuffer);    // Consume the rest of the line.
                        break;

                    default:
                        errors++;
                    }
                default:
                    errors++;
                }
                break;

                // Dialing should come last..
                // TODO:  Need to allow for spaces after D, DT, DP.  Currently fails.
            case 'D':   // ATD
                switch (Modem_CommandBuffer[i++])
                {
                case '\0':                          /* ATD = ATO.  Probably don't need this...' */
                    if (Modem_isConnected)
                    {
                        Modem_isCommandMode = false;
                    }
                    else
                        errors++;
                    break;

                case 'T':
                case 'P':

                    switch (Modem_CommandBuffer[i++])
                    {
                    case '#':
                        // Phonebook dial
                        numString[0] = Modem_CommandBuffer[i];
                        numString[1] = '\0';

                        phoneBookNumber = atoi(numString);
                        if (phoneBookNumber >= 1 && phoneBookNumber <= ADDR_HOST_ENTRIES)
                        {
                            strncpy(address, readEEPROMPhoneBook(ADDR_HOSTS + ((phoneBookNumber - 1) * ADDR_HOST_SIZE)).c_str(), ADDR_HOST_SIZE);
                            removeSpaces(address);
                            Modem_Dialout(address);
                            suppressOkError = 1;
                        }
                        else
                            errors++;
                        break;

                    default:
                        i--;
                        removeSpaces(&Modem_CommandBuffer[i]);
                        Modem_Dialout(&Modem_CommandBuffer[i]);
                        suppressOkError = 1;
                        i = COMMAND_BUFFER_SIZE - 3;    // Make sure we don't try to process any more...
                        break;
                    }
                    break;

                case '#':
                    // Phonebook dial
                    numString[0] = Modem_CommandBuffer[i];
                    numString[1] = '\0';

                    phoneBookNumber = atoi(numString);
                    if (phoneBookNumber >= 1 && phoneBookNumber <= ADDR_HOST_ENTRIES)
                    {
                        strncpy(address, readEEPROMPhoneBook(ADDR_HOSTS + ((phoneBookNumber - 1) * ADDR_HOST_SIZE)).c_str(), ADDR_HOST_SIZE);
                        removeSpaces(address);
                        Modem_Dialout(address);
                        suppressOkError = 1;
                    }
                    else
                        errors++;
                    break;

                default:
                    i--;        // ATD
                    removeSpaces(&Modem_CommandBuffer[i]);
                    Modem_Dialout(&Modem_CommandBuffer[i]);
                    suppressOkError = 1;
                    i = COMMAND_BUFFER_SIZE - 3;    // Make sure we don't try to process any more...
                    break;
                }
                break;

            case '\n':
            case '\r':
                break;

            default:
                errors++;
            }
        }
    }
    
    if (!suppressOkError)           // Don't print anything if we just dialed out etc
    {
        if (Modem_suppressErrors || !errors)        // ats99=1 to disable errors and always print OK
            Modem_PrintOK();
        else if (errors)
            Modem_PrintERROR();        
    }   

    Modem_ResetCommandBuffer();
}

void Modem_Ring()
{
    Modem_isRinging = true;

    Modem_PrintResponse(2, F("\r\nRING"));
    if (Modem_S0_AutoAnswer != 0)
    {
        digitalWriteFast(C64_RI, HIGH);
        delay(250);
        digitalWriteFast(C64_RI, LOW);
        Modem_Answer();
    }
    else
    {
        digitalWriteFast(C64_RI, HIGH);
        delay(250);
        digitalWriteFast(C64_RI, LOW);
    }
}

void Modem_Connected(boolean incoming)
{
    if (Modem_X_Result == 0)
    {
        Modem_PrintResponse(1, F("CONNECT"));
    }
    else {
        switch(BAUD_RATE)
        {
        case 1200:
            Modem_PrintResponse(5, F("CONNECT 1200"));
            break;
        case 2400:
            Modem_PrintResponse(10, F("CONNECT 2400"));
            break;
        case 4800:
            Modem_PrintResponse(11, F("CONNECT 4800"));
            break;
        case 9600:
            Modem_PrintResponse(12, F("CONNECT 9600"));
            break;
        case 19200:
            Modem_PrintResponse(14, F("CONNECT 19200"));
            break;
        case 38400:
            Modem_PrintResponse(28, F("CONNECT 38400"));
            break;
        default:
            Modem_PrintResponse(1, F("CONNECT"));
        }
    }

    if (Modem_DCDFollowsRemoteCarrier)
        digitalWriteFast(C64_DCD, Modem_ToggleCarrier(true));
    
    if (Modem_dataSetReady == 1)
        digitalWriteFast(C64_DSR, LOW);

        
    //if (!commodoreServer38k)
    //    CheckTelnet();
    isFirstChar = true;
    telnetBinaryMode = false;

    Modem_isConnected = true;
    Modem_isCommandMode = false;
    Modem_isRinging = false;

    if (incoming) {
        //wifly.println(F("CONNECTING..."));
        if (EEPROM.read(ADDR_ANSWER_MESSAGE))
        {
            for (int j = 0; j < ADDR_ANSWER_MESSAGE_SIZE - 1; j++)
            {
                // Assuming it was stored correctly with a trailing \0
                char temp = EEPROM.read(ADDR_ANSWER_MESSAGE + j);
                if (temp == '^')
                    wifly.print("\r\n");
                else
                    wifly.print(temp);
                if (temp == 0)
                    break;
            }
            wifly.println();
        }
    }
}

void Modem_ProcessData()
{
    while (C64Serial.available() >0)
    {
        if (commodoreServer38k)
        {
            //DoFlowControlC64ToModem();

            wifly.write(C64Serial.read());
        }
        else
        {
            // Command Mode -----------------------------------------------------------------------
            if (Modem_isCommandMode)
            {
                unsigned char unsignedInbound;
                //boolean petscii_char;

                if (Modem_flowControl)
                {
                    digitalWriteFast(C64_CTS, (Modem_isCtsRtsInverted ? LOW : HIGH));
                    //digitalWriteFast(C64_CTS, LOW);
                }
                //char inbound = toupper(_serial->read());
                char inbound = C64Serial.read();
                // C64 PETSCII terminal programs send lower case 0x41-0x5a, upper case as 0xc1-0xda
                /* Real modem:
                ASCII at = OK
                ASCII AT = OK
                PET at = ok
                PET AT = OK
                */
                /*if ((inbound >= 0xc1) && (inbound <= 0xda))
                    petscii_char = true;
                inbound = charset_p_toascii_upper_only(inbound);

                if (inbound == 0)
                    return;*/


                    // Block non-ASCII/PETSCII characters
                unsignedInbound = (unsigned char)inbound;

                // Do not delete this.  Used for troubleshooting...
                //char temp[5];
                //sprintf(temp, "-%d-",unsignedInbound);
                //C64Serial.write(temp);

                if (unsignedInbound == 0x08 || unsignedInbound == 0x0a || unsignedInbound == 0x0d || unsignedInbound == 0x14) {}  // backspace, LF, CR, C= Delete
                else if (unsignedInbound <= 0x1f)
                    break;
                else if (unsignedInbound >= 0x80 && unsignedInbound <= 0xc0)
                    break;
                else if (unsignedInbound >= 0xdb)
                    break;

                if (Modem_EchoOn)
                {
                    if (!Modem_flowControl)
                        delay(100 / ((BAUD_RATE >= 2400 ? BAUD_RATE : 2400) / 2400));     // Slow down command mode to prevent garbage if flow control
                                                             // is disabled.  Doesn't work at 9600 but flow control should 
                                                             // be on at 9600 baud anyways.  TODO:  Fix
                    C64Serial.write(inbound);
                }

                if (IsBackSpace(inbound))
                {
                    if (strlen(Modem_CommandBuffer) > 0)
                    {
                        Modem_CommandBuffer[strlen(Modem_CommandBuffer) - 1] = '\0';
                    }
                }
                //else if (inbound != '\r' && inbound != '\n' && inbound != Modem_S2_EscapeCharacter)
                else if (inbound != '\r' && inbound != '\n')
                {
                    if (strlen(Modem_CommandBuffer) >= COMMAND_BUFFER_SIZE) {
                        //Display (F("CMD Buf Overflow"));
                        Modem_PrintERROR();
                        Modem_ResetCommandBuffer();
                    }
                    else {
                        // TODO:  Move to Modem_ProcessCommandBuffer?
                        if (Modem_AT_Detected)
                        {
                            Modem_CommandBuffer[strlen(Modem_CommandBuffer)] = inbound;
                        }
                        else
                        {
                            switch (strlen(Modem_CommandBuffer))
                            {
                            case 0:
                                switch (unsignedInbound)
                                {
                                case 'A':
                                case 'a':
                                case 0xC1:
                                    Modem_CommandBuffer[strlen(Modem_CommandBuffer)] = inbound;
                                    break;
                                }
                                break;
                            case 1:
                                switch (unsignedInbound)
                                {
                                case 'T':
                                case 't':
                                case '/':
                                case 0xD4:
                                    Modem_CommandBuffer[strlen(Modem_CommandBuffer)] = inbound;
                                    Modem_AT_Detected = true;
                                    break;
                                }
                                break;
                            }
                        }

                        if (toupper(charset_p_toascii_upper_only(Modem_CommandBuffer[0])) == 'A' && (Modem_CommandBuffer[1] == '/'))
                        {
                            strcpy(Modem_CommandBuffer, Modem_LastCommandBuffer);
                            if (Modem_flowControl)
                            {
                                digitalWriteFast(C64_CTS, (Modem_isCtsRtsInverted ? HIGH : LOW));
                            }
                            Modem_ProcessCommandBuffer();
                            Modem_ResetCommandBuffer();  // To prevent A matching with A/ again
                        }
                    }
                }
                // It was a '\r' or '\n'
                else if (toupper(charset_p_toascii_upper_only(Modem_CommandBuffer[0])) == 'A' && toupper(charset_p_toascii_upper_only(Modem_CommandBuffer[1])) == 'T')
                {
                    if (Modem_flowControl)
                    {
                        digitalWriteFast(C64_CTS, (Modem_isCtsRtsInverted ? HIGH : LOW));
                    }
                    Modem_ProcessCommandBuffer();
                }
                else
                {
                    Modem_ResetCommandBuffer();
                }
            }

            else    // Online ------------------------------------------
            {
                if (Modem_isConnected)
                {
                    char C64input = C64Serial.read();

                    // +++ escape
                    if (Modem_S2_EscapeCharacter < 128) // 128-255 disables escape sequence
                    {
                        if ((millis() - ESCAPE_GUARD_TIME) > escapeTimer)
                        {
                            if (C64input == Modem_S2_EscapeCharacter && lastC64input != Modem_S2_EscapeCharacter)
                            {
                                escapeCount = 1;
                                lastC64input = C64input;
                            }
                            else if (C64input == Modem_S2_EscapeCharacter && lastC64input == Modem_S2_EscapeCharacter)
                            {
                                escapeCount++;
                                lastC64input = C64input;
                            }
                            else
                            {
                                escapeCount = 0;
                                escapeTimer = millis();   // Last time non + data was read
                            }
                        }
                        else
                        {
                            escapeTimer = millis();   // Last time data was read
                        }


                        if (escapeCount == 3) {
                            Display("Escape!");
                            escapeReceived = true;
                            escapeCount = 0;
                            escapeTimer = 0;
                            Modem_isCommandMode = true;
                            C64Println();
                            Modem_PrintOK();
                        }
                    }

                    lastC64input = C64input;

                    DoFlowControlC64ToModem();

                    // If we are in telnet binary mode, write and extra 255 byte to escape NVT
                    if ((unsigned char)C64input == NVT_IAC && telnetBinaryMode)
                        wifly.write(NVT_IAC);

                    int result = wifly.write(C64input);
                }
            }
        }
    }
    if (Modem_flowControl && !commodoreServer38k)
    {
        digitalWriteFast(C64_CTS, (Modem_isCtsRtsInverted ? HIGH : LOW));
    }
}

void Modem_Answer()
{
    if (!Modem_isRinging)    // If not actually ringing...
    {
        Modem_Disconnect(true);  // This prints "NO CARRIER"
        return;
    }

    Modem_Connected(true);
}

// Main processing loop for the virtual modem.  Needs refactoring!
void Modem_Loop()
{
    while (true)
    {
        // Modem to C64 flow
        boolean wiflyIsConnected = wifly.isConnected();

        if (wiflyIsConnected && commodoreServer38k)
        {
            while (wifly.available() > 0)
            {
                DoFlowControlModemToC64();
                C64Serial.write(wifly.read());
            }
        }
        else
        {
            if (wiflyIsConnected) {
                // Check for new remote connection
                if (!Modem_isConnected && !Modem_isRinging)
                {
                    //wifly.println(F("CONNECTING..."));

                    Display(F("INCOMING\nCALL"));
                    Modem_Ring();
                    return;
                }

                // If connected, handle incoming data  
                if (Modem_isConnected)
                {
                    // Echo an error back to remote terminal if in command mode.
                    if (Modem_isCommandMode && wifly.available() > 0)
                    {
                        // If we print this, remote end gets flooded with this message 
                        // if we go to command mode on the C64 and remote side sends something..
                        //wifly.println(F("error: remote modem is in command mode."));
                    }
                    else
                    {
                        int data;

                        // Buffer for 1200 baud
                        char buffer[10];
                        int buffer_index = 0;

                        {
                            while (wifly.available() > 0)
                            {
                                int data = wifly.read();

                                // If first character back from remote side is NVT_IAC, we have a telnet connection.
                                if (isFirstChar) {
                                    if (data == NVT_IAC)
                                    {
                                        isTelnet = true;
                                        CheckTelnetInline();
                                    }
                                    else
                                    {
                                        isTelnet = false;
                                    }
                                    isFirstChar = false;
                                }
                                else
                                {
                                    if (data == NVT_IAC && isTelnet)
                                    {
                                        if (baudMismatch)
                                        {
                                            digitalWriteFast(WIFI_CTS, LOW);        // re-enable data
                                            if (CheckTelnetInline())
                                                buffer[buffer_index++] = NVT_IAC;
                                            digitalWriteFast(WIFI_CTS, HIGH);     // ..stop data from Wi-Fi
                                        }
                                        else
                                        {
                                            if (CheckTelnetInline())
                                                C64Serial.write(NVT_IAC);
                                        }

                                    }
                                    else
                                    {
                                        if (baudMismatch)
                                        {
                                            digitalWriteFast(WIFI_CTS, HIGH);     // ..stop data from Wi-Fi
                                            buffer[buffer_index++] = data;
                                        }
                                        else
                                        {
                                            DoFlowControlModemToC64();
                                            C64Serial.write(data);
                                        }
                                    }
                                }
                            }
                            if (baudMismatch)
                            {
                                // Dump the buffer to the C64 before clearing WIFI_CTS
                                if (buffer_index > 8)
                                    buffer_index = 8;
                                for (int i = 0; i < buffer_index; i++) {
                                    C64Serial.write(buffer[i]);
                                }
                                buffer_index = 0;

                                digitalWriteFast(WIFI_CTS, LOW);
                            }
                        }
                    }
                }
            }
            else  // !wiflyIsConnected
            {
                // Check for a dropped remote connection while ringing
                if (Modem_isRinging)
                {
                    Modem_Disconnect(true);
                    return;
                }

                // Check for a dropped remote connection while connected
                if (Modem_isConnected)
                {
                    Modem_Disconnect(true);
                    return;
                }
            }
        }

        // C64 to Modem flow
        //Modem_ProcessData();
        while (C64Serial.available() > 0)
        {
            if (commodoreServer38k)
            {
                DoFlowControlC64ToModem();

                wifly.write(C64Serial.read());
            }
            else
            {
                // Command Mode -----------------------------------------------------------------------
                if (Modem_isCommandMode)
                {
                    unsigned char unsignedInbound;
                    //boolean petscii_char;

                    if (Modem_flowControl)
                    {
                        // Tell the C64 to stop
                        digitalWriteFast(C64_CTS, (Modem_isCtsRtsInverted ? LOW : HIGH));       // Stop
                    }
                    //char inbound = toupper(_serial->read());
                    char inbound = C64Serial.read();
                    // C64 PETSCII terminal programs send lower case 0x41-0x5a, upper case as 0xc1-0xda
                    /* Real modem:
                    ASCII at = OK
                    ASCII AT = OK
                    PET at = ok
                    PET AT = OK
                    */
                    /*if ((inbound >= 0xc1) && (inbound <= 0xda))
                    petscii_char = true;
                    inbound = charset_p_toascii_upper_only(inbound);

                    if (inbound == 0)
                    return;*/


                    // Block non-ASCII/PETSCII characters
                    unsignedInbound = (unsigned char)inbound;

                    // Do not delete this.  Used for troubleshooting...
                    //char temp[5];
                    //sprintf(temp, "-%d-",unsignedInbound);
                    //C64Serial.write(temp);

                    if (unsignedInbound == 0x08 || unsignedInbound == 0x0a || unsignedInbound == 0x0d || unsignedInbound == 0x14) {}  // backspace, LF, CR, C= Delete
                    else if (unsignedInbound <= 0x1f)
                        break;
                    else if (unsignedInbound >= 0x80 && unsignedInbound <= 0xc0)
                        break;
                    else if (unsignedInbound >= 0xdb)
                        break;

                    if (Modem_EchoOn)
                    {
                        if (!Modem_flowControl)
                            delay(100 / ((BAUD_RATE >= 2400 ? BAUD_RATE : 2400) / 2400));     // Slow down command mode to prevent garbage if flow control
                                                                                              // is disabled.  Doesn't work at 9600 but flow control should 
                                                                                              // be on at 9600 baud anyways.  TODO:  Fix
                        C64Serial.write(inbound);
                    }

                    if (IsBackSpace(inbound))
                    {
                        if (strlen(Modem_CommandBuffer) > 0)
                        {
                            Modem_CommandBuffer[strlen(Modem_CommandBuffer) - 1] = '\0';
                        }
                    }
                    //else if (inbound != '\r' && inbound != '\n' && inbound != Modem_S2_EscapeCharacter)
                    else if (inbound != '\r' && inbound != '\n')
                    {
                        if (strlen(Modem_CommandBuffer) >= COMMAND_BUFFER_SIZE) {
                            //Display (F("CMD Buf Overflow"));
                            Modem_PrintERROR();
                            Modem_ResetCommandBuffer();
                        }
                        else {
                            // TODO:  Move to Modem_ProcessCommandBuffer?
                            if (Modem_AT_Detected)
                            {
                                Modem_CommandBuffer[strlen(Modem_CommandBuffer)] = inbound;
                            }
                            else
                            {
                                switch (strlen(Modem_CommandBuffer))
                                {
                                case 0:
                                    switch (unsignedInbound)
                                    {
                                    case 'A':
                                    case 'a':
                                    case 0xC1:
                                        Modem_CommandBuffer[strlen(Modem_CommandBuffer)] = inbound;
                                        break;
                                    }
                                    break;
                                case 1:
                                    switch (unsignedInbound)
                                    {
                                    case 'T':
                                    case 't':
                                    case '/':
                                    case 0xD4:
                                        Modem_CommandBuffer[strlen(Modem_CommandBuffer)] = inbound;
                                        Modem_AT_Detected = true;
                                        break;
                                    }
                                    break;
                                }
                            }

                            if (toupper(charset_p_toascii_upper_only(Modem_CommandBuffer[0])) == 'A' && (Modem_CommandBuffer[1] == '/'))
                            {
                                strcpy(Modem_CommandBuffer, Modem_LastCommandBuffer);
                                if (Modem_flowControl)
                                {
                                    digitalWriteFast(C64_CTS, (Modem_isCtsRtsInverted ? HIGH : LOW));       // Go
                                }
                                Modem_ProcessCommandBuffer();
                                Modem_ResetCommandBuffer();  // To prevent A matching with A/ again
                            }
                        }
                    }
                    // It was a '\r' or '\n'
                    else if (toupper(charset_p_toascii_upper_only(Modem_CommandBuffer[0])) == 'A' && toupper(charset_p_toascii_upper_only(Modem_CommandBuffer[1])) == 'T')
                    {
                        if (Modem_flowControl)
                        {
                            digitalWriteFast(C64_CTS, (Modem_isCtsRtsInverted ? HIGH : LOW));       // Go
                        }
                        Modem_ProcessCommandBuffer();
                    }
                    else
                    {
                        Modem_ResetCommandBuffer();
                    }
                }

                else    // Online ------------------------------------------
                {
                    if (Modem_isConnected)
                    {
                        char C64input = C64Serial.read();

                        // +++ escape
                        if (Modem_S2_EscapeCharacter < 128) // 128-255 disables escape sequence
                        {
                            if ((millis() - ESCAPE_GUARD_TIME) > escapeTimer)
                            {
                                if (C64input == Modem_S2_EscapeCharacter && lastC64input != Modem_S2_EscapeCharacter)
                                {
                                    escapeCount = 1;
                                    lastC64input = C64input;
                                }
                                else if (C64input == Modem_S2_EscapeCharacter && lastC64input == Modem_S2_EscapeCharacter)
                                {
                                    escapeCount++;
                                    lastC64input = C64input;
                                }
                                else
                                {
                                    escapeCount = 0;
                                    escapeTimer = millis();   // Last time non + data was read
                                }
                            }
                            else
                            {
                                escapeTimer = millis();   // Last time data was read
                            }


                            if (escapeCount == 3) {
                                Display("Escape!");
                                escapeReceived = true;
                                escapeCount = 0;
                                escapeTimer = 0;
                                Modem_isCommandMode = true;
                                C64Println();
                                Modem_PrintOK();
                            }
                        }

                        lastC64input = C64input;

                        DoFlowControlC64ToModem();

                        // If we are in telnet binary mode, write and extra 255 byte to escape NVT
                        if ((unsigned char)C64input == NVT_IAC && telnetBinaryMode)
                            wifly.write(NVT_IAC);

                        int result = wifly.write(C64input);
                    }
                }
            }
        }
        if (Modem_flowControl)
        {
            digitalWriteFast(C64_CTS, (Modem_isCtsRtsInverted ? HIGH : LOW));       // Go
        }

        //digitalWrite(DCE_RTS, HIGH);
    }
}

#endif // HAYES

void setBaudWiFi(unsigned int newBaudRate) {
  WiFly_BAUD_RATE = newBaudRate;
  WifiSerial.flush();
  delay(2);
  wifly.setBaud(newBaudRate);     // Uses set uart instant so no save and reboot needed
  delay(1000);
  WifiSerial.end();
  WifiSerial.begin(newBaudRate);

  /* After setBaud changes the baud rate it loses control
  as it doesn't see the AOK and then doesn't exit command mode
  properly.  We send this command to get things back in order.. */
  wifly.getPort();
}

void setBaudWiFi2(unsigned int newBaudRate) {
  WifiSerial.flush();
  delay(2);
  WifiSerial.end();
  WifiSerial.begin(newBaudRate);
}

int freeRam () 
{
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

long detRate(int recpin)  // function to return valid received baud rate
// Note that the serial monitor has no 600 baud option and 300 baud
// doesn't seem to work with version 22 hardware serial library
{
    unsigned long timer;
    timer = millis();

    while (1)
    {
      if (millis() - 3000 < timer)
          return (0);
      if (digitalRead(recpin) == 0)
          break;
    }
    long baud;
    long rate = pulseIn(recpin, LOW); // measure zero bit width from character. ASCII 'U' (01010101) provides the best results.

    //if (rate < 12)
        //baud = 115200;
    //else if (rate < 20)
        //baud = 57600;
    //else 
        if (rate < 29)
        baud = 38400;
    //else if (rate < 40)
        //baud = 28800;
    else if (rate < 60)
        baud = 19200;
    //else if (rate < 80)
        //baud = 14400;
    else if (rate < 150)
        baud = 9600;
    else if (rate < 300)
        baud = 4800;
    else if (rate < 600)
        baud = 2400;
    else if (rate < 1200)
        baud = 1200;
    else
        baud = 0;
    return baud;
}

void updateEEPROMByte(int address, byte value)
{
    if (EEPROM.read(address) != value)
        EEPROM.write(address, value);
}

void updateEEPROMPhoneBook(int address, String host)
{
    int i=0;
    for (; i < 38; i++)
    {
        EEPROM.write(address + i, host.c_str()[i]);
    }
}

String readEEPROMPhoneBook(int address)
{
    char host[ADDR_HOST_SIZE-2];
    int i=0;
    for (; i < ADDR_HOST_SIZE-2; i++)
    {
        host[i] = EEPROM.read(address + i);
    }
    return host;
}


void processC64Inbound()
{
    char C64input = C64Serial.read();

    if ((millis() - ESCAPE_GUARD_TIME) > escapeTimer)
        {

        if (C64input == Modem_S2_EscapeCharacter && lastC64input != Modem_S2_EscapeCharacter)
        {
            escapeCount = 1;
            lastC64input = C64input;
        }
        else if (C64input == Modem_S2_EscapeCharacter && lastC64input == Modem_S2_EscapeCharacter)
        {
            escapeCount++;
            lastC64input = C64input;
        }
        else
        {
            escapeCount = 0;
            escapeTimer = millis();   // Last time non + data was read
        }
    }
    else
        escapeTimer = millis();   // Last time data was read
    
    if (escapeCount == 3) {
        Display(F("Escape!"));
        escapeReceived = true;
        escapeCount = 0;
        escapeTimer = 0;
    }

    // If we are in telnet binary mode, write and extra 255 byte to escape NVT
    if ((unsigned char)C64input == NVT_IAC && telnetBinaryMode)
        wifly.write(NVT_IAC);

    DoFlowControlC64ToModem();
    wifly.write(C64input);
}

void removeSpaces(char *temp)
{
  char *p1 = temp;
  char *p2 = temp;

  while(*p2 != 0)
  {
    *p1 = *p2++;
    if (*p1 != ' ')
      p1++;
  }
  *p1 = 0;
}

boolean setLocalPort(int localport)
{
    if(WiFlyLocalPort != localport)
    {
        wifly.setPort(localport);
        wifly.save();
        WiFlyLocalPort = localport;
        
        if (WiFly_BAUD_RATE != 2400) {
            C64Println(F("\n\rReboot MicroView & WiFi to set new port."));
            return true;
        }
        else {
            C64Println(F("\n\rRebooting WiFi..."));
            wifly.reboot();
            delay(5000);
            configureWiFly();
            return false;
        }
    }
    else
        return false;
}

int Modem_ToggleCarrier(boolean isHigh)
{
    // We get a TRUE (1) if we want to activate DCD which is a logic LOW (0) normally.
    // So if not inverted, send !isHigh.  TODO:  Clean this up.
    if (Modem_isDcdInverted)
        return(isHigh);
    else
        return(!isHigh);
}

#ifdef HAYES
/* C64 CTS and RTS are inverted.  To allow data to flow from the C64, 
   set CTS on the C64 to HIGH.  Normal computer would be LOW.
   Function not currently in use.  Tried similar function for RTS
   but caused issues with CommodoreServer at 38,400.
   */
void C64_SetCTS(boolean value)
{
    if (Modem_isCtsRtsInverted)
        value = !value;
    digitalWriteFast(C64_CTS, value);
}
#endif

void DisplayPhoneBook() {
    for (int i = 0; i < ADDR_HOST_ENTRIES; i++)
    {
        C64Serial.print(i+1);
        C64Print(F(":"));
        C64Println(readEEPROMPhoneBook(ADDR_HOSTS + (i * ADDR_HOST_SIZE)));
    }
    C64Println();
    C64Print(F("Autostart: "));
    C64Serial.print(EEPROM.read(ADDR_HOST_AUTO));
    C64Println();
}

void Modem_Dialout(char* host)
{
    char* index;
    uint16_t port = 23;
    String hostname = String(host);

        if (strlen(host) == 0)
        {
#ifdef HAYES
            Modem_PrintERROR();
#endif
            return;
        }
    
        if ((index = strstr(host, ":")) != NULL)
        {
            index[0] = '\0';
            hostname = String(host);
            port = atoi(index + 1);
        }

    lastHost = hostname;
    lastPort = port;

    Connect(hostname, port, false);
}

void configureWiFly() {
    // Enable DNS caching, TCP retry, TCP_NODELAY, TCP connection status
    wifly.setIpFlags(16 | 4 | 2 | 1);           // 23  // Does this require a save?
    wifly.setIpProtocol(WIFLY_PROTOCOL_TCP);    // Does this require a save?  If so, remove and set once on WiFly and document.

}

void wake()
{
    setBaudWiFi2(2400);
    wifly.wake();
    //configureWiFly();
    //setBaudWiFi(WiFly_BAUD_RATE);
    //isSleeping = false;
}

void Modem_DSR_Set()
{
    if (Modem_dataSetReady != 2)
    {
        pinModeFast(C64_DSR, OUTPUT);
        switch (Modem_dataSetReady)
        {
        case 0:     // 0=Force DSR signal active
            pinModeFast(C64_DSR, OUTPUT);
            digitalWriteFast(C64_DSR, HIGH);
            break;
        case 1:
            // 1=DSR active according to the CCITT specification.
            // DSR will become active after answer tone has been detected 
            // and inactive after the carrier has been lost.
            pinModeFast(C64_DSR, OUTPUT);
            if (Modem_isConnected)
            {
                digitalWriteFast(C64_DSR, HIGH);
            }
            else
            {
                digitalWriteFast(C64_DSR, LOW);
            }
            break;
        }
        
    }
}



// #EOF - Leave this at the very end...

