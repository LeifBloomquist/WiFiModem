/*
     Commodore 64 - MicroView - Wi-fi Cart
     Telnet Test
     Leif Bloomquist
*/

#include <MicroView.h>
#include <elapsedMillis.h>
#include <SoftwareSerial.h>
#include <WiFlyHQ.h>

;  // Keep this here to pacify the Arduino pre-processor

#define WIFI_BAUD 4800
#define C64_BAUD  9600

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
 
  pinMode(RTS, INPUT);
  
  display2("Wi-Fi\nInit...");
  
  boolean ok = wifly.begin(&WifiSerial, &C64Serial);
  
  if (ok)
  {
     display2("READY.");
  }
  else
  {
    display2("Failed!");
  }
}
  
void loop()
{  
   C64Serial.println();
   C64Serial.println("Commodore 64 Wi-Fi Modem");
   C64Serial.println("Telnet Test");
   C64Serial.print("Host: ");
   C64Serial.setTimeout(1000);
   
   String readString = "";
   bool changed=false;   
   long c64_chars=0;
   
   while (true)
   {
     while (C64Serial.available() > 0)
     {
       c64_chars++;
       char c = C64Serial.read();
       C64Serial.print(c); // Echo
       
       if ((int)c == 13)
       {
         Telnet(readString);
         return;
       }
       else
       {      
         readString += c;
       } 
     }
   }   
}

void display(String message)
{
   uView.clear(PAGE);	// erase the memory buffer, when next uView.display() is called, the OLED will be cleared.
   uView.setCursor(0,0);  
   uView.println(message);
   uView.display();
}

void display2(String message)
{
   display(message);
   C64Serial.println(message);
}

void Telnet (String host)
{
   sprintf(temp, "\n\nOpening Telnet connection to %s", host.c_str() );   
   display2(temp);
   
   wifly.setIpProtocol(WIFLY_PROTOCOL_TCP);
   
   boolean ok = wifly.open(host.c_str(), 23);
   
   if (ok)
   {
      display2("Connected");
   }
   else
   {
     display2("Connect\nFailed!");
   }
   
   TerminalMode();
   
   wifly.close();
   
  
   display2("Connection closed");
}

void TerminalMode()
{
   C64Serial.println("\nEntering Terminal Mode\n");
  
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
   }
}
