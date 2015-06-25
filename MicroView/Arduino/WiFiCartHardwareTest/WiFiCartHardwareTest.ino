/*
     Commodore 64 - MicroVire - Wi-fi Cart
     Hardware Test
     Leif Bloomquist
*/

#include <MicroView.h>
#include <elapsedMillis.h>
#include <SoftwareSerial.h>

;  // Keep this here to pacify the Arduino pre-processor

#define RxD  6
#define RTS  A5
#define DTR  A4
#define RI   A3
#define DCD  A2
#define CTS  A1
#define DSR  A0
#define TxD  5

#define C64_BAUD 2400
#define WIFI_BAUD 2400

SoftwareSerial C64Serial(RxD, TxD);
HardwareSerial& WifiSerial = Serial;

void setup()
{
  uView.begin();	// begin of MicroView  
  uView.setFontType(0);
   
  C64Serial.begin(C64_BAUD);
  

}
  
void loop()
{
  input_tests();
}


  
void display(String message)
{
   clearScreen();
   uView.println(message);
   uView.display();
}

   
    
void clearScreen()
{
  uView.clear(ALL);	// erase hardware memory inside the OLED controller
  uView.clear(PAGE);	// erase the memory buffer, when next uView.display() is called, the OLED will be cleared.
  uView.display();
  uView.setCursor(0,0);  
}  


/* Diagnostic Tests for the Hardware */

void input_tests()
{
  pinMode(RxD, INPUT);
  pinMode(RTS, INPUT);
  pinMode(DTR, INPUT);
  pinMode(RI,  INPUT);
  pinMode(DCD, INPUT);
  pinMode(CTS, INPUT);
  pinMode(DSR, INPUT);
  pinMode(TxD, INPUT);
  
  while (1)
  {
    clearScreen();
    
    if (digitalRead(RxD) == HIGH) uView.print("RxD ");
    if (digitalRead(RTS) == HIGH) uView.print("RTS ");
    if (digitalRead(DTR) == HIGH) uView.print("DTR ");
    if (digitalRead(RI ) == HIGH) uView.print("RI  ");
    if (digitalRead(DCD) == HIGH) uView.print("DCD ");
    if (digitalRead(CTS) == HIGH) uView.print("CTS ");
    if (digitalRead(DSR) == HIGH) uView.print("DSR ");
    if (digitalRead(TxD) == HIGH) uView.print("TxD ");
    
    uView.display();
    delay(50);
  }  
}



void output_tests()
{
  pinMode(RxD, OUTPUT);
  pinMode(RTS, OUTPUT);
  pinMode(DTR, OUTPUT);
  pinMode(RI, OUTPUT);
  pinMode(DCD, OUTPUT);
  pinMode(CTS, OUTPUT);
  pinMode(DSR, OUTPUT);
  pinMode(TxD, OUTPUT);
  
  test_output("RxD", RxD);
  test_output("RTS", RTS);
  test_output("DTR", DTR);
  test_output("RI",  RI);
  test_output("DCD", DCD);
  test_output("CTS", CTS);
  test_output("DSR", DSR);
  test_output("TxD", TxD);
}

void test_output(String pinname, int pinnum)
{
  digitalWrite(pinnum, HIGH);
  display(pinname + " HIGH");
  delay(1000);
  
  digitalWrite(pinnum, LOW);
  display(pinname + " LOW");
  delay(1000);  
}
