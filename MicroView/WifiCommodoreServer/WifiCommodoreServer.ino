/*
     Commodore 64 - MicroView - Wi-fi Cart
     This sketch connects to CommodoreServer for V1541 from CommodoreServer
     Leif Bloomquist, Greg Alekel
*/

#include <MicroView.h>
#include <elapsedMillis.h>
#include <SoftwareSerial.h>
#include <WiFlyHQ.h>

;  // Keep this here to pacify the Arduino pre-processor

#define WIFI_BAUD 2400
#define C64_BAUD  2400

#define COMMODORE_SERVER "commodoreserver.com"
#define COMMODORE_PORT 1541


// Configuration 0v2: Wifi Hardware, C64 Software.  -------------------------------------

// Wifi
// RxD is D0  (Hardware Serial)
// TxD is D1  (Hardware Serial)

// Additional RS-232 lines to C64 User Port
#define RTS  A5
#define DTR  A4
#define RI   A3
#define DCD  A2
#define CTS  A1
#define DSR  A0
#define TxD  6
#define RxD  5

SoftwareSerial C64Serial(RxD, TxD);
HardwareSerial& WifiSerial = Serial;
WiFly wifly;

// ----------------------------------------------------------

char temp[100];

void setup()
{
  uView.begin();	// begin of MicroView
  uView.setFontType(0);
  uView.clear(ALL);	// erase hardware memory inside the OLED controller

  C64Serial.begin(C64_BAUD);
  WifiSerial.begin(WIFI_BAUD);

  C64Serial.setTimeout(1000);

  pinMode(RTS, INPUT);

  display("Wi-Fi\nInit...");
  C64Serial.println("Wi-Fi Init...");

  boolean ok = wifly.begin(&WifiSerial, &C64Serial);

  if (ok)
  {
    display2("READY.");
  }
  else
  {
    display("Wi-Fi\nFailed!");
    C64Serial.println("Wi-Fi Failed!");
  }
}

void loop()
{
 
  C64Serial.println();
  C64Serial.println(F("Commodore 64 Wi-Fi Modem"));
  C64Serial.println(F("CommodoreServer Client"));
  ShowStats();
 
  while (true)
  {
    RawTCPConnect(COMMODORE_SERVER, COMMODORE_PORT);
  }
}

// ----------------------------------------------------------

void display(String message)
{
  uView.clear(PAGE);	// erase the memory buffer, when next uView.display() is called, the OLED will be cleared.
  uView.setCursor(0, 0);
  uView.println(message);
  uView.display();
}

void display2(String message)
{
  display(message);
  C64Serial.println(message);
}

// ----------------------------------------------------------

String GetInput()
{
  char in_char[100];
  int max_length = sizeof(in_char);

  int i = 0; // Input buffer pointer
  char key;

  while (true)
  {
    key = ReadByte(C64Serial); // Read in one character
    in_char[i] = key;
    C64Serial.write(key); // Echo key press back to the user.

    if ((key == '\b') && (i > 0)) i -= 2; // Handles back space.

    if (((int)key == 13) || (i == max_length - 1))
    { // The \n represents enter key.
      in_char[i] = 0; // Terminate the string with 0.
      return String(in_char);
    }
    i++;
    if (i < 0) i = 0;
  }
}

// ----------------------------------------------------------

void ShowStats()
{
  char mac[20];
  char ip[20];
  char ssid[20];

  wifly.getMAC(mac, 20);
  wifly.getIP(ip, 20);
  wifly.getSSID(ssid, 20);

  C64Serial.print(F("MAC Address: "));   C64Serial.println(mac);
  C64Serial.print(F("IP Address:  "));   C64Serial.println(ip);
  C64Serial.print(F("Wi-Fi SSID:  "));   C64Serial.println(ssid);
}


// ----------------------------------------------------------

void RawTCPConnect(String host, int port)
{
  sprintf(temp, "\nConnecting to %s", host.c_str() );
  display2(temp);

  wifly.setIpProtocol(WIFLY_PROTOCOL_TCP);

  boolean ok = wifly.open(host.c_str(), port);

  if (ok)
  {
    sprintf(temp, "Connected to %s", host.c_str() );
    display2(temp);
  }
  else
  {
    display2("Connect \nFailed!");
    return;
  }

  TerminalMode();
}

void TerminalMode()
{
  while (wifly.available() != -1) // -1 means closed
  {
    while (wifly.available() > 0)
    {
      C64Serial.write( wifly.read() );
    }

    while (C64Serial.available() > 0)
    {
      wifly.write( C64Serial.read() );
    }

    // Alternate check for open/closed state
    if (!wifly.isConnected())
    {
      break;
    }
  }

  wifly.close();
  display2("Connection closed");
}
