/*
    Commodore 64 - MicroView - Wi-Fi Cart
    Author: Leif Bloomquist
    With assistance and code from Greg Alekel, Payton Byrd, Craig Bruce
*/

// Defining HAYES enables Hayes commands and disables the 1) and 2) menu options for telnet and incoming connections.
// This is required to ensure the compiled code is <= 30,720 bytes 
#define HAYES
//#define ENABLE_C64_DCD
#include <MicroView.h>
#include <elapsedMillis.h>
#include <SoftwareSerial.h>
#include <WiFlyHQ.h>
#include <EEPROM.h>
#include <digitalWriteFast.h>
#include "petscii.h"

;  // Keep this here to pacify the Arduino pre-processor

#define VERSION "0.05b1"

// Configuration 0v3: Wifi Hardware, C64 Software.

#define BAUD_RATE 2400
#define WiFly_BAUD_RATE 2400
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

// Misc Values
#define TIMEDOUT  -1
int baudMismatch = (BAUD_RATE != WiFly_BAUD_RATE ? 1 : 0);
boolean Modem_flowControlEnabled = false;   // for &K setting.  Not currently stored in EEPROM

// EEPROM Addresses
#define ADDR_PETSCII       0
#define ADDR_AUTOSTART     1
#define ADDR_MODEM_ECHO    10

// PETSCII state
boolean petscii_mode = EEPROM.read(ADDR_PETSCII);

// Autoconnect Options
#ifdef HAYES
#define AUTO_NONE     0
#define AUTO_HAYES    1
#define AUTO_CSERVER  2
//#define AUTO_QLINK    3
#define AUTO_CUSTOM   100   // TODO
#else
#define AUTO_NONE     0
#define AUTO_CSERVER  1
//#define AUTO_QLINK    3
#define AUTO_CUSTOM   100   // TODO

#endif

int autostart_mode = EEPROM.read(ADDR_AUTOSTART);

// ----------------------------------------------------------
// Arduino Setup Function

int main(void) {
    init();
    {
        uView.begin(); // begin of MicroView
        uView.setFontType(0);
        uView.clear(ALL); // erase hardware memory inside the OLED controller
    
        C64Serial.begin(BAUD_RATE);
        WifiSerial.begin(WiFly_BAUD_RATE);
    
        C64Serial.setTimeout(1000);

        // Always setup pins for flow control
        pinModeFast(C64_RTS, INPUT);
        pinModeFast(C64_CTS, OUTPUT);
    
        pinModeFast(WIFI_RTS, INPUT);
        pinModeFast(WIFI_CTS, OUTPUT);
    
        // Force CTS to defaults on both sides to start, so data can get through
        digitalWriteFast(WIFI_CTS, LOW);
        digitalWriteFast(C64_CTS, HIGH);  // C64 is inverted
    
        DisplayBoth(F("Wi-Fi Init..."));
    
        boolean ok = wifly.begin(&WifiSerial, &C64Serial);
    
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
    
        wifly.close();

#ifdef ENABLE_C64_DCD
        digitalWriteFast(C64_DCD, true);    // true = no carrier
#endif    
        HandleAutoStart();
    
        C64Println();
#ifdef HAYES
        C64Println(F("Commodore Wi-Fi Modem Hayes"));
#else
        C64Println(F("Commodore Wi-Fi Modem"));
#endif // HAYES
        ShowInfo(true);
    }
    while (1)
    {
        Display(F("READY."));
    
        C64Println();
        ShowPETSCIIMode();
        C64Println();
#ifdef HAYES
        C64Println(F("1. Hayes Emulation Mode"));
        C64Println(F("2. Configuration"));
#else
        C64Println(F("1. Telnet to host or BBS"));
        C64Println(F("2. Wait for incoming connection"));
        C64Println(F("3. Configuration"));
#endif // HAYES
        C64Println();
        C64Print(F("Select: "));
    
        int option = ReadByte(C64Serial);
        C64Serial.println((char)option);
    
        switch (option)
        {
#ifdef HAYES
        case '1':
            HayesEmulationMode();
            break;
    
        case '2':
                Configuration();
            break;
#else
        case '1':
            DoTelnet();
            break;
    
        case '2':
            Incoming();
            break;
       
        case '3':
                Configuration();
            break;

#endif // HAYES   
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
}

void Configuration()
{
    while (true)
    {
        C64Println();
        C64Println(F("Configuration Menu"));
        C64Println();
        C64Println(F("1. Display Current Configuration"));
        C64Println(F("2. Change SSID"));
        C64Println(F("3. Autostart Options"));
        C64Println(F("4. Direct Terminal Mode (Debug)"));
        C64Println(F("5. Return to Main Menu"));
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

        case '3': ConfigureAutoStart();
            break;

        case '4': RawTerminalMode();
            return;

        case '5': return;

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
        boolean ok = !wifly.setSSID(input.c_str());   // Note inverted, not sure why this has to be

            if (ok)
            {
                C64Println(F("SSID Successfully changed"));
                wifly.save();
                wifly.reboot();
                return;
            }
            else
            {
                C64Println(F("Error Setting SSID"));
                continue;
            }
    }
}

// ----------------------------------------------------------
// MicroView Display helpers

void Display(String message)
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

inline void C64Println()
{
    C64Serial.println();
}

void ShowPETSCIIMode()
{
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

    C64Println(F(" (Del to switch)"));
}

void SetPETSCIIMode(boolean mode)
{
    petscii_mode = mode;
    EEPROM.write(ADDR_PETSCII, petscii_mode);
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

    wifly.getMAC(mac, 20);
    wifly.getIP(ip, 20);
    wifly.getSSID(ssid, 20);

    C64Println();
    C64Print(F("MAC Address: "));    C64Println(mac);
    C64Print(F("IP Address:  "));    C64Println(ip);
    C64Print(F("Wi-Fi SSID:  "));    C64Println(ssid);
    C64Print(F("Firmware:    "));    C64Println(VERSION);

    if (powerup)
    {
        char temp[50];

        sprintf(temp, "Firmware\n\n%s", VERSION);
        Display(temp);
        delay(1000);

        sprintf(temp, "Baud Rate\n\n%d", BAUD_RATE);
        Display(temp);
        delay(1000);

        sprintf(temp, "IP Address \n%s", ip);
        Display(temp);
        delay(1000);

        sprintf(temp, "SSID\n\n%s", ssid);
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
        wifly.setPort(localport);
        wifly.save();
        wifly.reboot();
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
    sprintf(temp, "\nConnecting to %s", host.c_str());
    DisplayBoth(temp);

    wifly.setIpProtocol(WIFLY_PROTOCOL_TCP);

    /* Do a DNS lookup to get the IP address.  Lookup has a 5 second timeout. */
    char ip[16];
    if (!wifly.getHostByName(host.c_str(), ip, sizeof(ip))) {
       DisplayBoth(F("Could not resolve DNS.  Connect Failed!")); 
       return;
    }

    boolean ok = wifly.open(ip, port);

    if (ok)
    {
        sprintf(temp, "Connected to %s", host.c_str());
        DisplayBoth(temp);
#ifdef ENABLE_C64_DCD
        digitalWriteFast(C64_DCD, false);
#endif
    }
    else
    {
        DisplayBoth(F("Connect Failed!"));
        return;
    }

    if (!raw)
    {
        CheckTelnet();
    }
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
                while (digitalReadFast(C64_RTS) == LOW);   // If not...  C64 RTS and CTS are inverted.
                C64Serial.write(buffer[i]);
            }

            // Only used for displaying max buffer size on Microview for testing
            if (max_buffer_size_reached < buffer_index)
                max_buffer_size_reached = buffer_index;
            char message[20];
            sprintf(message, "Buf: %d", max_buffer_size_reached);
            Display(message);
            
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
            wifly.write(C64Serial.read());
        }

        // Alternate check for open/closed state
        if (!wifly.isConnected())
        {
            break;
        }
    }

    wifly.close();
    DisplayBoth(F("Connection closed"));
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
            if (Modem_flowControlEnabled || BAUD_RATE >= MIN_FLOW_CONTROL_RATE || baudMismatch)
            {
                sprintf(temp, "Wifi:%ld\n\nC64: %ld\n\nRTS: %ld", rnxv_chars, c64_chars, c64_rts_count);
            }
            else
            {
                sprintf(temp, "Wifi:%ld\n\nC64: %ld", rnxv_chars, c64_chars);
            }
            Display(temp);
        }
    }
}

inline void DoFlowControl()
{
   
    if (Modem_flowControlEnabled || BAUD_RATE >= MIN_FLOW_CONTROL_RATE || baudMismatch)
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

    // Wait for first character for 5 seconds
    inpint = PeekByte(wifly, 5000);

    if (inpint != IAC)
    {
        return;   // Not a telnet session
    }

    // IAC handling
    SendTelnetParameters();    // Start off with negotiating

    while (true)
    {
        inpint = PeekByte(wifly, 1000);       // peek at next character, timeout after 1 second

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

        EEPROM.write(ADDR_AUTOSTART, autostart_mode);  // Save
        C64Println(F("Saved"));
    }
}

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
            EEPROM.write(ADDR_AUTOSTART, AUTO_NONE);
            autostart_mode = AUTO_NONE;
            return;
    }

    // Wait for user to cancel

    C64Println(F("Press any key to cancel..."));

    int option = PeekByte(C64Serial, 5000);

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

#ifdef HAYES
// ----------------------------------------------------------
// Simple Hayes Emulation
// Portions of this code are adapted from Payton Byrd's Hayesduino - thanks!

boolean Modem_isCommandMode = true;
boolean Modem_isConnected = false;
boolean Modem_isRinging = false;
boolean Modem_EchoOn = EEPROM.read(ADDR_MODEM_ECHO);
boolean Modem_VerboseResponses = true;
boolean Modem_QuietMode = false;
boolean Modem_isDcdInverted = false;

boolean Modem_S0_AutoAnswer = false;
char    Modem_S2_EscapeCharacter = '+';

#define COMMAND_BUFFER_SIZE  81
char Modem_LastCommandBuffer[COMMAND_BUFFER_SIZE];
char Modem_CommandBuffer[COMMAND_BUFFER_SIZE];

int Modem_EscapeCount = 0;

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

    if (Modem_VerboseResponses)
        C64Println(msg);
    else
        C64Println(code);

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
    Modem_SetEcho(true);
    Modem_VerboseResponses = true;
    Modem_QuietMode = false;
    Modem_S0_AutoAnswer = false;
    Modem_S2_EscapeCharacter = '+';
    Modem_isDcdInverted = false;
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
    Modem_isCommandMode = true;
    Modem_isConnected = false;
    Modem_isRinging = false;    

    wifly.close();

    // TODO - According to http://totse2.net/totse/en/technology/telecommunications/trm.html
    //		  The BBS should detect <CR><LF>NO CARRIER<CR><LF> as a dropped carrier sequences.
    //		  DMBBS does not honor this and so I haven't sucessfully tested it, thus it's commented out.

    // LB: Added back in for user feedback

    delay(100);
    DisplayBoth(F("NO CARRIER"));

    digitalWriteFast(C64_DTR, Modem_ToggleCarrier(false));
}

void Modem_SetEcho(boolean on)
{
    Modem_EchoOn = on;
    EEPROM.write(ADDR_MODEM_ECHO, Modem_EchoOn);  // Save
}


// Validate and handle AT sequence  (A/ was handled already)
void Modem_ProcessCommandBuffer()
{
    boolean petscii_mode_guess = false;
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

    strcpy(Modem_LastCommandBuffer, Modem_CommandBuffer);

    // Force uppercase for consistency 

    for (int i = 0; i < strlen(Modem_CommandBuffer); i++)
    {
        Modem_CommandBuffer[i] = toupper(Modem_CommandBuffer[i]);
    }

    Display(Modem_CommandBuffer);

    // TODO: Handle concatenated command strings.  For now, process a aingle command.

    if (strcmp(Modem_CommandBuffer, ("ATZ")) == 0)
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
        if (strcmp(Modem_LastCommandBuffer, ("AT&F")) == 0)
        {
            Modem_LoadDefaults();
            Modem_PrintOK();
        }
        else
        {
            C64Println(F("Send command again to verify")); 
        }
    }
    else if (strcmp(Modem_CommandBuffer, ("ATA")) == 0)
    {
        Modem_Answer();    
    }
    else if (strcmp(Modem_CommandBuffer, ("ATD")) == 0 || strcmp(Modem_CommandBuffer, ("ATO")) == 0)
    {
        if (Modem_isConnected)
        {
            Modem_isCommandMode = false;
        }
        else
        {
            Modem_PrintERROR();
        }
    }
    else if (   // This needs to be revisited
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
    }
    else if ((strcmp(Modem_CommandBuffer, ("ATH0")) == 0 || strcmp(Modem_CommandBuffer, ("ATH")) == 0))
    {
        Modem_Disconnect();
    }
    else if (strncmp(Modem_CommandBuffer, ("AT"), 2) == 0)
    {
        if (strstr(Modem_CommandBuffer, ("E0")) != NULL)
        {
            Modem_SetEcho(false);
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
            Modem_flowControlEnabled = false;
        }
        if (strstr(Modem_CommandBuffer, ("&K1")) != NULL)
        {
            Modem_flowControlEnabled = true;
        }

        char *currentS;
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
        }

        Modem_PrintOK();
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
    sprintf(temp, "CONNECT %d", BAUD_RATE);
    DisplayBoth(temp);

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

    while (C64Serial.available())
    {
        
        // Command Mode -----------------------------------------------------------------------
        if (Modem_isCommandMode)
        {
            if (Modem_flowControlEnabled) 
                digitalWriteFast(C64_CTS, LOW);
            //char inbound = toupper(_serial->read());
            char inbound = C64Serial.read();

            if (Modem_EchoOn)
            {
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
                  Display (F("CMD Buf Overflow"));
                  Modem_PrintERROR();
                  Modem_ResetCommandBuffer();
                }
                else {
                    Modem_CommandBuffer[strlen(Modem_CommandBuffer)] = inbound;
    
                    if (toupper(Modem_CommandBuffer[0]) == 'A' && (Modem_CommandBuffer[1] == '/'))
                    {
                        strcpy(Modem_CommandBuffer, Modem_LastCommandBuffer);
                        if (Modem_flowControlEnabled) 
                            digitalWriteFast(C64_CTS, HIGH);
                        Modem_ProcessCommandBuffer();
                        Modem_ResetCommandBuffer();  // To prevent A matching with A/ again
                    }
                }
            }
            else if (toupper(Modem_CommandBuffer[0]) == 'A' && toupper(Modem_CommandBuffer[1]) == 'T')
            {
                if (Modem_flowControlEnabled) 
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
    if (Modem_flowControlEnabled) 
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

    Connect(hostname, port, false);
}

// Main processing loop for the virtual modem.  Needs refactoring!
void Modem_Loop()
{
    boolean isConnected = wifly.isConnected();
    
    // Check for new remote connection
    if (!Modem_isConnected && !Modem_isRinging && isConnected)
    {
        wifly.println(F("CONNECTING TO SYSTEM."));
        Display(F("INCOMING\nCALL"));
        Modem_Ring();
        return;
    }

    // Check for a dropped remote connection while ringing
    if (Modem_isRinging && !isConnected)
    {
        Modem_Disconnect();
        return;
    }

    // Check for a dropped remote connection while connected
    if (Modem_isConnected && !isConnected)
    {
        Modem_Disconnect();
        return;
    }

    // If connected, handle incoming data	
    if (Modem_isConnected && isConnected)
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
            }
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


// EOF!
