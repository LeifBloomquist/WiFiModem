/***********************************************
HAYESDUINO PROJECT - COPYRIGHT 2013, PAYTON BYRD

This version modded by Leif Bloomquist for use on the Wi-Fi Modem!

Project homepage: http://hayesduino.codeplex.com
License: http://hayesduino.codeplex.com/license
***********************************************/

#include <WiFlyHQ.h>
#include "..\..\..\hayesduino\ModemBase.h"

int ModemBase::available(void) 
{ 
	if(_serial) return _serial->available(); 
	return 0; 
}

int ModemBase::peek(void) 
{
	if(_serial) return _serial->peek(); 
    return 0; 
}

int ModemBase::read(void) 
{
	if(_serial) return _serial->read(); 
    return 0; 
}

void ModemBase::flush(void) 
{
	if(_serial) _serial->flush(); 
}
size_t ModemBase::write(uint8_t var) 
{
	if(_serial) return _serial->write(var); 
	return 0; 
}

void ModemBase::resetCommandBuffer()
{
	memset(_commandBuffer, 0, strlen(_commandBuffer));
}

ModemBase::ModemBase()
{
	pinMode(RI , OUTPUT);
	pinMode(CTS, OUTPUT);	
	pinMode(DSR, OUTPUT);
	pinMode(DTR, OUTPUT);
	pinMode(DCD, INPUT);
	pinMode(RTS, INPUT);
	
	digitalWrite(RI, LOW);
	digitalWrite(CTS, HIGH);
	digitalWrite(DSR, HIGH);
	digitalWrite(DTR, toggleCarrier(false));
}

void ModemBase::begin(Stream *serial, WiFly *wifly, void(*onDialoutHandler)(char*), void(*onDisconnectHandler)())
{
	_serial = serial;
	_wifly = wifly;
	onDialout = onDialoutHandler;
	onDisconnect = onDisconnectHandler;
	
	_escapeCount = 0;

	_isCommandMode = true;
	_isConnected = false;
	_isRinging = false;

	loadDefaults();

	digitalWrite(DTR, toggleCarrier(false));    // These look weird - compare with above and RS232 spec
	digitalWrite(RTS, HIGH);
	digitalWrite(RI, LOW);

	resetCommandBuffer();
}

void ModemBase::loadDefaults(void)
{
	_echoOn = true;
	_verboseResponses = true;
	_quietMode = false;
	_S0_autoAnswer = false;
}

uint32_t ModemBase::getBaudRate(void)
{
	return _baudRate;
}

void ModemBase::setDefaultBaud(uint32_t baudRate)
{
	_baudRate = baudRate;
}

void ModemBase::setDcdInverted(char isInverted)
{
	_isDcdInverted = isInverted;
}

bool ModemBase::getDcdInverted(void)
{
	return _isDcdInverted == 1;
}

bool ModemBase::getIsConnected(void)
{
	return _isConnected;
}

bool ModemBase::getIsRinging(void)
{
	return _isRinging;
}


bool ModemBase::getIsCommandMode(void)
{
	return _isCommandMode;
}

int ModemBase::toggleCarrier(boolean isHigh)
{

	int result = 0; //_isDcdInverted ? (isHigh ? LOW : HIGH) : (isHigh ? HIGH : LOW);
	switch(_isDcdInverted)
	{
	case 0: result = (int)(!isHigh); break;
	case 1: result = (int)(isHigh); break;
	case 2: result = LOW;
	}
	//digitalWrite(STATUS_LED, result);
	return result;
}

void ModemBase::disconnect()
{
	_isCommandMode = true;
	_isConnected = false;
	_isRinging = false;

	// TODO - According to http://totse2.net/totse/en/technology/telecommunications/trm.html
	//		  The BBS should detect <CR><LF>NO CARRIER<CR><LF> as a dropped carrier sequences.
	//		  DMBBS does not honor this and so I haven't sucessfully tested it, thus it's commented out.

	//delay(100);
	//print('\r'); 
	//print((char)_S4_lfCharacter);
	//print(F("NO CARRIER"));
	//print('\r'); 
	//print((char)_S4_lfCharacter);

	digitalWrite(RTS, LOW);
	digitalWrite(DTR, toggleCarrier(false));
	onDisconnect();
}

void ModemBase::processCommandBuffer()
{
	for (int i=0; i < strlen(_commandBuffer); ++i)
	{
		_commandBuffer[i] = toupper(_commandBuffer[i]);
	}

	if (strcmp(_commandBuffer, ("AT/")) == 0)
	{
		strcpy(_commandBuffer, _lastCommandBuffer);
	}
	
	if (strcmp(_commandBuffer, ("ATZ")) == 0)
	{
		loadDefaults();
		printOK();
	}
	else if (strcmp(_commandBuffer, ("ATI")) == 0)
	{
		ShowInfo();
		printOK();
	}
	else if (strcmp(_commandBuffer, ("AT&F")) == 0)
	{
		if(strcmp(_lastCommandBuffer, ("AT&F")) == 0)
		{
			loadDefaults();
			printOK();
		}
		else
		{
			_serial->println(F("send command again to verify."));
		}
	}
	else if(strcmp(_commandBuffer, ("ATA")) == 0)
	{
		answer();
	}
	else if (strcmp(_commandBuffer, ("ATD")) == 0 || strcmp(_commandBuffer, ("ATO")) == 0)
	{
		if (_isConnected)
		{
			_isCommandMode = false;
		}
		else
		{
			printError();
		}
	}
	else if(
		strncmp(_commandBuffer, ("ATDT "), 5) == 0 ||
		strncmp(_commandBuffer, ("ATDP "), 5) == 0 ||
		strncmp(_commandBuffer, ("ATD "), 4) == 0
		)
	{
		onDialout(strstr(_commandBuffer, " ") + 1);	
		resetCommandBuffer();  // This avoids port# string fragments on subsequent calls
	}
	else if (strncmp(_commandBuffer, ("ATDT"), 4) == 0)
	{
		onDialout(strstr(_commandBuffer, "ATDT") + 4);
		resetCommandBuffer();  // This avoids port# string fragments on subsequent calls
	}
	else if ((strcmp(_commandBuffer, ("ATH0")) == 0 || strcmp(_commandBuffer, ("ATH")) == 0))
	{
		disconnect();
	}
	else if(strncmp(_commandBuffer, ("AT"), 2) == 0)
	{
		if(strstr(_commandBuffer, ("E0")) != NULL)
		{
			_echoOn = false;
		}

		if(strstr(_commandBuffer, ("E1")) != NULL)
		{
			_echoOn = true;
		}

		if(strstr(_commandBuffer, ("Q0")) != NULL)
		{
			_verboseResponses = false;
			_quietMode = false;
		}

		if(strstr(_commandBuffer, ("Q1")) != NULL)
		{
			_quietMode = true;
		}

		if(strstr(_commandBuffer, ("V0")) != NULL)
		{
			_verboseResponses = false;
		}

		if(strstr(_commandBuffer, ("V1")) != NULL)
		{
			_verboseResponses = true;
		}

		if(strstr(_commandBuffer, ("X0")) != NULL)
		{
			// TODO
		}
		if(strstr(_commandBuffer, ("X1")) != NULL)
		{
			// TODO
		}

		char *currentS;
		char temp[100];

		int offset = 0;
		if((currentS = strstr(_commandBuffer, ("S0="))) != NULL)
		{
			offset = 3;
			while(currentS[offset] != '\0' && isDigit(currentS[offset]))
			{
				offset++;
			}

			memset(temp, 0, 100);
			strncpy(temp, currentS + 3, offset - 3);
			_S0_autoAnswer = atoi(temp);
		}
/*
		if((currentS = strstr(_commandBuffer, ("S1="))) != NULL)
		{
			offset =3;
			while(currentS[offset] != '\0'
				&& isDigit(currentS[offset]))
			{
				offset++;
			}

			memset(temp, 0, 100);
			strncpy(temp, currentS + 3, offset - 3);
			_S1_ringCounter = atoi(temp);
#if DEBUG == 1
			lggr.print(F("S1=")); lggr.println(temp);
#endif
		}

		if((currentS = strstr(_commandBuffer, ("S2="))) != NULL)
		{
			offset =3;
			while(currentS[offset] != '\0'
				&& isDigit(currentS[offset]))
			{
				offset++;
			}

			memset(temp, 0, 100);
			strncpy(temp, currentS + 3, offset - 3);
			_S2_escapeCharacter = atoi(temp);
#if DEBUG == 1
			lggr.print(F("S2=")); lggr.println(temp);
#endif
		}

		if((currentS = strstr(_commandBuffer, ("S3="))) != NULL)
		{
			offset =3;
			while(currentS[offset] != '\0'
				&& isDigit(currentS[offset]))
			{
				offset++;
			}

			memset(temp, 0, 100);
			strncpy(temp, currentS + 3, offset - 3);
			_S3_crCharacter = atoi(temp);
#if DEBUG == 1
			lggr.print(F("S3=")); lggr.println(temp);
#endif
		}

		if((currentS = strstr(_commandBuffer, ("S4="))) != NULL)
		{
			offset =3;
			while(currentS[offset] != '\0'
				&& isDigit(currentS[offset]))
			{
				offset++;
			}

			memset(temp, 0, 100);
			strncpy(temp, currentS + 3, offset - 3);
			_S4_lfCharacter = atoi(temp);
#if DEBUG == 1
			lggr.print(F("S4=")); lggr.println(temp);
#endif
		}

		if((currentS = strstr(_commandBuffer, ("S5="))) != NULL)
		{
			offset =3;
			while(currentS[offset] != '\0'
				&& isDigit(currentS[offset]))
			{
				offset++;
			}

			memset(temp, 0, 100);
			strncpy(temp, currentS + 3, offset - 3);
			_S5_bsCharacter = atoi(temp);
#if DEBUG == 1
			lggr.print(F("S5=")); lggr.println(temp);
#endif
		}

		if((currentS = strstr(_commandBuffer, ("S6="))) != NULL)
		{
			offset =3;
			while(currentS[offset] != '\0'
				&& isDigit(currentS[offset]))
			{
				offset++;
			}

			memset(temp, 0, 100);
			strncpy(temp, currentS + 3, offset - 3);
			_S6_waitBlindDial = atoi(temp);
#if DEBUG == 1
			lggr.print(F("S6=")); lggr.println(temp);
#endif
		}

		if((currentS = strstr(_commandBuffer, ("S7="))) != NULL)
		{
			offset =3;
			while(currentS[offset] != '\0'
				&& isDigit(currentS[offset]))
			{
				offset++;
			}

			memset(temp, 0, 100);
			strncpy(temp, currentS + 3, offset - 3);
			_S7_waitForCarrier = atoi(temp);
#if DEBUG == 1
			lggr.print(("S7=")); lggr.println(temp);
#endif
		}

		if((currentS = strstr(_commandBuffer, ("S8="))) != NULL)
		{
			offset =3;
			while(currentS[offset] != '\0'
				&& isDigit(currentS[offset]))
			{
				offset++;
			}

			memset(temp, 0, 100);
			strncpy(temp, currentS + 3, offset - 3);
			_S8_pauseForComma = atoi(temp);
#if DEBUG == 1
			lggr.print(F("S8=")); lggr.println(temp);
#endif
		}

		if((currentS = strstr(_commandBuffer, ("S9="))) != NULL)
		{
			offset =3;
			while(currentS[offset] != '\0'
				&& isDigit(currentS[offset]))
			{
				offset++;
			}

			memset(temp, 0, 100);
			strncpy(temp, currentS + 3, offset - 3);
			_S9_cdResponseTime = atoi(temp);
#if DEBUG == 1
			lggr.print(F("S9=")); lggr.println(temp);
#endif
		}

		if((currentS = strstr(_commandBuffer, ("S10="))) != NULL)
		{
			offset =4;
			while(currentS[offset] != '\0'
				&& isDigit(currentS[offset]))
			{
				offset++;
			}

			memset(temp, 0, 100);
			strncpy(temp, currentS + 4, offset - 4);
			_S10_delayHangup = atoi(temp);
#if DEBUG == 1
			lggr.print(F("S10=")); lggr.println(temp);
#endif
		}

		if((currentS = strstr(_commandBuffer, ("S11="))) != NULL)
		{
			offset =4;
			while(currentS[offset] != '\0'
				&& isDigit(currentS[offset]))
			{
				offset++;
			}

			memset(temp, 0, 100);
			strncpy(temp, currentS + 4, offset - 4);
			_S11_dtmf = atoi(temp);
#if DEBUG == 1
			lggr.print(F("S11=")); lggr.println(temp);
#endif
		}

		if((currentS = strstr(_commandBuffer, ("S12="))) != NULL)
		{
			offset =4;
			while(currentS[offset] != '\0'
				&& isDigit(currentS[offset]))
			{
				offset++;
			}

			memset(temp, 0, 100);
			strncpy(temp, currentS + 4, offset - 4);
			_S12_escGuardTime = atoi(temp);
#if DEBUG == 1
			lggr.print(F("S12=")); lggr.println(temp);
#endif
		}

		if((currentS = strstr(_commandBuffer, ("S18="))) != NULL)
		{
			offset =4;
			while(currentS[offset] != '\0'
				&& isDigit(currentS[offset]))
			{
				offset++;
			}

			memset(temp, 0, 100);
			strncpy(temp, currentS + 4, offset - 4);
			_S18_testTimer = atoi(temp);
#if DEBUG == 1
			lggr.print(F("S18=")); lggr.println(temp);
#endif
		}

		if((currentS = strstr(_commandBuffer, ("S25="))) != NULL)
		{
			offset =4;
			while(currentS[offset] != '\0'
				&& isDigit(currentS[offset]))
			{
				offset++;
			}

			memset(temp, 0, 100);
			strncpy(temp, currentS + 4, offset - 4);
			_S25_delayDTR = atoi(temp);
#if DEBUG == 1
			lggr.print(F("S25=")); lggr.println(temp);
#endif
		}

		if((currentS = strstr(_commandBuffer, ("S26="))) != NULL)
		{
			offset =4;
			while(currentS[offset] != '\0'
				&& isDigit(currentS[offset]))
			{
				offset++;
			}

			memset(temp, 0, 100);
			strncpy(temp, currentS + 4, offset - 4);
			_S26_delayRTS2CTS = atoi(temp);
#if DEBUG == 1
			lggr.print(F("S26=")); lggr.println(temp);
#endif
		}

		if((currentS = strstr(_commandBuffer, ("S30="))) != NULL)
		{
			offset =4;
			while(currentS[offset] != '\0'
				&& isDigit(currentS[offset]))
			{
				offset++;
			}

			memset(temp, 0, 100);
			strncpy(temp, currentS + 4, offset - 4);
			_S30_inactivityTimer = atoi(temp);
#if DEBUG == 1
			lggr.print(F("S30=")); lggr.println(temp);
#endif
		}

		if((currentS = strstr(_commandBuffer, ("S37="))) != NULL)
		{
			offset =4;
			while(currentS[offset] != '\0'
				&& isDigit(currentS[offset]))
			{
				offset++;
			}

			memset(temp, 0, 100);
			strncpy(temp, currentS + 4, offset - 4);
			_S37_lineSpeed = atoi(temp);
#if DEBUG == 1
			lggr.print(F("S37=")); lggr.println(temp);
#endif

			EEPROM.write(S37_ADDRESS, _S37_lineSpeed);

			setLineSpeed();

#if DEBUG == 1
			lggr.print(F("Set Baud Rate: ")); lggr.println(_baudRate);
#endif
		}

		if((currentS = strstr(_commandBuffer, ("S38="))) != NULL)
		{
			offset =4;
			while(currentS[offset] != '\0'
				&& isDigit(currentS[offset]))
			{
				offset++;
			}

			memset(temp, 0, 100);
			strncpy(temp, currentS + 4, offset - 4);
			_S38_delayForced = atoi(temp);
#if DEBUG == 1
			lggr.print(F("S38=")); lggr.println(temp);
#endif
		}

		if((currentS = strstr(_commandBuffer, ("S90="))) != NULL)
		{
			offset =4;
			while(currentS[offset] != '\0'
				&& isDigit(currentS[offset]))
			{
				offset++;
			}

			memset(temp, 0, 100);
			strncpy(temp, currentS + 4, offset - 4);
			_isDcdInverted = atoi(temp);
#if DEBUG == 1
			lggr.print(F("S90=")); lggr.println(temp);
#endif
			print(F("SAVED S90=")); println(temp);
			EEPROM.write(S90_ADDRESS, atoi(temp));
		}

		if((currentS = strstr(_commandBuffer, ("S200="))) != NULL)
		{
			offset =5;
			while(currentS[offset] != '\0'
				&& isDigit(currentS[offset]))
			{
				offset++;
			}

			memset(temp, 0, 100);
			strncpy(temp, currentS + 5, offset - 5);
			_baudRate = atol(temp);
			setDefaultBaud(_baudRate);
#if DEBUG == 1
			lggr.print(F("S200=")); lggr.println(temp);
#endif
//			print(F("SAVED S200=")); println(temp);
//			EEPROM.write(S90_ADDRESS, atoi(temp));
		}

		for(int i=MAC_1 + 300; i<=USE_DHCP + 300; ++i)
		{
			char reg[6];
			sprintf(reg, "S%u=", i);

			if((currentS = strstr(_commandBuffer, reg)) != NULL)
			{
				int value = atoi(currentS + 5);
				EEPROM.write(i - 300, value);
				print(F("SAVED S")); print(i); print("="); println(value);
			}
		}

		for(int i=1; i<10; ++i)
		{
			char reg[6];
			sprintf(reg, ("S10%u="), i);
			if((currentS = strstr(_commandBuffer, reg)) != NULL)
			{
				int offset = 5;
				while(currentS[offset] != '\0')
				{
					offset++;
				}

				memset(temp, 0, 100);
				strncpy(temp, currentS + 5, offset - 5);
#if DEBUG == 1
				lggr.print(reg); lggr.println(temp);
#endif
				print(F("SAVED ")); print(reg); print('='); println(temp);
				writeAddressBook(ADDRESS_BOOK_START + (ADDRESS_BOOK_LENGTH * i), temp);
			}
		}
*/

		printOK();
	}
	else
	{
		printError();
	}

	strcpy(_lastCommandBuffer, _commandBuffer);
	resetCommandBuffer();
}

void ModemBase::ring()
{
    _isRinging = true;

	if(_S0_autoAnswer != 0) 
	{
		answer();
	}
	else
	{
		printResponse("2", F("RING"));

		digitalWrite(DCD, toggleCarrier(true));

		digitalWrite(RI, HIGH);
		delay(250);
		digitalWrite(RI, LOW);
	}
}

void ModemBase::connectOut()
{
	if(_verboseResponses)
	{
		_serial->print(F("CONNECT "));
	}

	if(_baudRate == 38400)
	{
		printResponse("28", F("38400"));
	}
	else if(_baudRate == 19200)
	{
		printResponse("15", F("19200"));
	}
	else if(_baudRate == 14400)
	{
		printResponse("13", F("14400"));
	}
	else if(_baudRate == 9600)
	{
		printResponse("12", F("9600"));
	}
	else if(_baudRate == 4800)
	{
		printResponse("11", F("4800"));
	}
	else if(_baudRate == 2400)
	{
		printResponse("10", F("2400"));
	}
	else if(_baudRate == 1200)
	{
		printResponse("5", F("1200"));
	}
	else
	{
		if(!_verboseResponses)    
			_serial->println('1');
		else
		{
			_serial->println(_baudRate);
		}
	}

	digitalWrite(DTR, toggleCarrier(true));

	_isConnected = true;
	_isCommandMode = false;
	_isRinging = false;
}

void ModemBase::processData()
{
	digitalWrite(RTS, LOW);
	//if(digitalRead(DCE_CTS) == HIGH) Serial.write("::DCE_CTS is high::");
	//if(digitalRead(DCE_CTS) == LOW) Serial.write("::DCE_CTS is low::");
	while(_serial->available())
	{
		if(_isCommandMode)
		{
			char inbound = toupper(_serial->read());

			if(_echoOn)  // && inbound != _S2_escapeCharacter
			{
				_serial->write(inbound);
			}

			if (inbound == S2_escapeCharacter)
			{
				_escapeCount++;
			}
			else
			{
				_escapeCount = 0;
			}

			if(_escapeCount == 3)
			{
				_escapeCount = 0;   // TODO, guard time!
				printOK();
			}

			if (inbound == S5_bsCharacter)
			{
				if(strlen(_commandBuffer) > 0)
				{
					_commandBuffer[strlen(_commandBuffer) - 1] = '\0';
				}
			}
			else if (inbound != '\r' && inbound != '\n' && inbound != S2_escapeCharacter)
			{
				_commandBuffer[strlen(_commandBuffer)] = inbound;

				if (_commandBuffer[0] == 'A' && _commandBuffer[1] == '/')
				{
					strcpy(_commandBuffer, _lastCommandBuffer);
					processCommandBuffer();
					resetCommandBuffer();  // To prevent A matching with A/ again
				}
			}
			else if(_commandBuffer[0] == 'A' && _commandBuffer[1] == 'T')
			{
				processCommandBuffer();
			}
			else
			{
				resetCommandBuffer();
			}
		}
		else
		{
			if(_isConnected)
			{
				char inbound = _serial->read();

				if(_echoOn) _serial->write(inbound);

				if (inbound == S2_escapeCharacter)    // TODO - refactor to get rid of duplication above
				{
					_escapeCount++;
				}
				else
				{
					_escapeCount = 0;
				}

				if(_escapeCount == 3)
				{
					_escapeCount = 0;
					_isCommandMode = true;   // TODO, guard time!

					printOK();
				}

				if (!_isCommandMode)
				{
					int result = _wifly->write(inbound);
				}
			}
		}
	}
	//digitalWrite(DCE_RTS, LOW);
}

void ModemBase::printOK(void)
{
	printResponse("0", F("OK"));
}

void ModemBase::printError(void)
{
	printResponse("4", F("ERROR"));
}


void ModemBase::printResponse(const char* code, const __FlashStringHelper * msg)
{
	_serial->println();

	if(!_verboseResponses)
		_serial->println(code);
	else
		_serial->println(msg);
}

void ModemBase::answer()
{
	if (!_isRinging)
	{
		disconnect();  // This prints "NO CARRIER"
		return;
	}

	_isConnected = true;
	_isCommandMode = false;
	_isRinging = false;

	if (_baudRate == 38400)
	{
		printResponse("28", F("CONNECT 38400"));
	}
	else if (_baudRate == 19200)
	{
		printResponse("14", F("CONNECT 19200"));
	}
	else if (_baudRate == 14400)
	{
		printResponse("13", F("CONNECT 14400"));
	}
	else if (_baudRate == 9600)
	{
		printResponse("12", F("CONNECT 9600"));
	}
	else if (_baudRate == 4800)
	{
		printResponse("11", F("CONNECT 4800"));
	}
	else if (_baudRate == 2400)
	{
		printResponse("10", F("CONNECT 2400"));
	}
	else if (_baudRate == 1200)
	{
		printResponse("5", F("CONNECT 1200"));
	}
	else
	{
		if (!_verboseResponses)
			_serial->println('1');
		else
		{
			_serial->print(F("CONNECT "));
			_serial->println(_baudRate);
		}
	}

	digitalWrite(RTS, LOW);

	TerminalMode();
}

// ----------------------------------------------------------

void ModemBase::ShowInfo()
{
	char mac[20];
	char ip[20];
	char ssid[20];

	_wifly->getMAC(mac, 20);
	_wifly->getIP(ip, 20);
	_wifly->getSSID(ssid, 20);

	_serial->print(F("MAC Address: "));  _serial->println(mac);
	_serial->print(F("IP Address:  "));  _serial->println(ip);
	_serial->print(F("Wi-Fi SSID:  "));  _serial->println(ssid);
}

void ModemBase::TerminalMode()
{
	while (_wifly->available() != -1) // -1 means closed
	{
		while (_wifly->available() > 0)
		{
			_serial->write(_wifly->read());
		}

		while (_serial->available() > 0)
		{
			_wifly->write(_serial->read());
		}

		// Alternate check for open/closed state
		if (!_wifly->isConnected())
		{
			break;
		}
	}
	disconnect();
}