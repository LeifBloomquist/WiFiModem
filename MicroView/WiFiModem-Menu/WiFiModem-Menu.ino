/*
    Commodore 64 - MicroView - Wi - fi Cart
    Simple Menu Sketch
    Leif Bloomquist
*/

#include <MicroView.h>
#include <elapsedMillis.h>
#include <SoftwareSerial.h>
#include <WiFlyHQ.h>
#include <EEPROM.h>
#include <digitalWriteFast.h>
#include "petscii.h"

;  // Keep this here to pacify the Arduino pre-processor

#define VERSION "0.02"


// Configuration 0v3: Wifi Hardware, C64 Software.

#define BAUD_RATE 2400

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

// EEPROM Addresses
#define ADDR_PETSCII       0
#define ADDR_AUTOSTART     1

// PETSCII state
boolean petscii_mode = EEPROM.read(ADDR_PETSCII);

// Autoconnect Options
#define AUTO_NONE     0
#define AUTO_HAYES    1
#define AUTO_CSERVER  2
#define AUTO_QLINK    3
#define AUTO_CUSTOM   100   // TODO

int autostart_mode = EEPROM.read(ADDR_AUTOSTART);

// ----------------------------------------------------------
// Arduino Setup Function

void setup()
{
    uView.begin(); // begin of MicroView
    uView.setFontType(0);
    uView.clear(ALL); // erase hardware memory inside the OLED controller

    C64Serial.begin(BAUD_RATE);
    WifiSerial.begin(BAUD_RATE);

    C64Serial.setTimeout(1000);

    if (BAUD_RATE > 2400)
    {
        pinModeFast(C64_RTS, INPUT);
        pinModeFast(C64_CTS, OUTPUT);

        pinModeFast(WIFI_RTS, INPUT);
        pinModeFast(WIFI_CTS, OUTPUT);

        // Force CTS to defaults on both sides to start, so data can get through
        digitalWriteFast(WIFI_CTS, LOW);
        digitalWriteFast(C64_CTS, HIGH);
    }

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

    wifly.close();

    HandleAutoStart();

    C64Println();
    C64Println(F("Commodore Wi-Fi Modem"));
    ShowInfo(true);
}

// ----------------------------------------------------------
// Arduino Loop Function

void loop()
{
    Display(F("READY."));

    C64Println();
    ShowPETSCIIMode();
    C64Println();
    C64Println(F("1. Telnet to host or BBS"));
    C64Println(F("2. Wait for incoming connection"));
    C64Println(F("3. Hayes Emulation Mode"));
    C64Println(F("4. Configuration"));
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
        HayesEmulationMode();
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
        C64Serial.println(message);
    }
    else
    {
        C64Serial.println("ASCII Mode");
    }
}

void SetPETSCIIMode(boolean mode)
{
    petscii_mode = mode;
    EEPROM.write(ADDR_PETSCII, petscii_mode);
}

// ----------------------------------------------------------
// User Input Handling

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

        if ((key == '\b') && (i > 0)) i -= 2; // Handles back space.

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

    if (powerup)
    {
        C64Print(F("Firmware:    "));    C64Println(VERSION);

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
        C64Print(F("\nPort ("));
        C64Serial.print(lastPort);
        C64Print(F("): "));

        String strport = GetInput();

        if (strport.length() > 0)
        {
            port = strport.toInt();
        }
        else
        {
            port = lastPort;
        }

        lastHost = hostName;
        lastPort = port;

        Connect(hostName, port, false);
    }
    else
    {
        if (lastHost.length() > 0)
        {
            Connect(lastHost, lastPort, false);
        }
        else
        {
            return;
        }
    }
}

void Connect(String host, int port, boolean raw)
{
    char temp[50];
    sprintf(temp, "\nConnecting to %s", host.c_str());
    DisplayBoth(temp);

    wifly.setIpProtocol(WIFLY_PROTOCOL_TCP);

    boolean ok = wifly.open(host.c_str(), port);

    if (ok)
    {
        sprintf(temp, "Connected to %s", host.c_str());
        DisplayBoth(temp);
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
    C64Println(F("*** Terminal Mode ***"));

    while (wifly.available() != -1) // -1 means closed
    {
        while (wifly.available() > 0)
        {
            DoFlowControl();
            C64Serial.write(wifly.read());
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
            if (BAUD_RATE > 2400)
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
    if (BAUD_RATE > 2400)
    {
        // Check that C64 is ready to receive
        while (digitalReadFast(C64_RTS) == LOW)   // If not...
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

// Peek at next byte.  Returns byte (as a int, via Stream::peek()) or -1 if timed out

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
        C64Print(F(")"));
        C64Println();
        C64Println(F("0. Clear Autostart Options"));
        C64Println(F("1. Hayes Emulation Mode"));
        C64Println(F("2. CommodoreServer"));
        C64Println(F("3. QuantumLink Reloaded"));
        C64Println(F("4. Return to Main Menu"));
        C64Println();
        C64Print(F("Select: "));

        int option = ReadByte(C64Serial);
        C64Serial.println((char)option);

        switch (option)
        {
        case '0': autostart_mode = AUTO_NONE;
            break;

        case '1': autostart_mode = AUTO_HAYES;
            break;

        case '2': autostart_mode = AUTO_CSERVER;
            break;

        case '3': autostart_mode = AUTO_QLINK;
            break;

        case '4': return;

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
    }
}

void HandleAutoStart()
{
    // Display chosen action

    switch (autostart_mode)
    {
    case AUTO_NONE:  // No Autostart, continue as normal
        return;

    case AUTO_HAYES: // Hayes Emulation
        C64Println(F("Entering Hayes Emulation Mode"));
        break;

    case AUTO_CSERVER: // Hayes Emulation
        C64Println(F("Connecting to CommodoreServer"));
        break;

    case AUTO_QLINK: // QuantumLink Reloaded
        C64Println(F("Ready to Connect to QuantumLink Reloaded"));
        C64Println(F("TODO: Not Implemented!"));  //  !!!! TODO
        return;

    default: // Invalid - on first startup or if corrupted.  Clear silently and continue to menu.
        EEPROM.write(ADDR_AUTOSTART, AUTO_NONE);
        autostart_mode = AUTO_NONE;
        return;
    }

    // Wait for user to cancel

    C64Println(F("Press any key within 5 seconds to cancel"));
    C64Println();

    int option = PeekByte(C64Serial, 5000);
    ReadByte(C64Serial);    // eat character

    if (option != TIMEDOUT) return;  // Key pressed

    // Take the chosen action

    switch (autostart_mode)
    {
    case AUTO_HAYES: // Hayes Emulation
        C64Println(F("Entering Hayes Emulation Mode"));
        HayesEmulationMode();
        break;

    case AUTO_CSERVER: // CommodoreServer - just connect repeatedly
        while (true)
        {
            Connect("www.commodoreserver.com", 1541, true);
            delay(1000);
        }
        break;

    case AUTO_QLINK: // QuantumLink Reloaded
        // !!!! Not 100% sure what to do here.  Need 1200 baud.
        return;   // !!!!

    default: // Shouldn't ever reach here.  Just continue to menu if so.
        return;
    }
}

// ----------------------------------------------------------
// Simple Hayes Emulation

void HayesEmulationMode()
{
    boolean command_mode = true;

    PrintOK();

    // !!!!


}

inline void PrintOK()
{
    C64Print(F("OK"));
}

inline void PrintERROR()
{
    C64Print(F("ERROR"));
}

// EOF!