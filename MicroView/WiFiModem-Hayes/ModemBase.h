/***********************************************
HAYESDUINO PROJECT - COPYRIGHT 2013, PAYTON BYRD

Project homepage: http://hayesduino.codeplex.com
License: http://hayesduino.codeplex.com/license
***********************************************/
#include "HardwareSerial.h"

#define __UNO__

#define S2_escapeCharacter '+'
#define S5_bsCharacter 8


#ifndef _MODEMBASE_h
#define _MODEMBASE_h

#include <Stream.h>
#include "Arduino.h"

#ifdef __UNO__
#define RTS  A5
#define DTR  A4
#define RI   A3
#define DCD  A2
#define CTS  A1
#define DSR  A0
#endif

class ModemBase : public Stream
{
 private:
	int _escapeCount;
	char _lastCommandBuffer[81];
	char _commandBuffer[81];

	Stream* _serial;
	WiFly * _wifly;

	uint32_t _baudRate;
	
	bool _isCommandMode;
	bool _isConnected;
	bool _isRinging;
	uint8_t _isDcdInverted;

	bool _echoOn;
	bool _verboseResponses;
	bool _quietMode;

	bool _S0_autoAnswer;			// Default false

	void resetCommandBuffer();

	void (*onDialout)(char*);
	void (*onDisconnect)();

	void printOK(void);
	void printError(void);
	void printResponse(const char* code, const __FlashStringHelper * msg);
	void loadDefaults();

 public:
	 ModemBase();

	 void begin(Stream*, WiFly*, void(*)(char*), void(*)());

  	 virtual int available(void);
     virtual int peek(void);
     virtual int read(void);
     virtual void flush(void);
     virtual size_t write(uint8_t);

	 void factoryReset(void);
	 uint32_t getBaudRate(void);
	 void setDefaultBaud(uint32_t baudRate);
	 void setDcdInverted(char isInverted);
	 bool getDcdInverted(void);
	 bool getIsConnected(void);
	 bool getIsRinging(void);
	 bool getIsCommandMode(void);
	 int toggleCarrier(boolean isHigh);
	 void disconnect();
	 void ring();
	 void connectOut(void);

	 void processCommandBuffer();
	 void processData();
	 
	 void resetToDefaults(void);

	 void ShowInfo();
	 void TerminalMode();
	 void answer();

	 using Print::write;
};

#endif

