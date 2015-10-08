/*
     Commodore 64 - MicroView - Wi-fi Cart
     Simple Menu Sketch
     Leif Bloomquist
*/

#include <MicroView.h>
#include <elapsedMillis.h>
#include <SoftwareSerial.h>
#include <WiFlyHQ.h>
#include <digitalWriteFast.h>

;  // Keep this here to pacify the Arduino pre-processor

#define FLOWCONTROL 1

#define WIFI_BAUD 9600
#define C64_BAUD  9600 

// Configuration 0v2: Wifi Hardware, C64 Software.  -------------------------------------

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

//#ifdef FLOWCONTROL

  pinModeFast(C64_RTS, INPUT);
  pinModeFast(C64_CTS, OUTPUT);

  pinModeFast(WIFI_RTS, INPUT);
  pinModeFast(WIFI_CTS, OUTPUT);

  // Force CTS High on both sides to start
  digitalWriteFast(WIFI_CTS, LOW);
  digitalWriteFast(C64_CTS, HIGH);

//#endif

  Display(F("Wi-Fi\nInit..."));
  C64Serial.println(F("Wi-Fi Init..."));

  boolean ok = wifly.begin(&WifiSerial, &C64Serial);

  if (ok)
  {
      DisplayBoth(F("READY."));
  }
  else
  {
      Display(F("Wi-Fi\nFailed!"));
      C64Serial.println(F("Wi-Fi Failed!"));
      RawTerminalMode();
  }
}

void loop()
{
  C64Serial.println();
  C64Serial.println(F("Commodore Wi-Fi Modem"));
  ShowInfo();

  while (true)
  {
	  C64Serial.println();
	  C64Serial.println(F("1. Telnet to host or BBS"));
      C64Serial.println(F("2. Wait for incoming connection"));
      C64Serial.println(F("3. Configuration"));
      C64Serial.println();
      C64Serial.print(F("Select:"));      

      int option = ReadByte(C64Serial);
      C64Serial.println((char)option);
      
    switch (option)
    {
      case '1': doTelnet();
        return;

      case '2': Incoming();
        return;
      
      case '3': Configuration();
        continue;

      case '4': RawTerminalMode();
        return;

	  case '5': RawTerminalModeFlowControl();
		  return;

	  case '\n': 
      case '\r':
      case ' ':
          continue;

      default: C64Serial.println(F("Unknown option, try again"));
        continue;
    }
  }
}

void Configuration()
{
    while (true)
    {
        C64Serial.println();
        C64Serial.println(F("Configuration Menu"));
        C64Serial.println();
        C64Serial.println(F("1. Display Current Configuration"));
        C64Serial.println(F("2. Change SSID"));
        C64Serial.println(F("3. Direct Terminal Mode (Debug)"));
        C64Serial.println(F("4. Main Menu"));
        C64Serial.println();
        C64Serial.print(F("Select:"));

        int option = ReadByte(C64Serial);
        C64Serial.println((char)option);

        switch (option)
        {
            case '1': ShowInfo();
                break;

            case '2': //
                break;

            case '3': RawTerminalMode();
                return;

            case '4': return;

            case '5': RawTerminalModeFlowControl();
                return;

            case '\n':
            case '\r':
            case ' ':
                continue;

            default: C64Serial.println(F("Unknown option, try again"));
                continue;
        }
    }
}


// ----------------------------------------------------------

void Display(String message)
{
  uView.clear(PAGE);	// erase the memory buffer, when next uView.display() is called, the OLED will be cleared.
  uView.setCursor(0, 0);
  uView.println(message);
  uView.display();
}

void DisplayBoth(String message)
{
  Display(message);
  C64Serial.println(message);
}

// ----------------------------------------------------------

void doTelnet()
{
    int port = lastPort;

    C64Serial.print("\nTelnet host: ");
    String hostName = GetInput();

    if (hostName.length() > 0)
    {
        C64Serial.print(F("\nPort ("));
        C64Serial.print(lastPort);
        C64Serial.print(F("): "));

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

        Telnet(hostName, port);
    }
    else
    {
        if (lastHost.length() > 0)
        {
            Telnet(lastHost, lastPort);
        }
        else
        {
            return;
        }
    }
}


String GetInput()
{
    String temp = GetInput_Raw();
    temp.trim();
    return temp;
}

String GetInput_Raw()
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

void ShowInfo()
{
  char mac[20];
  char ip[20];
  char ssid[20];

  wifly.getMAC(mac, 20);
  wifly.getIP(ip, 20);
  wifly.getSSID(ssid, 20);

  char temp[50]; 
  sprintf(temp, "IP Address \n%s", ip);
  Display(temp);
  
  C64Serial.println();
  C64Serial.print(F("MAC Address: "));   C64Serial.println(mac);
  C64Serial.print(F("IP Address:  "));   C64Serial.println(ip);
  C64Serial.print(F("Wi-Fi SSID:  "));   C64Serial.println(ssid);
}

void Incoming()
{
    int localport = wifly.getPort();

    C64Serial.println();
    C64Serial.print(F("Incoming port ("));
    C64Serial.print(localport);
    C64Serial.print(F("): "));
   
    String strport = GetInput();
    
    if (strport.length() > 0)
    {
        localport = strport.toInt();
        wifly.setPort(localport);
    }

    localport = wifly.getPort();
    
    C64Serial.println();
    C64Serial.print(F("Waiting for connections on port "));
    C64Serial.println(localport);
  
    // Idle here until connected
    while (!wifly.isConnected())  {}

    C64Serial.println(F("Incoming Connection")); 
 
    TerminalMode();
}


// ----------------------------------------------------------

void Telnet(String host, int port)
{
  sprintf(temp, "\nConnecting to %s", host.c_str() );
  DisplayBoth(temp);

  wifly.setIpProtocol(WIFLY_PROTOCOL_TCP);

  boolean ok = wifly.open(host.c_str(), port);

  if (ok)
  {
    sprintf(temp, "Connected to %s", host.c_str() );
    DisplayBoth(temp);
  }
  else
  {
    DisplayBoth("Connect \nFailed!");
    return;
  }

  C64Serial.println(F("Determining Connection Type"));
  CheckTelnet();
  TerminalMode();
}

void TerminalMode()
{
  C64Serial.println(F("*** Terminal Mode ***"));

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
  DisplayBoth(F("Connection closed"));
}

// Raw Terminal Mode.  There is no escape.
void RawTerminalMode()
{
  bool changed=false;
  long rnxv_chars=0;
  long c64_chars=0;

  C64Serial.println(F("*** Terminal Mode (Debug) ***"));

  while (true)
  {
		while (wifly.available() > 0)
		{
			rnxv_chars++;
			C64Serial.write( wifly.read() );
			changed=true;
		}

		while (C64Serial.available() > 0)
		{
			c64_chars++;
			wifly.write( C64Serial.read() );
			changed=true;
		}
    
		if (changed)
		{  
			sprintf(temp, "Wifi:%ld\n\nC64: %ld", rnxv_chars, c64_chars);
			Display(temp);
		}
  }
}

// Raw Terminal Mode.  There is no escape.
void RawTerminalModeFlowControl()
{
	bool changed = false;
	long rnxv_chars = 0;
	long c64_chars = 0;
    long c64_rts = 0;

	C64Serial.println(F("*** Terminal Mode (Debug+Flow) ***"));
	Display(F("Debug\nMode"));

	while (true)
	{
	//	C64Serial.flush();   Goog says this is important, but it locks up C64 comms?

		while (wifly.available() > 0)
		{
			rnxv_chars++;

            // Check that C64 is ready to receive
            if (digitalReadFast(C64_RTS) == LOW)  // If not...
            {
                digitalWriteFast(WIFI_CTS, HIGH);     // ..stop data from Wi-Fi and wait
                //Display("RTS");                

                while (digitalReadFast(C64_RTS) == LOW) {};  // 

                c64_rts++;
            };            
            digitalWriteFast(WIFI_CTS, LOW);

			C64Serial.write(wifly.read()); 

			changed = true;
		}       

		while (C64Serial.available() > 0)
		{            
            c64_chars++;
			wifly.write(C64Serial.read());
			changed = true;
		}

        if (true) // changed)
		{
            sprintf(temp, "Wifi:%ld\n\nC64: %ld\n\nRTS: %ld", rnxv_chars, c64_chars, c64_rts);
			Display(temp);
		}
	}
}

/*
inline void DoFlowControl()
{  
    // Check that C64 is ready to receive
    while (digitalReadFast(C64_RTS) == LOW)  // If not...
    {
        digitalWriteFast(WIFI_CTS, LOW);     // ..stop data from Wi-Fi
    };
    digitalWriteFast(WIFI_CTS, HIGH);
}
*/


void CheckTelnet()     //  inquiry host for telnet parameters / negotiate telnet parameters with host
{
  int inpint, verbint, optint;                         //    telnet parameters as integers

  // Wait for first character
  inpint = PeekByte(wifly);
  if (inpint != IAC)
  {
    C64Serial.println(F("Raw TCP Connection Detected"));
    return;   // Not a telnet session
  }

  C64Serial.println(F("Telnet Connection Detected"));

  // IAC handling
  SendTelnetParameters();    // Start off with negotiating

  while (true)
  {
    inpint = PeekByte(wifly);       //      peek at next character

    if (inpint != IAC)
    {
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