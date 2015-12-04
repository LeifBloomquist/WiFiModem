/*
    Commodore 64 - MicroView - Wi-Fi Cart
    Authors: Leif Bloomquist and Alex Burger
    With assistance and code from Greg Alekel and Payton Byrd
*/

/* TODO
 *  set ip tcp-mode 0x10 to disable remote configuration
 *  wifly.setBroadcastInterval(0);
 *  wifly.setDeviceID("Wifi-Modem");
 *  
 *  
 *  
 */


//#define DEBUG

// Defining HAYES enables Hayes commands and disables the 1) and 2) menu options for telnet and incoming connections.
// This is required to ensure the compiled code is <= 30,720 bytes 
//#define HAYES
//#define ENABLE_C64_DCD
#include <MicroView.h>
#include <elapsedMillis.h>
#include <SoftwareSerial.h>
#include <WiFlyHQ.h>
#include <EEPROM.h>
#include <digitalWriteFast.h>
#include "petscii.h"

;  // Keep this here to pacify the Arduino pre-processor

#define VERSION "0.06b1"

// Configuration 0v3: Wifi Hardware, C64 Software.

int BAUD_RATE=2400;
//#define BAUD_RATE 2400
int WiFly_BAUD_RATE=2400;
//#define WiFly_BAUD_RATE 2400
#define MIN_FLOW_CONTROL_RATE 4800  // If BAUD rate is this speed or higher, RTS/CTS flow control is enabled.

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
#define IAC 255
String lastHost = "";
int lastPort = 23;

char escapeCount = 0;
int lastC64input = 0;
unsigned long escapeTimer = 0;
boolean escapeReceived = false;
#define ESCAPE_GUARD_TIME 1000

char autoConnectHost = 0;
boolean autoConnectedAtBootAlready = 0;           // We only want to auto-connect once..
#define ADDR_HOST_SIZE     40
#define ADDR_HOST_ENTRIES  9
    
// EEPROM Addresses
#define ADDR_PETSCII       0
#define ADDR_AUTOSTART     1
#define ADDR_BAUD_LO       2
#define ADDR_BAUD_HI       3
#define ADDR_MODEM_ECHO         10
#define ADDR_MODEM_FLOW         11
#define ADDR_MODEM_VERBOSE      12
#define ADDR_MODEM_QUIET        13
#define ADDR_MODEM_S0_AUTOANS   14
#define ADDR_MODEM_S2_ESCAPE    15
#define ADDR_MODEM_DCD_INVERTED 16

#define ADDR_HOST_AUTO          99     // Autostart host number
#define ADDR_HOSTS              100    // to 299 with ADDR_HOST_SIZE = 40 and ADDR_HOST_ENTRIES = 5
//#define ADDR_xxxxx            300

// Hayes variables
#ifdef HAYES
boolean Modem_isCommandMode = true;
boolean Modem_isConnected = false;
boolean Modem_isRinging = false;
boolean Modem_EchoOn = true;
//boolean Modem_EchoOn = true;
boolean Modem_VerboseResponses = true;
boolean Modem_QuietMode = false;
boolean Modem_isDcdInverted = false;
boolean Modem_S0_AutoAnswer = false;
char    Modem_S2_EscapeCharacter = '+';
#endif    // HAYES

#define COMMAND_BUFFER_SIZE  81
char Modem_LastCommandBuffer[COMMAND_BUFFER_SIZE];
char Modem_CommandBuffer[COMMAND_BUFFER_SIZE];

int Modem_EscapeCount = 0;

// Misc Values
#define TIMEDOUT  -1
boolean baudMismatch = (BAUD_RATE != WiFly_BAUD_RATE ? 1 : 0);
boolean Modem_flowControl = false;   // for &K setting.

// PETSCII state
boolean petscii_mode = EEPROM.read(ADDR_PETSCII);

// Autoconnect Options
#define AUTO_NONE     0
#define AUTO_HAYES    1
#define AUTO_CSERVER  2
#define AUTO_QLINK    3
#define AUTO_CUSTOM   100   // TODO

#ifndef HAYES
byte autostart_mode = EEPROM.read(ADDR_AUTOSTART);    // 0-255 only
#endif

// ----------------------------------------------------------
// Arduino Setup Function

int main(void)
{
    init();
    uView.begin(); // begin of MicroView
    uView.setFontType(0);
    uView.clear(ALL); // erase hardware memory inside the OLED controller
    
    BAUD_RATE = (EEPROM.read(ADDR_BAUD_LO) * 256 + EEPROM.read(ADDR_BAUD_HI));
    if (BAUD_RATE != 1200 && BAUD_RATE != 2400 && BAUD_RATE != 4800 && BAUD_RATE != 9600 && BAUD_RATE != 19200)
        BAUD_RATE = 2400;

    //
    // Baud rate detection
    //
    pinMode (C64_RxD, INPUT);
    digitalWrite (C64_RxD, HIGH);

    Display(F("Baud\ndetection"));

    long detectedBaudRate = detRate(C64_RxD);  // Function finds a standard baudrate of either
    // 1200,2400,4800,9600,14400,19200,28800,38400,57600,115200
    // by having sending circuit send "U" characters.
    // Returns 0 if none or under 1200 baud

    //char temp[20];
    //sprintf(temp, "Baud\ndetected:\n%ld", detectedBaudRate);
    //Display(temp);

    if (detectedBaudRate == 1200 || detectedBaudRate == 2400 || detectedBaudRate == 4800 || detectedBaudRate == 9600 || detectedBaudRate == 19200) {
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

    //setBaud(BAUD_RATE, false, true);

    //
    // Baud rate detection end
    //

        C64Serial.begin(BAUD_RATE);
        WifiSerial.begin(WiFly_BAUD_RATE);
    
        C64Serial.setTimeout(1000);

        baudMismatch = (BAUD_RATE != WiFly_BAUD_RATE ? 1 : 0);

        // Always setup pins for flow control
        pinModeFast(C64_RTS, INPUT);
        pinModeFast(C64_CTS, OUTPUT);
    
        pinModeFast(WIFI_RTS, INPUT);
        pinModeFast(WIFI_CTS, OUTPUT);
    
        // Force CTS to defaults on both sides to start, so data can get through
        digitalWriteFast(WIFI_CTS, LOW);
        digitalWriteFast(C64_CTS, HIGH);  // C64 is inverted
       
        //WifiSerial.begin(WiFly_BAUD_RATE);

  /*
        BAUD_RATE = (EEPROM.read(ADDR_BAUD_LO) * 256 + EEPROM.read(ADDR_BAUD_HI));
        if (BAUD_RATE != 1200 && BAUD_RATE != 2400 && BAUD_RATE != 9600 && BAUD_RATE != 38400)
            BAUD_RATE = 2400;
  */

        //C64Serial.begin(BAUD_RATE);

        //delay(1000);

        DisplayBoth(F("Wi-Fi Init..."));

#ifdef DEBUG
        boolean ok = wifly.begin(&WifiSerial, &C64Serial);
#else
        boolean ok = wifly.begin(&WifiSerial);
#endif
    
        if (ok)
        {
            DisplayBoth(F("Wi-Fi OK!"));
        }
        else
        {
            DisplayBoth(F("Wi-Fi Failed!"));
            RawTerminalMode();
        }

        // Enable DNS caching, TCP retry, TCP_NODELAY, TCP connection status
            wifly.setIpFlags(16 && 4 && 2 && 1);
    
        if (BAUD_RATE != 2400) {
            delay(1000);
            if(BAUD_RATE == 1200)
                setBaudWiFi(2400);
            else
                setBaudWiFi(BAUD_RATE);
        }
        baudMismatch = (BAUD_RATE != WiFly_BAUD_RATE ? 1 : 0);
      
        wifly.close();

#ifdef ENABLE_C64_DCD
        digitalWriteFast(C64_DCD, true);    // true = no carrier
#endif    

#ifndef HAYES        
        Modem_flowControl = EEPROM.read(ADDR_MODEM_FLOW);
        HandleAutoStart();
#endif  // HAYES
    
        C64Println();
#ifdef HAYES
        C64Println(F("Commodore Wi-Fi Modem Hayes Emulation"));
        ShowPETSCIIMode();
        ShowInfo(true);
        autoConnectHost = EEPROM.read(ADDR_HOST_AUTO);
        HayesEmulationMode();
#else
        C64Println(F("Commodore Wi-Fi Modem"));
        ShowInfo(true);

    while (1)
    {
        Display(F("READY."));
    
        ShowPETSCIIMode();
        C64Println(F("1. Telnet to host or BBS"));
        C64Println(F("2. Wait for incoming connection"));
        C64Println(F("3. Configuration"));
        C64Println();
        C64Print(F("Select: "));
    
        int option = ReadByte(C64Serial);
        C64Serial.println((char)option);
    
        switch (option)
        {
        case '1':
            DoTelnet();
            break;
    
        case '2':
            Incoming();
            break;
       
        case '3':
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
        C64Println();
        C64Println(F("Configuration Menu"));
        C64Println();
        C64Println(F("1. Display Current Configuration"));
        C64Println(F("2. Change SSID"));
        sprintf_P(temp,PSTR("3. %s flow control"),Modem_flowControl == true ? "Disable" : "Enable");        
        C64Println(temp);      
        C64Println(F("4. Autostart Options"));      
        C64Println(F("5. Direct Terminal Mode (Debug)"));
        C64Println(F("6. Return to Main Menu"));
        C64Println();
        C64Print(F("Select: "));

        int option = ReadByte(C64Serial);
        C64Serial.println((char)option);

        switch (option)
        {
        case '1': ShowInfo(false);
            break;

        case '2': ChangeSSID();
            break;

#ifndef HAYES
        case '3':
            Modem_flowControl = !Modem_flowControl;
            updateEEPROMByte(ADDR_MODEM_FLOW,Modem_flowControl);
            break;

        case '4': ConfigureAutoStart();
            break;
#endif

/*      case '4': ConfigureBaud();
            break;*/

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
        C64Println();
        C64Println(F("Change SSID"));
        C64Println();
        C64Println(F("1. WEP"));
        C64Println(F("2. WPA / WPA2"));
        C64Println(F("3. Return to Configuration Menu"));
        C64Println();
        C64Print(F("Select: "));

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
        wifly.setSSID(input.c_str());   // Note inverted, not sure why this has to be
        wifly.save();
        wifly.leave();
            if (wifly.join(20000))    // 20 second timeout
            {
                C64Println();
                C64Println(F("SSID Successfully changed"));
                return;
            }
            else
            {
                C64Println();
                C64Println(F("Error joining network"));
                continue;
            }
    }
}
#endif  // HAYES

// ----------------------------------------------------------
// MicroView Display helpers

void Display(String message)
{
    uView.clear(PAGE); // erase the memory buffer, when next uView.display() is called, the OLED will be cleared.
    uView.setCursor(0, 0);
    uView.println(message);
    uView.display();
}

// Pointer version.  Does not work with F("") or PSTR("").  Use with sprintf and sprintf_P
void DisplayP(const char *message)
{
    uView.clear(PAGE); // erase the memory buffer, when next uView.display() is called, the OLED will be cleared.
    uView.setCursor(0, 0);
    uView.println(message);
    uView.display();
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
    C64PrintlnP(message);
    //message.replace(' ', '\n');
    DisplayP(message);
}


// ----------------------------------------------------------
// Wrappers that always print correctly for ASCII/PETSCII

void C64Print(String message)
{
    if (petscii_mode)
    {
        C64Serial.print(petscii::ToPETSCII(message.c_str()));
    }
    else
    {
        C64Serial.print(message);
    }
}

void C64Println(String message)
{
    C64Print(message);
    C64Serial.println();
}

// Pointer version.  Does not work with F("") or PSTR("").  Use with sprintf and sprintf_P
void C64PrintP(const char *message)
{
    if (petscii_mode)
    {
        C64Serial.print(petscii::ToPETSCII(message));
    }
    else
    {
        C64Serial.print(message);
    }
}

// Pointer version.  Does not work with F("") or PSTR("").  Use with sprintf and sprintf_P
void C64PrintlnP(const char *message)
{
    C64PrintP(message);
    C64Serial.println();
}

inline void C64Println()
{
    C64Serial.println();
}

void ShowPETSCIIMode()
{
    C64Println();

    if (petscii_mode)
    {
        char message[] =
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

#ifndef HAYES
    C64Println(F(" (Del to switch)"));
#endif  // HAYES

    C64Println();
}


void SetPETSCIIMode(boolean mode)
{
    petscii_mode = mode;
    updateEEPROMByte(ADDR_PETSCII,petscii_mode);
}


// ----------------------------------------------------------
// User Input Handling

boolean IsBackSpace(char c)
{
    if ((c == 8) || (c == 20))
    {
        return true;
    }
    else
    {
        return false;
    }
}

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
        key = ReadByte(C64Serial); // Read in one character
        temp[i] = key;
        C64Serial.write(key); // Echo key press back to the user.

       // if ((key == '\b') && (i > 0)) i -= 2; // Handles back space.

        if (IsBackSpace(key) && (i > 0)) i -= 2; // Handles back space.        

        if (((int)key == 13) || (i == max_length - 1))   // The 13 represents enter key.
        {
            temp[i] = 0; // Terminate the string with 0.
            return String(temp);
        }
        i++;
        if (i < 0) i = 0;
    }
}

// ----------------------------------------------------------
// Show Configuration

void ShowInfo(boolean powerup)
{
    char mac[20];
    char ip[20];
    char ssid[20];

    wifly.getMAC(mac, 20);    // Sometimes the first time contains garbage..
    wifly.getMAC(mac, 20);
    wifly.getIP(ip, 20);
    wifly.getSSID(ssid, 20);

    C64Println();
    C64Print(F("MAC Address: "));    C64Println(mac);
    C64Print(F("IP Address:  "));    C64Println(ip);
    C64Print(F("Wi-Fi SSID:  "));    C64Println(ssid);
    C64Print(F("Firmware:    "));    C64Println(VERSION);
#ifdef HAYES
    if (!powerup) {
        char at_settings[20];
        sprintf_P(at_settings, PSTR("ATE%dV%dQ%d&K%dS0=%d"),Modem_EchoOn,Modem_VerboseResponses,Modem_QuietMode,Modem_flowControl,Modem_S0_AutoAnswer);
        C64Print(F("Init string: "));    C64Println(at_settings);
    }
#endif

    if (powerup)
    {
        char temp[50];

        sprintf_P(temp, PSTR("Firmware\n\n%s"), VERSION);
        Display(temp);
        delay(1000);

        sprintf_P(temp, PSTR("Baud Rate\n\n%d"), BAUD_RATE);
        Display(temp);
        delay(1000);

        sprintf_P(temp, PSTR("IP Address \n%s"), ip);
        Display(temp);
        delay(1000);

        sprintf_P(temp, PSTR("SSID\n\n%s"), ssid);
        Display(temp);
        delay(1000);
    }
}

#ifndef HAYES
// ----------------------------------------------------------
// Simple Incoming connection handling

void Incoming()
{
    int localport = wifly.getPort();

    C64Println();
    C64Print(F("Incoming port ("));
    C64Serial.print(localport);
    C64Print(F("): "));

    String strport = GetInput();

    if (strport.length() > 0)
    {
        localport = strport.toInt();
        if (setLocalPort(localport))
            while(1);            
    }

    localport = wifly.getPort();

    C64Println();
    C64Print(F("Waiting for connection on port "));
    C64Serial.println(localport);

    // Idle here until connected
    while (!wifly.isConnected())  {}

    C64Println(F("Incoming Connection"));

    TerminalMode();
}
// ----------------------------------------------------------
// Telnet Handling


void DoTelnet()
{
    int port = lastPort;

    C64Print(F("\nTelnet host ("));
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
    C64Print(F("\nPort ("));
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
    char temp[50];
    sprintf_P(temp, PSTR("\nConnecting to %s"), host.c_str());
#ifdef HAYES
    DisplayP(temp);
#else
    DisplayBothP(temp);
#endif

    wifly.setIpProtocol(WIFLY_PROTOCOL_TCP);

    /* Do a DNS lookup to get the IP address.  Lookup has a 5 second timeout. */
    char ip[16];
    if (!wifly.getHostByName(host.c_str(), ip, sizeof(ip))) {
       DisplayBoth(F("Could not resolve DNS.  Connect Failed!"));
#ifdef HAYES
       Modem_Disconnect();
#endif
       return;
    }

    boolean ok = wifly.open(ip, port);

    if (ok)
    {
        sprintf_P(temp, PSTR("Connected to %s"), host.c_str());
#ifdef HAYES
        DisplayP(temp);
#else
        DisplayBothP(temp);
#endif
#ifdef ENABLE_C64_DCD
        digitalWriteFast(C64_DCD, false);
#endif
    }
    else
    {
#ifdef HAYES
        Display(F("Connect Failed!"));
        Modem_Disconnect();
#else
        DisplayBoth(F("Connect Failed!"));
#endif
        return;
    }

    if (!raw)
    {
        CheckTelnet();
    }

    /*
    // Display free RAM.  When this was ~184, Microview wouldn't display..
    char temp3[10];
    itoa(freeRam(), temp3, 10);
    DisplayP(temp3);
    delay (2000);*/
    
#ifdef HAYES
    Modem_Connected();
#endif
    TerminalMode();
}

void TerminalMode()
{
    int data;
    char buffer[100];
    int buffer_index = 0;
    int buffer_bytes = 0;
    int max_buffer_size_reached = 0;

    
    while (wifly.available() != -1) // -1 means closed
    {
        //if (1) {   // Assumes host is slower than WiFly
        if (baudMismatch) {   // Assumes host is slower than WiFly
            while (wifly.available() > 0)
            {
                digitalWriteFast(WIFI_CTS, HIGH);     // ..stop data from Wi-Fi
    
                data = wifly.read();
                buffer[buffer_index++] = data;
                if (buffer_index > 99)
                  buffer_index = 99;
            }
    
            // Dump the buffer to the C64 before clearing WIFI_CTS
            for(int i=0; i<buffer_index; i++) {
                // Always do flow control if baudMismatch
                // Check that C64 is ready to receive
            //while (digitalReadFast(C64_RTS) == LOW);   // If not...  C64 RTS and CTS are inverted.
                C64Serial.write(buffer[i]);

                while (C64Serial.available() > 0)
                {
                    processC64Inbound();
                }
            }

            /*
            // Only used for displaying max buffer size on Microview for testing
            if (max_buffer_size_reached < buffer_index)
                max_buffer_size_reached = buffer_index;
            char message[20];
            sprintf_P(message, PSTR("Buf: %d"), max_buffer_size_reached);
            Display(message);*/
            
            // Show largest buffer size on Microview
            // 4800/9600 = 3
            // 4800/19200 = 5
            // 1200/19200 = doesn't work
            // 4800/38400 = 12
            // 2400/38400 = 40 - bbs.jammingsignal.com:23 (1200 baud) worked.
            // 2400/38400 = 40 - cib.dyndns.com:6400 (19200 baud) worked.
            // 2400/38400 = 40 - Linux telnet did not work with Novaterm.  Default flow tolerance = 200.  Set to 100 and sometimes connected.
            // 9600/38400 = 11           

            buffer_index = 0;
              
            digitalWriteFast(WIFI_CTS, LOW);
        }
        else {
            while (wifly.available() > 0)
            {
                DoFlowControl();
                C64Serial.write(wifly.read());
            }
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
    Modem_Disconnect();
#else
    DisplayBoth(F("Connection closed"));
#endif
#ifdef ENABLE_C64_DCD
    digitalWriteFast(C64_DCD, false);
#endif
}

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
            DoFlowControl();

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
            if (Modem_flowControl || BAUD_RATE >= MIN_FLOW_CONTROL_RATE || baudMismatch)
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

inline void DoFlowControl()
{
   
    if (Modem_flowControl || BAUD_RATE >= MIN_FLOW_CONTROL_RATE || baudMismatch)
    {
        // Check that C64 is ready to receive
        while (digitalReadFast(C64_RTS) == LOW)   // If not...  C64 RTS and CTS are inverted.
        {
            digitalWriteFast(WIFI_CTS, HIGH);     // ..stop data from Wi-Fi and wait
        }
        digitalWriteFast(WIFI_CTS, LOW);
    }
}

void CheckTelnet()     //  inquiry host for telnet parameters / negotiate telnet parameters with host
{
    int inpint, verbint, optint;        //    telnet parameters as integers

    // Wait for first character for 10 seconds
    inpint = PeekByte(wifly, 10000);

    if (inpint != IAC)
    {
        return;   // Not a telnet session
    }

    // IAC handling
    SendTelnetParameters();    // Start off with negotiating

    while (true)
    {
        inpint = PeekByte(wifly, 5000);       // peek at next character, timeout after 5 second

            if (inpint != IAC)
            {
                return;   // Let Terminal mode handle character
            }

        inpint = ReadByte(wifly);    // Eat IAC character
        verbint = ReadByte(wifly);   // receive negotiation verb character
        if (verbint == -1) continue;                //          if  negotiation verb character is nothing break the routine(should never happen)

            switch (verbint) {                             //   evaluate negotiation verb character
            case IAC:                                    //          if    negotiation verb character is 255 (restart negotiation)
                    continue;                                     //break the routine

            case 251:                                    //          if     negotiation verb character is 251 (will)or
            case 252:                                    //          if       negotiation verb character is 252 (wont)or
            case 253:                                    //          if      negotiation verb character is 253 (do) or
            case 254:                                    //          if      negotiation verb character is 254 (dont)
                    optint = ReadByte(wifly);                  //               receive negotiation option character
                    if (optint == -1) continue;                   //            if             negotiation option character is nothing break the routine(should       never happen)

                    switch (optint) {

                    case 3:                                   // if negotiation                        option character is 3 (suppress - go - ahead)
                            SendTelnetDoWill(verbint, optint);
                        break;

                    default:                                     //        if                        negotiation option character is none of the above(all others)
                            SendTelnetDontWont(verbint, optint);
                        break;                                     //  break the routine
                }
        }
    }
}

void SendTelnetDoWill(int verbint, int optint)
{
    wifly.write(IAC);                           // send character 255 (start negotiation)
    wifly.write(verbint == 253 ? 253 : 251);    // send character 253  (do) if negotiation verb character was 253 (do) else send character 251 (will)
    wifly.write(optint);
}

void SendTelnetDontWont(int verbint, int optint)
{
    wifly.write(IAC);                           // send character 255   (start negotiation)
    wifly.write(verbint == 253 ? 252 : 254);    // send character 252   (wont) if negotiation verb character was 253 (do) else send character254 (dont)
    wifly.write(optint);
}

void SendTelnetParameters()
{
    wifly.write(IAC);                           // send character 255 (start negotiation) 
    wifly.write(254);                           // send character 254 (dont)
    wifly.write(34);                            // linemode

    wifly.write(IAC);                           // send character 255 (start negotiation)
    wifly.write(254);                           // send character 253 (do)
    wifly.write(1);                             // echo
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
    /*unsigned long timer;
    timer = millis();

    while (in.available() == 0)
    {
      if (millis() - timeout < timer)
          return TIMEDOUT;
    }*/
 
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
// ----------------------------------------------------------
// Autoconnect Handling

void ConfigureAutoStart()
{
    while (true)
    {
        C64Println();
        C64Print(F("Autostart Menu (Current: "));
        C64Serial.print(autostart_mode);
        C64Println(F(")"));
        C64Println();
        C64Println(F("0. Clear Autostart Options"));
#ifdef HAYES
        C64Println(F("1. Hayes Emulation Mode"));
        C64Println(F("2. CommodoreServer"));
//        C64Println(F("3. QuantumLink Reloaded"));
        C64Println(F("3. Return to Main Menu"));
#else
        C64Println(F("1. CommodoreServer"));
//        C64Println(F("2. QuantumLink Reloaded"));
        C64Println(F("2. Return to Main Menu"));
#endif // HAYES

        C64Println();
        C64Print(F("Select: "));

        int option = ReadByte(C64Serial);
        C64Serial.println((char)option);

        switch (option)
        {
        case '0': autostart_mode = AUTO_NONE;
            break;
#ifdef HAYES
        case '1': autostart_mode = AUTO_HAYES;
            break;

        case '2': autostart_mode = AUTO_CSERVER;
            break;

//        case '3': autostart_mode = AUTO_QLINK;
//            break;

//        case '4': return;
        case '3': return;
#else
        case '1': autostart_mode = AUTO_CSERVER;
            break;

//        case '2': autostart_mode = AUTO_QLINK;
//            break;

//        case '3': return;
        case '2': return;
#endif // HAYES

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

        updateEEPROMByte(ADDR_AUTOSTART,autostart_mode);
        C64Println(F("Saved"));
    }
}
#endif // HAYES

#ifndef HAYES
void HandleAutoStart()
{
    // Display chosen action

    switch (autostart_mode)
    {
        case AUTO_NONE:  // No Autostart, continue as normal
            return;

#ifdef HAYES
        case AUTO_HAYES: // Hayes Emulation
            C64Println(F("Entering Hayes Emulation Mode"));
            break;

#endif // HAYES
        case AUTO_CSERVER: // Commodore Server
            C64Println(F("Connecting to CommodoreServer"));
            break;

//        case AUTO_QLINK: // QuantumLink Reloaded
//            C64Println(F("Ready to Connect to QuantumLink Reloaded"));          
//            break;

        default: // Invalid - on first startup or if corrupted.  Clear silently and continue to menu.
            updateEEPROMByte(ADDR_AUTOSTART,AUTO_NONE);
            autostart_mode = AUTO_NONE;
            return;
    }

    // Wait for user to cancel

    C64Println(F("Press any key to cancel..."));

    int option = PeekByte(C64Serial, 2000);

    if (option != TIMEDOUT)   // Key pressed
    {
        ReadByte(C64Serial);    // eat character
        return;
    }

    // Take the chosen action

    switch (autostart_mode)
    {
#ifdef HAYES
        case AUTO_HAYES: // Hayes Emulation
            HayesEmulationMode();
            break;
#endif // HAYES
        case AUTO_CSERVER: // CommodoreServer - just connect repeatedly
            while (true)
            {
                Connect(F("www.commodoreserver.com"), 1541, true);
                delay(1000);
            }
            break;

//        case AUTO_QLINK: // QuantumLink Reloaded
//            // !!!! Not 100% sure what to do here.  Need 1200 baud.
//            C64Println(F("TODO: Not Implemented!"));  //  !!!! TODO
//            return;   // !!!!

        default: // Shouldn't ever reach here.  Just continue to menu if so.
            return;
    }
}
#endif  // HAYES

#ifdef HAYES
// ----------------------------------------------------------
// Simple Hayes Emulation
// Portions of this code are adapted from Payton Byrd's Hayesduino - thanks!

void HayesEmulationMode()
{
    pinModeFast(C64_RI, OUTPUT);
    pinModeFast(C64_DSR, OUTPUT);
    pinModeFast(C64_DTR, OUTPUT);
    pinModeFast(C64_DCD, OUTPUT);
    pinModeFast(C64_RTS, INPUT);

    digitalWriteFast(C64_RI, LOW);
    digitalWriteFast(C64_DSR, HIGH);
    digitalWriteFast(C64_DTR, Modem_ToggleCarrier(false));

    Modem_EscapeCount = 0;

    Modem_LoadDefaults();

    // Load saved settings
    Modem_EchoOn = EEPROM.read(ADDR_MODEM_ECHO);
    Modem_flowControl = EEPROM.read(ADDR_MODEM_FLOW);
    Modem_VerboseResponses = EEPROM.read(ADDR_MODEM_VERBOSE);
    Modem_QuietMode = EEPROM.read(ADDR_MODEM_QUIET);
    Modem_S0_AutoAnswer = EEPROM.read(ADDR_MODEM_S0_AUTOANS);
    Modem_S2_EscapeCharacter = EEPROM.read(ADDR_MODEM_S2_ESCAPE);
    Modem_isDcdInverted = EEPROM.read(ADDR_MODEM_DCD_INVERTED);
    Modem_ResetCommandBuffer();

    C64Println();
    DisplayBoth(F("OK"));

    while (true)
    {
        Modem_Loop();
    }
}


inline void Modem_PrintOK()
{
    Modem_PrintResponse("0", F("OK"));
}

inline void Modem_PrintERROR()
{
    Modem_PrintResponse("4", F("ERROR"));
}

void Modem_PrintResponse(const char* code, const __FlashStringHelper * msg)
{
    C64Println();

    if (!Modem_QuietMode)
    {
        if (Modem_VerboseResponses)
            C64Println(msg);
        else
            C64Println(code);
    }

    // Always show verbose version on OLED, underneath command
    uView.println();
    uView.println(msg);
    uView.display();
}

void Modem_ResetCommandBuffer()
{
    memset(Modem_CommandBuffer, 0, COMMAND_BUFFER_SIZE);
}


void Modem_LoadDefaults(void)
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
    Modem_isDcdInverted = false;
    Modem_flowControl = false;
}


int Modem_ToggleCarrier(boolean isHigh)
{
    int result = 0; //_isDcdInverted ? (isHigh ? LOW : HIGH) : (isHigh ? HIGH : LOW);
    switch (Modem_isDcdInverted)
    {
    case 0: result = (int)(!isHigh); break;
    case 1: result = (int)(isHigh); break;
    case 2: result = LOW;
    }
   
    return result;
}

void Modem_Disconnect()
{
    char temp[15];
    Modem_isCommandMode = true;
    Modem_isConnected = false;
    Modem_isRinging = false;    

    // TODO - According to http://totse2.net/totse/en/technology/telecommunications/trm.html
    //		  The BBS should detect <CR><LF>NO CARRIER<CR><LF> as a dropped carrier sequences.
    //		  DMBBS does not honor this and so I haven't sucessfully tested it, thus it's commented out.

    // LB: Added back in for user feedback

    delay(500);
    sprintf_P(temp, PSTR("\n\rNO CARRIER\n\r"));
    DisplayBothP(temp);
    //DisplayBoth(F("\n\rNO CARRIER\n\r"));
    //delay(100);

    if (wifly.available() == -1)
        wifly.close();

    digitalWriteFast(C64_DTR, Modem_ToggleCarrier(false));
}

// Validate and handle AT sequence  (A/ was handled already)
void Modem_ProcessCommandBuffer()
{
    boolean petscii_mode_guess = false;
    byte errors = 0;
    boolean dialed_out = 0;
    // Phonebook dial
    char numString[2];
    char address[ADDR_HOST_SIZE];
    int phoneBookNumber;
    
    // Simple PETSCII/ASCII detection
   
    if (((Modem_CommandBuffer[0] == 'A') && (Modem_CommandBuffer[1] == 'T')))
    {
        petscii_mode_guess = true;
    }
    else if (((Modem_CommandBuffer[0] == 'a') && (Modem_CommandBuffer[1] == 't')))
    {
        petscii_mode_guess = false;
    }
    else
    {
        return;  // Not an AT command, ignore silently
    }

    // Only write to EEPROM if changed
    if (petscii_mode_guess != petscii_mode)
    {
        SetPETSCIIMode(petscii_mode_guess);
    }

    // Used for a/ and also for setting SSID, PASS, KEY as they require upper/lower
    strcpy(Modem_LastCommandBuffer, Modem_CommandBuffer);

    // Force uppercase for consistency 

    for (int i = 0; i < strlen(Modem_CommandBuffer); i++)
    {
        Modem_CommandBuffer[i] = toupper(Modem_CommandBuffer[i]);
    }

    Display(Modem_CommandBuffer);
    
    // TODO: Handle concatenated command strings.  For now, process a aingle command.

    /*if (strcmp(Modem_CommandBuffer, ("ATZ")) == 0)
    {
        Modem_LoadDefaults();
        Modem_PrintOK();
    }
    else if (strcmp(Modem_CommandBuffer, ("ATI")) == 0)
    {
        ShowInfo(false);
        Modem_PrintOK();
    }
    else if (strcmp(Modem_CommandBuffer, ("AT&F")) == 0)
    {
        Modem_LoadDefaults();
        Modem_PrintOK();
    }
    else if (strcmp(Modem_CommandBuffer, ("ATA")) == 0)
    {
        Modem_Answer();    
    }
    else*/ if (strcmp(Modem_CommandBuffer, ("ATD")) == 0 || strcmp(Modem_CommandBuffer, ("ATO")) == 0)
    {
        if (Modem_isConnected)
        {
            Modem_isCommandMode = false;
        }
        else
        {
            Modem_PrintERROR();
        }
    }/*
    else if (strncmp(Modem_CommandBuffer, ("ATD#"), 4) == 0)
    {
        // Phonebook dial
        char numString[2];
        numString[0] = Modem_CommandBuffer[4];
        numString[1] = '\0';
        char address[ADDR_HOST_SIZE];
        
        int phoneBookNumber = atoi(numString);
        if (phoneBookNumber >= 1 && phoneBookNumber <= ADDR_HOST_ENTRIES)
        {
            strncpy(address,readEEPROMPhoneBook(ADDR_HOSTS + ((phoneBookNumber-1) * ADDR_HOST_SIZE)).c_str(),ADDR_HOST_SIZE);
            Modem_Dialout(address);
        }
        else
            Modem_PrintERROR();
    }*/
    /*else if (   // This needs to be revisited
        strncmp(Modem_CommandBuffer, ("ATDT "), 5) == 0 ||
        strncmp(Modem_CommandBuffer, ("ATDP "), 5) == 0 ||
        strncmp(Modem_CommandBuffer, ("ATD "), 4) == 0
        )
    {
        Modem_Dialout(strstr(Modem_CommandBuffer, " ") + 1);
        Modem_ResetCommandBuffer();  // This avoids port# string fragments on subsequent calls
    }
    else if (strncmp(Modem_CommandBuffer, ("ATDT"), 4) == 0)
    {
        Modem_Dialout(strstr(Modem_CommandBuffer, "ATDT") + 4);
        Modem_ResetCommandBuffer();  // This avoids port# string fragments on subsequent calls
    }*/
    /*else if ((strcmp(Modem_CommandBuffer, ("ATH0")) == 0 || strcmp(Modem_CommandBuffer, ("ATH")) == 0))
    {
        Modem_Disconnect();
    }*/
    /*else if (strcmp(Modem_CommandBuffer, ("AT&RAW")) == 0)
    {
        RawTerminalMode();
    }  */ 
    else if (strncmp(Modem_CommandBuffer, ("AT&SSID="), 8) == 0)
    {
        if (petscii_mode)
            wifly.setSSID(petscii::ToASCII(Modem_LastCommandBuffer + 8).c_str());
        else 
            wifly.setSSID(Modem_LastCommandBuffer + 8);
        
        wifly.leave();
        if(wifly.join(20000))    // 20 second timeout
            Modem_PrintOK();
        else
            Modem_PrintERROR();
    }
    else if (strncmp(Modem_CommandBuffer, ("AT&PASS="), 8) == 0)
    {
        if (petscii_mode)
            wifly.setPassphrase(petscii::ToASCII(Modem_LastCommandBuffer + 8).c_str());
        else 
            wifly.setPassphrase(Modem_LastCommandBuffer + 8);
        
        Modem_PrintOK();
    }
    else if (strncmp(Modem_CommandBuffer, ("AT&KEY="), 7) == 0)
    {
        if (petscii_mode)
            wifly.setKey(petscii::ToASCII(Modem_LastCommandBuffer + 7).c_str());
        else 
            wifly.setKey(Modem_LastCommandBuffer + 7);
        
        Modem_PrintOK();
    }
    // AT&PB1=bbs.jammingsignal.com:23
    else if (strncmp(Modem_CommandBuffer, ("AT&PBAUTO="), 10) == 0)
    {
        char numString[2];
        numString[0] = Modem_CommandBuffer[10];
        numString[1] = '\0';
        
        int phoneBookNumber = atoi(numString);
        if (phoneBookNumber >= 0 && phoneBookNumber <= ADDR_HOST_ENTRIES)
        {
            updateEEPROMByte(ADDR_HOST_AUTO, phoneBookNumber);
                        
            Modem_PrintOK();
        }
        else
            Modem_PrintERROR();
    }    
    else if (strncmp(Modem_CommandBuffer, ("AT&PORT="), 8) == 0)
    {
        char numString[6];
        
        int localport = atoi(&Modem_CommandBuffer[8]);
        if (localport >= 1 && localport <= 65535)
        {
              setLocalPort(localport);
              Modem_PrintOK();
        }
        else
            Modem_PrintERROR();
    }    
    else if (strstr(Modem_CommandBuffer, ("AT&PB?")) != NULL)
    {
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
        Modem_PrintOK();
    }
    else if (strstr(Modem_CommandBuffer, ("AT&PBCLEAR")) != NULL)
    {
        for (int i = 0; i < ADDR_HOST_ENTRIES; i++)
        {
            updateEEPROMPhoneBook(ADDR_HOSTS + (i * ADDR_HOST_SIZE), "\0");
        }
        updateEEPROMByte(ADDR_HOST_AUTO, 0);
        Modem_PrintOK();
    }
    else if (strncmp(Modem_CommandBuffer, ("AT&PB"), 5) == 0)
    {
        char numString[2];
        numString[0] = Modem_CommandBuffer[5];
        numString[1] = '\0';
        
        int phoneBookNumber = atoi(numString);
        if (phoneBookNumber >= 1 && phoneBookNumber <= ADDR_HOST_ENTRIES)
        {
            updateEEPROMPhoneBook(ADDR_HOSTS + ((phoneBookNumber-1) * ADDR_HOST_SIZE), Modem_CommandBuffer + 7);
            Modem_PrintOK();
        }
        else
            Modem_PrintERROR();
            
        //Display(readEEPROMPhoneBook(ADDR_HOSTS + ((phoneBookNumber-1) * ADDR_HOST_SIZE)));
        //delay(2000);
    }

    else if (strncmp(Modem_CommandBuffer, ("AT"), 2) == 0)
    {
        for(int i=2; i < strlen(Modem_CommandBuffer) && i < COMMAND_BUFFER_SIZE-3;)
        {
            switch(Modem_CommandBuffer[i++]) 
            {
                case 'Z':
                Modem_LoadDefaults();
                break;
                
                case 'I':
                ShowInfo(false);
                break;

                case 'A':
                Modem_Answer();               
                break;

                case 'E':
                switch(Modem_CommandBuffer[i++])
                {
                    case '0':
                    Modem_EchoOn = false;
                    break;

                    case '1':
                    Modem_EchoOn = true;
                    break;

                    case '?':
                    C64Serial.print(Modem_EchoOn);
                    //C64PrintIntln(EEPROM.read(ADDR_MODEM_ECHO));
                    break;

                    default:
                    errors++;
                }
                break;
                
                case 'H':
                if (Modem_CommandBuffer[i+1] == '0')
                  Modem_CommandBuffer[i++]; 
                Modem_Disconnect();
                break;

                case 'Q':
                switch(Modem_CommandBuffer[i++])
                {
                    case '0':
                    Modem_QuietMode = false;
                    break;

                    case '1':
                    Modem_QuietMode = true;
                    break;

                    case '?':
                    C64Serial.print(Modem_QuietMode);
                    break;

                    default:
                    errors++;
                }
                break;

                case 'S':
                switch(Modem_CommandBuffer[i++])
                {
                    case '0':
                    switch(Modem_CommandBuffer[i++])
                    {
                        case '=':
                        switch(Modem_CommandBuffer[i++])
                        {
                            case '0':
                            Modem_S0_AutoAnswer = 0;
                            break;
        
                            case '1':
                            Modem_S0_AutoAnswer = 1;
                            break;

                            case '?':
                            C64Serial.print(Modem_S0_AutoAnswer);
                            break;

                            default:
                            errors++;
                            }
                        break;    
                    }
                    break;

                }
                break;

                
                case 'V':
                switch(Modem_CommandBuffer[i++])
                {
                    case '0':
                    Modem_VerboseResponses = false;
                    break;

                    case '1':
                    Modem_VerboseResponses = true;
                    break;

                    case '?':
                    C64Serial.print(Modem_VerboseResponses);
                    break;

                    default:
                    errors++;
                }
                break;
                
                case 'X':
                switch(Modem_CommandBuffer[i++])
                {
                    case '0':
                    // TODO
                    break;

                    case '1':
                    // TODO
                    break;

                }
                break;
                
                case '&':
                switch(Modem_CommandBuffer[i++])
                {
                    case 'F':
                    Modem_LoadDefaults();
                    break;
                    
                    case 'K':
                    switch(Modem_CommandBuffer[i++])
                    {
                        case '0':
                        Modem_flowControl = false;
                        break;
    
                        case '1':
                        Modem_flowControl = true;
                        break;
                        
                        case '?':
                        C64Serial.print(Modem_flowControl);
                        break;

                        default:
                        errors++;
                    }
                    break;

                    case 'R':
                    RawTerminalMode();
                    break;
                    
                    case 'W':
                    updateEEPROMByte(ADDR_MODEM_ECHO,Modem_EchoOn);
                    updateEEPROMByte(ADDR_MODEM_FLOW,Modem_flowControl);
                    updateEEPROMByte(ADDR_MODEM_VERBOSE, Modem_VerboseResponses);
                    updateEEPROMByte(ADDR_MODEM_QUIET, Modem_QuietMode);
                    updateEEPROMByte(ADDR_MODEM_S0_AUTOANS, Modem_S0_AutoAnswer);
                    updateEEPROMByte(ADDR_MODEM_S2_ESCAPE, Modem_S2_EscapeCharacter);
                    updateEEPROMByte(ADDR_MODEM_DCD_INVERTED, Modem_isDcdInverted);
                    
                    /*if (wifly.save())
                        Modem_PrintOK();
                    else
                        Modem_PrintERROR();*/
                    if (!(wifly.save()))
                        errors++;                        
                    break;
                }
                break;


                // Dialing should come last..
                // TODO:  Need to allow for spaces after D, DT, DP.  Currently fails.
                case 'D':
                switch(Modem_CommandBuffer[i])
                {
                    case 'T':
                    case 'P':
                    removeSpaces(&Modem_CommandBuffer[i+1]);
                    Modem_Dialout(&Modem_CommandBuffer[i+1]);
                    dialed_out = 1;
                    i = COMMAND_BUFFER_SIZE-3;    // Make sure we don't try to process any more...
                    break;

                    case '#':
                    // Phonebook dial
                    //char numString[2];
                    numString[0] = Modem_CommandBuffer[i+1];
                    numString[1] = '\0';
                    //char address[ADDR_HOST_SIZE];
        
                    //int phoneBookNumber = atoi(numString);
                    phoneBookNumber = atoi(numString);
                    if (phoneBookNumber >= 1 && phoneBookNumber <= ADDR_HOST_ENTRIES)
                    {
                        strncpy(address,readEEPROMPhoneBook(ADDR_HOSTS + ((phoneBookNumber-1) * ADDR_HOST_SIZE)).c_str(),ADDR_HOST_SIZE);
                        removeSpaces(address);
                        Modem_Dialout(address);
                        dialed_out = 1;                        
                    }
                    else
                        errors++;
                    break;

                    default:
                    removeSpaces(&Modem_CommandBuffer[i]);
                    Modem_Dialout(&Modem_CommandBuffer[i]);
                    dialed_out = 1;
                    i = COMMAND_BUFFER_SIZE-3;    // Make sure we don't try to process any more...
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
    
    /*
    else if (strncmp(Modem_CommandBuffer, ("AT"), 2) == 0)
    {
        if (strstr(Modem_CommandBuffer, ("E0")) != NULL)
        {
            Modem_EchoOn = false;
        }

        if (strstr(Modem_CommandBuffer, ("E1")) != NULL)
        {
            Modem_EchoOn = true;
        }

        if (strstr(Modem_CommandBuffer, ("Q0")) != NULL)
        {
            Modem_VerboseResponses = false;
            Modem_QuietMode = false;
        }

        if (strstr(Modem_CommandBuffer, ("Q1")) != NULL)
        {
            Modem_QuietMode = true;
        }

        if (strstr(Modem_CommandBuffer, ("V0")) != NULL)
        {
            Modem_VerboseResponses = false;
        }

        if (strstr(Modem_CommandBuffer, ("V1")) != NULL)
        {
            Modem_VerboseResponses = true;
        }

        if (strstr(Modem_CommandBuffer, ("X0")) != NULL)
        {
            // TODO
        }
        if (strstr(Modem_CommandBuffer, ("X1")) != NULL)
        {
            // TODO
        }
        if (strstr(Modem_CommandBuffer, ("&K0")) != NULL)
        {
            Modem_flowControl = false;
        }
        if (strstr(Modem_CommandBuffer, ("&K1")) != NULL)
        {
            Modem_flowControl = true;
        }
        if (strstr(Modem_CommandBuffer, ("&W")) != NULL)
        {
            if (wifly.save())
                Modem_PrintOK();
            else
                Modem_PrintERROR();
        }
*/
        /*char *currentS;
        char temp[100];

        int offset = 0;
        if ((currentS = strstr(Modem_CommandBuffer, ("S0="))) != NULL)
        {
            offset = 3;
            while (currentS[offset] != '\0' && isDigit(currentS[offset]))
            {
                offset++;
            }

            memset(temp, 0, 100);
            strncpy(temp, currentS + 3, offset - 3);
            Modem_S0_AutoAnswer = atoi(temp);
        }*/

        if (!dialed_out) {    // Should add error checking to calls to Modem_Dialout(), Connect() etc.
            if (errors)   
                Modem_PrintERROR();
            else
                Modem_PrintOK();
        }
    }
    else
    {
        Modem_PrintERROR();
    }

    //strcpy(Modem_LastCommandBuffer, Modem_CommandBuffer);   // can't be here, already switched to uppercase
    Modem_ResetCommandBuffer();
}

void Modem_Ring()
{
    Modem_isRinging = true;

    if (Modem_S0_AutoAnswer != 0)
    {
        Modem_Answer();
    }
    else
    {
        Modem_PrintResponse("2", F("RING"));

        digitalWriteFast(C64_DCD, Modem_ToggleCarrier(true));

        digitalWriteFast(C64_RI, HIGH);
        delay(250);
        digitalWriteFast(C64_RI, LOW);
    }
}

void Modem_Connected()
{
    char temp[20];
    sprintf_P(temp, PSTR("CONNECT %d"), BAUD_RATE);
    
    DisplayBothP(temp);

    digitalWriteFast(C64_DTR, Modem_ToggleCarrier(true));

    Modem_isConnected = true;
    Modem_isCommandMode = false;
    Modem_isRinging = false;

/*
    if (_verboseResponses)
    {
        _serial->print(F("CONNECT "));
    }

    if (_baudRate == 38400)
    {
        Modem_PrintResponse("28", F("38400"));
    }
    else if (_baudRate == 19200)
    {
        printResponse("15", F("19200"));
    }
    else if (_baudRate == 14400)
    {
        printResponse("13", F("14400"));
    }
    else if (_baudRate == 9600)
    {
        printResponse("12", F("9600"));
    }
    else if (_baudRate == 4800)
    {
        printResponse("11", F("4800"));
    }
    else if (_baudRate == 2400)
    {
        printResponse("10", F("2400"));
    }
    else if (_baudRate == 1200)
    {
        printResponse("5", F("1200"));
    }
    else
    {
        if (!_verboseResponses)
            _serial->println('1');
        else
        {
            _serial->println(_baudRate);
        }
    }
    */
}

void Modem_ProcessData()
{
    //digitalWrite(RTS, LOW);
    //if(digitalRead(DCE_CTS) == HIGH) Serial.write("::DCE_CTS is high::");
    //if(digitalRead(DCE_CTS) == LOW) Serial.write("::DCE_CTS is low::");

    while (C64Serial.available() >0)
    {
        // Command Mode -----------------------------------------------------------------------
        if (Modem_isCommandMode)
        {
            if (Modem_flowControl) 
                digitalWriteFast(C64_CTS, LOW);
            //char inbound = toupper(_serial->read());
            char inbound = C64Serial.read();

            if (Modem_EchoOn)
            {
                if (!Modem_flowControl) 
                delay(100 / (BAUD_RATE / 2400));     // Slow down command mode to prevent garbage if flow control
                // is disabled.  Doesn't work at 9600 but flow control should be on at 9600 baud anyways.  TODO:  Fix
                C64Serial.write(inbound);
            }

            if (IsBackSpace(inbound))
            {
                if (strlen(Modem_CommandBuffer) > 0)
                {
                    Modem_CommandBuffer[strlen(Modem_CommandBuffer) - 1] = '\0';
                }
            }
            else if (inbound != '\r' && inbound != '\n' && inbound != Modem_S2_EscapeCharacter)
            {
                if (strlen(Modem_CommandBuffer) >= COMMAND_BUFFER_SIZE) {
                  //Display (F("CMD Buf Overflow"));
                  Modem_PrintERROR();
                  Modem_ResetCommandBuffer();
                }
                else {
                    Modem_CommandBuffer[strlen(Modem_CommandBuffer)] = inbound;
    
                    if (toupper(Modem_CommandBuffer[0]) == 'A' && (Modem_CommandBuffer[1] == '/'))
                    {
                        strcpy(Modem_CommandBuffer, Modem_LastCommandBuffer);
                        if (Modem_flowControl) 
                            digitalWriteFast(C64_CTS, HIGH);
                        Modem_ProcessCommandBuffer();
                        Modem_ResetCommandBuffer();  // To prevent A matching with A/ again
                    }
                }
            }
            else if (toupper(Modem_CommandBuffer[0]) == 'A' && toupper(Modem_CommandBuffer[1]) == 'T')
            {
                if (Modem_flowControl) 
                    digitalWriteFast(C64_CTS, HIGH);
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
                char inbound = C64Serial.read();

                if (inbound == Modem_S2_EscapeCharacter)
                {
                    Modem_EscapeCount++;
                }
                else
                {
                    Modem_EscapeCount = 0;
                    // TODO, guard time!
                }

                if (Modem_EscapeCount == 3)
                {
                    Modem_EscapeCount = 0;
                    Modem_isCommandMode = true;   // TODO, guard time!

                    Modem_PrintOK();
                }

                int result = wifly.write(inbound);
            }
        }
    }
    //digitalWrite(DCE_RTS, LOW);
    if (Modem_flowControl) 
        digitalWriteFast(C64_CTS, HIGH);
}

void Modem_Answer()
{
    if (!Modem_isRinging)    // If not actually ringing...
    {
        Modem_Disconnect();  // This prints "NO CARRIER"
        return;
    }

    Modem_Connected();
}

void Modem_Dialout(char* host)
{
    char* index;
    uint16_t port = 23;
    String hostname = String(host);

        if (strlen(host) == 0)
        {
            Modem_PrintERROR();
            return;
        }
    
        if ((index = strstr(host, ":")) != NULL)
        {
            index[0] = '\0';
            hostname = String(host);
            port = atoi(index + 1);
        }
    /*if (hostname == F("5551212"))
    {
        QuantumLinkReloaded();
        return;
    }*/

    lastHost = hostname;
    lastPort = port;

    Connect(hostname, port, false);
}

// Main processing loop for the virtual modem.  Needs refactoring!
void Modem_Loop()
{
    boolean wiflyIsConnected = wifly.isConnected();

    if (wiflyIsConnected) {
        // Check for new remote connection
        if (!Modem_isConnected && !Modem_isRinging)
        {
            wifly.println(F("CONNECTING TO SYSTEM."));
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
                wifly.println(F("error: remote modem is in command mode."));
            }
            else
            {
                while (wifly.available() > 0)
                {
                    C64Serial.write(wifly.read());
                    // TODO:  Review..  Flow control?
                }
            }
        }
    }
    else
    {
        // Check for a dropped remote connection while ringing
        if (Modem_isRinging)
        {
            Modem_Disconnect();
            return;
        }
    
        // Check for a dropped remote connection while connected
        if (Modem_isConnected)
        {
            Modem_Disconnect();
            return;
        }
    }

    // Flow control, revisit
    //else if (!modem.getIsConnected() && modem.getIsCommandMode())
    //{
        //digitalWrite(DCE_RTS, LOW);
    //}
    //else if(digitalRead(DTE_CTS) == HIGH)
    //{
    //	digitalWrite(DTE_RTS, LOW);
    //}

    if (autoConnectHost >=1 && autoConnectHost <= ADDR_HOST_ENTRIES && !autoConnectedAtBootAlready)
    {
        // Phonebook dial
        autoConnectedAtBootAlready = 1;
        char address[ADDR_HOST_SIZE];
        
        strncpy(address,readEEPROMPhoneBook(ADDR_HOSTS + ((autoConnectHost-1) * ADDR_HOST_SIZE)).c_str(),ADDR_HOST_SIZE);
        Modem_Dialout(address);
    }

    // Handle all other data arriving on the serial port of the virtual modem.
    Modem_ProcessData();

    //digitalWrite(DCE_RTS, HIGH);
}

#endif // HAYES

// -------------------------------------------------------------------------------------------------------------
// QuantumLink Reloaded! Support
/*
void QuantumLinkReloaded()
{

   // Open("qlink.lyonlabs.org", 5190);

}*/
/*
hostname = ;
port = 5190;

//Prepare to change the Arduino baud rate.
Serial.flush();
delay(2);
Serial.end();

//Change the arduino's baud rate.
Serial.begin(115200);
*/

/*
void ConfigureBaud()
{
  if (BAUD_RATE == 2400)
  {
      setBaud(9600,true);
  }
  else
  {
      setBaud(2400,true);
  }
}*/

/*
void ConfigureBaud()
{
    while (true)
    {
        C64Println();
        C64Print(F("Baud Rate Menu (Current: "));
        C64Serial.print(BAUD_RATE);
        C64Println(F(")"));
        C64Println();
        C64Println(F("1. 1200"));
        C64Println(F("2. 2400"));
        C64Println(F("3. 9600"));
        C64Println(F("4. Return to Main Menu"));

        C64Println();
        C64Print(F("Select: "));

        int option = ReadByte(C64Serial);
        C64Serial.println((char)option);

        switch (option)
        {
        case '1': setBaud(1200, true);
            break;
        case '2': setBaud(2400, true);
            break;
        case '3': setBaud(9600, true);
            break;
        case '4': return;

        case '\n':
        case '\r':
        case ' ':
            continue;

        default: C64Println(F("Unknown option, try again"));
            continue;
        }

        EEPROM.write(ADDR_AUTOSTART, autostart_mode);  // Save
        C64Println(F("Saved"));
    }
}*/

/*
void setBaud(int newBaudRate, boolean pause, boolean setup) {
    char temp[20];
    BAUD_RATE = newBaudRate;
    if (!setup) {
        sprintf(temp, "New baud: %d", BAUD_RATE);
    C64Println(temp);
    C64Serial.flush();
    delay(2);
    C64Serial.end();
    }
    C64Serial.begin(BAUD_RATE);
    C64Serial.setTimeout(1000);

    if(BAUD_RATE == 1200)
        setBaudWiFi(2400);
    else
        setBaudWiFi(BAUD_RATE);

    if (pause)
        delay(5000);  // Delay 5 seconds so user has time to switch to help prevent junk...

    byte a = newBaudRate/256;
    byte b = newBaudRate % 256;

    // Let's not waste writes..
    if (EEPROM.read(ADDR_BAUD_LO) != a)
        EEPROM.write(ADDR_BAUD_LO, a);
    if (EEPROM.read(ADDR_BAUD_HI) != b)
        EEPROM.write(ADDR_BAUD_HI, b);
}*/

void setBaudWiFi(int newBaudRate) {
  WiFly_BAUD_RATE = newBaudRate;
  WifiSerial.flush();
  delay(2);
  wifly.setBaud(newBaudRate);     // Uses set uart instant so no save and reboot needed
  delay(1000);
  WifiSerial.end();
  WifiSerial.begin(newBaudRate);
}

int freeRam () 
{
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

// EOF!
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
    long rate = pulseIn(recpin, LOW); // measure zero bit width from character 'U'

    if (rate < 12)
        baud = 115200;
    else if (rate < 20)
        baud = 57600;
    else if (rate < 29)
        baud = 38400;
    else if (rate < 40)
        baud = 28800;
    else if (rate < 60)
        baud = 19200;
    else if (rate < 80)
        baud = 14400;
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

//void updateEEPROMPhoneBook(byte address, String host, int port)
void updateEEPROMPhoneBook(int address, String host)
{
    int i=0;
    for (; i < 38; i++)
    {
        EEPROM.write(address + i, host.c_str()[i]);
    }
    
    /*byte a = BAUD_RATE / 256;
    byte b = BAUD_RATE % 256;

    updateEEPROMByte(address+i++,a);
    updateEEPROMByte(address+i,b);*/
}

String readEEPROMPhoneBook(int address)
{
    char host[38];
    int i=0;
    for (; i < 38; i++)
    {
        host[i] = EEPROM.read(address + i);
    }
    return host;
    
    //lastHost = host;

    //lastPort = (EEPROM.read(address+i++) * 256 + EEPROM.read(address+i));
}


void processC64Inbound()
{
    char C64input = C64Serial.read();

    if ((millis() - ESCAPE_GUARD_TIME) > escapeTimer)
        {

        if (C64input == '+' && lastC64input != '+')
        {
            escapeCount = 1;
            lastC64input = C64input;
        }
        else if (C64input == '+' && lastC64input == '+')
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
        Display("Escape!");
        escapeReceived = true;
        escapeCount = 0;
        escapeTimer = 0;
    }
      
    wifly.write(C64input);
    //wifly.write(C64Serial.read());
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
    if(wifly.getPort() != localport)
    {
        wifly.setPort(localport);
        wifly.save();
        //wifly.reboot();
        //delay(5000);
        
        if (WiFly_BAUD_RATE != 2400) {
            C64Println(F("Reboot Microview & WiFi to set new port."));
            return true;
        }
        else {
            C64Println(F("Rebooting WiFi..."));
            wifly.reboot();
            delay(5000);
            return false;
        }
    }
}
