/*
     Commodore 64 - MicroView - Wi-fi Cart
     Comms Test
     Leif Bloomquist
*/

#include <MicroView.h>
#include <elapsedMillis.h>
#include <SoftwareSerial.h>

;  // Keep this here to pacify the Arduino pre-processor

#define C64_BAUD  9600
#define WIFI_BAUD 9600

// Configuration #2: Wifi Hardware, C64 Software.  -------------------------------------

// Wifi
// RxD is D0  (Hardware Serial) jumpered to D2
// TxD is D1  (Hardware Serial) jumpered to D3

// Additional RS-232 lines to C64 User Port
#define RTS  A5
#define DTR  A4
#define RI   A3
#define DCD  A2
#define CTS  A1
#define DSR  A0
#define RxD  5 
#define TxD  6

SoftwareSerial C64Serial(RxD, TxD);   
HardwareSerial& WifiSerial = Serial;  

// ----------------------------------------------------------


long rnxv_chars=0;
long c64_chars=0;

char temp[100];

void setup()
{
  uView.begin();	// begin of MicroView  
  uView.setFontType(0);
  uView.clear(ALL);	// erase hardware memory inside the OLED controller
   
  C64Serial.begin(C64_BAUD);
  WifiSerial.begin(WIFI_BAUD);
  
  C64Serial.println("READY.");
  display("READY.");
}
  
void loop()
{
   Send10Kchars();
   delay(5000);
   return;
   
   bool changed=false;
   
   while (WifiSerial.available() > 0)
   {
      rnxv_chars++;
      C64Serial.write( WifiSerial.read() );
      changed = true;
   }
   
   while (C64Serial.available() > 0)
   {
      c64_chars++;
      WifiSerial.write( C64Serial.read() );
      changed = true;
   }

   if (changed)
   {  
      sprintf(temp, "Wifi:%ld\n\nC64: %ld", rnxv_chars, c64_chars);
      display(temp);
   }
}
  
void display(String message)
{
   uView.clear(PAGE);	// erase the memory buffer, when next uView.display() is called, the OLED will be cleared.
   uView.setCursor(0,0);  
   uView.println(message);
   uView.display();
}

void Send10Kchars()
{
  char chars100[] = "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
  
  C64Serial.println("\9600 baud test START");
  for (int i=0; i<100; i++)
  {
     C64Serial.print(i);
     C64Serial.print("  ");
     C64Serial.println(chars100);
  } 
   C64Serial.println("\nEND");
}
