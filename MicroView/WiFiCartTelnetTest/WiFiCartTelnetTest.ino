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

// Telnet Stuff
#define IAC 255

String lastHost = "";

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
  
  display2("Wi-Fi \nInit...");
  
  boolean ok = wifly.begin(&WifiSerial, &C64Serial);
  
  if (ok)
  {
     display2("READY.");
  }
  else
  {
    display2("Wi-Fi \nFailed!");
  }
}
  
void loop()
{  
   C64Serial.println();
   C64Serial.println("Commodore 64 Wi-Fi Modem");
   C64Serial.println("Telnet Test");
   C64Serial.print("host: ");
   C64Serial.setTimeout(1000);
   
   String readString = GetInput();
   readString.trim();
         
   if (readString.length() > 0)
   {
     lastHost = readString;
     Telnet(readString);
   }
   else
   {
      Telnet(lastHost);
   }
}

// ----------------------------------------------------------

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

// ----------------------------------------------------------

String GetInput() 
{
  char in_char[100];
  int max_length=sizeof(in_char);
  
  int i=0; // Input buffer pointer
  char key;
  
  while(true) 
  {
      key=ReadByte(C64Serial); // Read in one character
      in_char[i]=key;      
      C64Serial.write(key); // Echo key press back to the user.
      
      if ((key=='\b')&&(i>0)) i-=2; // Handles back space.
      
      if (((int)key==13)||(i==max_length-1)) 
      { // The \n represents enter key.
        in_char[i]=0; // Terminate the string with 0.
        return String(in_char);
      }
      i++;
      if (i<0) i=0;
  }
}

// ----------------------------------------------------------

void Telnet(String host)
{
   sprintf(temp, "Connecting to %s", host.c_str() );   
   display2(temp);
   
   wifly.setIpProtocol(WIFLY_PROTOCOL_TCP);
   
   boolean ok = wifly.open(host.c_str(), 23);
   
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
   C64Serial.println("\nDetermining Connection Type\n"); 
   CheckTelnet();
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
     
     // Alternate check
     if (!wifly.isConnected())
     {
        break;
     }
   }
   
    wifly.close();
    display2("Connection closed");
}



void CheckTelnet()     //  inquiry host for telnet parameters / negotiate telnet parameters with host
{
  int inpint, verbint, optint;                         //    telnet parameters as integers
  
  // Wait for first character
  inpint = PeekByte(wifly);  
  if (inpint != IAC)  
  {
    C64Serial.println("Raw TCP Connection Detected");
    return;   // Not a telnet session
  }
  
  C64Serial.println("Telnet Connection Detected");
  
  // IAC handling
//  SendTelnetParameters();    // Start off with negotiating
  
  while (true)
  {
    inpint = PeekByte(wifly);       //      peek at next character
     
    if (inpint != IAC)  
    {
      C64Serial.print("Received inpint=");
      C64Serial.println(inpint);
      C64Serial.println("End of IAC Negotiation");
      return;   // Let Terminal mode handle character
    }
    
    inpint = ReadByte(wifly);    // Eat IAC character    
    verbint = ReadByte(wifly);   // receive negotiation verb character        
    if (verbint == - 1) continue;                //          if negotiation verb character is nothing break the routine  (should never happen)
        
    switch (verbint) {                             //          evaluate negotiation verb character
        case IAC:                                    //          if negotiation verb character is 255 (restart negotiation)
          continue;                                     //            break the routine
          
        case 251:                                    //          if negotiation verb character is 251 (will) or
        case 252:                                    //          if negotiation verb character is 252 (wont) or
        case 253:                                    //          if negotiation verb character is 253 (do) or
        case 254:                                    //          if negotiation verb character is 254 (dont)
          optint = ReadByte(wifly);                  //            receive negotiation option character         
          if (optint == -1) continue;                   //            if negotiation option character is nothing break the routine  (should never happen) 
                  
          switch (optint) {            
            
            case 3:                                   // if negotiation option character is 3 (suppress-go-ahead)
              SendTelnetDoWill(verbint, optint);
              break;
              
          default:                                     //        if negotiation option character is none of the above (all others)
              SendTelnetDontWont(verbint, optint);
            break;                                     //          break the routine
      }
    }
  } 
}

void SendTelnetDoWill(int verbint, int optint)
{
  wifly.write(IAC);                           // send character 255 (start negotiation)
  wifly.write(verbint == 253 ? 253 : 251);    // send character 253 (do) if negotiation verb character was 253 (do) else send character 251 (will)
  wifly.write(optint);
}

void SendTelnetDontWont(int verbint, int optint)
{
  wifly.write(IAC);                           // send character 255 (start negotiation)
  wifly.write(verbint == 253 ? 252 : 254);    // send character 252 (wont) if negotiation verb character was 253 (do) else send character 254 (dont)
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

int ReadByte(Stream& in)
{
    while (in.available() == 0) {}   
    return in.read();
}

int PeekByte(Stream& in)
{
    while (in.available() == 0) {}   
    return in.peek();
}

