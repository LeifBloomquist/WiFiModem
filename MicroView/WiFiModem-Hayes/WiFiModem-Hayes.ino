/*
   Commodore 64 - MicroView - Wi-Fi Cart
   Hayes Emulation!!!
   Leif Bloomquist, Payton Byrd, Greg Alekel 
*/

#include <EEPROM.h>
#include <MicroView.h>
#include <elapsedMillis.h>
#include <SoftwareSerial.h>
#include <WiFlyHQ.h>
#include "..\..\..\hayesduino\ModemBase.h"

;  // Keep this here to pacify the Arduino pre-processor

#define WIFI_BAUD 2400
#define C64_BAUD  2400

// Configuration 0v2: Wifi Hardware, C64 Software.  -------------------------------------

// Wifi
// RxD is D0  (Hardware Serial)
// TxD is D1  (Hardware Serial)

// Additional RS-232 lines to C64 User Port
#define TxD  6
#define RxD  5

SoftwareSerial C64Serial(RxD, TxD);
HardwareSerial& WifiSerial = Serial;
WiFly wifly;

// Modem Stuff
ModemBase modem;

// Telnet Stuff
#define IAC 255

String lastHost = "";
int lastPort = 23;

//Misc
char temp[100];

// ----------------------------------------------------------
void dialout(char* host, ModemBase *modm)
{
	char* index;
	uint16_t port = 23;
	String hostname = String(host);
	char hostnamebuffer[81];

	for (int i = 0; i<81; ++i) hostnamebuffer[i] = '\0';

	if ((index = strstr(host, ":")) != NULL)
	{
		index[0] = '\0';
		hostname = String(host);
		port = atoi(index + 1);
	}

	if (hostname == "5551212")
	{
		hostname = "qlink.lyonlabs.org";
		port = 5190;
	}

	Telnet(hostname, port);

	delay(1000);
}

// ------------------------------------------------------------------------------------------------


void Display(String message)
{
	uView.clear(PAGE);	// erase the memory buffer, when next uView.display() is called, the OLED will be cleared.
	uView.setCursor(0, 0);
	uView.println(message);
	uView.display();
}

void DisplayBoth(String message, bool fix = true)
{
	C64Serial.println(message);

	if (fix)
	{
		message.replace(' ', '\n');
	}
	Display(message);
}

// ------------------------------------------------------------------------------------------------

void setup()
{
	// MicroView
	uView.begin();	
	uView.setFontType(0);
	uView.clear(ALL);	// erase hardware memory inside the OLED controller

	// Serial Ports
	C64Serial.begin(C64_BAUD);
	WifiSerial.begin(WIFI_BAUD);
	C64Serial.setTimeout(1000);

	// Virtual Modem (Hayesduino)
	DisplayBoth(F("Modem Init..."));
	delay(1000);
	modem.begin(&WifiSerial, &C64Serial, &dialout);

	// Wi-Fi
	DisplayBoth(F("Wi-Fi Init..."));
	boolean ok = wifly.begin(&WifiSerial, &C64Serial);

	if (ok)
	{
		DisplayBoth("Wi-Fi Modem Ready!");
	}
	else
	{
		DisplayBoth("Wi-Fi Failed!");
	}

	C64Serial.println(F("\n\nCommodore 64 Wi-Fi Modem"));
	C64Serial.println(F("Hayes Modem Emulation Mode\n"));
	ShowStats();
	C64Serial.println(F("READY."));
}

void loop()
{
	// Check for remote disconnect
	if (modem.getIsConnected() && !wifly.isConnected())
	{
		modem.disconnect();
		return;
	}

	// Check for new remote connection
	if (!modem.getIsConnected() && wifly.isConnected())
	{
		modem.connect(&wifly);
		for (int i = 0; i < 25; ++i)
		{
			wifly.println(".");   // What's this for??
		}
		wifly.println(F("CONNECTING TO SYSTEM."));		
	}

	// If connected, handle incoming data
	char inbound;

	if (wifly.isConnected())
	{
		if (!modem.getIsCommandMode() && wifly.available() > 0)
		{
			//digitalWrite(DCE_RTS, HIGH);
			inbound = wifly.read();
			modem.write(inbound);			
		}
		else if (!modem.getIsCommandMode() && wifly.available() == 0)
		{
			//digitalWrite(DCE_RTS, LOW);
		}
		else if (modem.getIsCommandMode() && wifly.available() > 0)
		{
			wifly.println(F("modem is in command mode."));
		}
	}
	else if (modem.getIsConnected() || !modem.getIsCommandMode())   // Handle unexpected remote disconnect, I think?
	{
//		delay(5000);
		modem.disconnect();
	}
	else if (!modem.getIsConnected() &&	modem.getIsCommandMode())
	{
		//digitalWrite(DCE_RTS, LOW);
	}
	//else if(digitalRead(DTE_CTS) == HIGH)
	//{
	//	digitalWrite(DTE_RTS, LOW);
	//}

	modem.processData(&wifly);

	//digitalWrite(DCE_RTS, HIGH);

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

boolean Telnet(String host, int port)
{
	sprintf(temp, "\nConnecting to %s", host.c_str());
	DisplayBoth(temp, false);

	wifly.setIpProtocol(WIFLY_PROTOCOL_TCP);

	boolean ok = wifly.open(host.c_str(), port);

	if (ok)
	{
		sprintf(temp, "Connected to %s", host.c_str());
		DisplayBoth(temp, false);
	}
	else
	{
		DisplayBoth("Connect Failed!");
		return false;
	}

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
	DisplayBoth("Connection closed", false);
}

// Raw Terminal Mode.  There is no escape.
void RawTerminalMode()
{
	bool changed = false;
	long rnxv_chars = 0;
	long c64_chars = 0;

	C64Serial.println(F("*** Terminal Mode (Debug) ***"));
	Display(F("Debug\nMode"));

	while (true)
	{
		while (wifly.available() > 0)
		{
			rnxv_chars++;
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
			sprintf(temp, "Wifi:%ld\n\nC64: %ld", rnxv_chars, c64_chars);
			Display(temp);
		}
	}
}


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
		if (verbint == -1) continue;                //          if negotiation verb character is nothing break the routine  (should never happen)

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

