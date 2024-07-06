// ******************************************************************************************************
// 
//         Ingos Sonne
//  ==========================
//       Uwe Berger, 2024
//
//  
// SIM800L:
//  https://raspberry-pi.fr/download/SIM800%20Series_AT%20Command%20Manual_V1.09.pdf
//  https://www.makershop.de/download/Datasheet_SIM800L.pdf
//  https://www.makershop.de/download/SIM800L-GSM-Pinout.jpg
//
// ToDo:
// -----
//  * Reset SIM800L-Modul bei Setup sinnvoll/notwendig?
//  * Sleep-Mode für SIM800L?
//
//
//
//  ---------
//  Have fun!
//
// ******************************************************************************************************

#include <Arduino.h>
#include <avr/sleep.h>
#include <SoftwareSerial.h>
#include <Adafruit_NeoPixel.h>

#define SIM_PIN                 "4321"  // SIM-PIN (default)
#define TIMEOUT_WAIT_FOR_REG    30000   // 30s; Timeout fuer einen Registrierungszyklus im Handy-Netz

// LED-Animation bei welchen Ereignis?
#define CALL_LED_STRIP  	      1       // eingehender Anruf
#define SMS_LED_STRIP   	      1       // eingehende SMS
#define ALWAYS_LED_STRIP	      0       // immer, wenn RING-Pin auf Low geht

// Bei Anruf/SMS eine Antwort via SMS senden?
#define SMS_REPLY_PIN		        5 	    // wenn GPIO-Pin auf Low, dann ja

// Wakeup-Pin MCU <-- Ring-Pin of SIM800L
#define WAKEUP_MCU_PIN	        2       // GPIO-Pin MCU

// GPIOs fuer serielle Schnittstelle zu SIM800L
#define SIM800L_TX		          4
#define SIM800L_RX		          3

SoftwareSerial sim800l(SIM800L_RX, SIM800L_TX);

// LED-Strip (WS2812)
#define LED_PIN			            6       // GPIO-Pin
#define NUM_LEDS		            14      // Anzahl LEDs im Strip
#define TIME_LED_ANIMATION      10000   // 10s; Dauer Animation

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Gruende, warum RING-Pin feuert
enum WakeUpTyp {WAKEUP_CALL, WAKEUP_SMS, WAKEUP_UNKNOWN};


// ******************************************************************
void sendATCommand(String command, unsigned int wait_ms) {
  sim800l.println(command);
  delay(wait_ms);
  while (sim800l.available()) {
    Serial.write(sim800l.read());
  }
  Serial.println("");
}

// ******************************************************************
void sendSms(String text, String number) {

  Serial.println("Send sms to "+number+"; text: "+text);
  sendATCommand("AT+CMGF=1", 100);                  // SMS text mode
  sendATCommand("AT+CMGS=\""+number+"\"", 100);     // an welche Telefonnummer
  sendATCommand(text, 100);                         // SMS-Text
  sim800l.write(26);                                // CTRL + Z
  sendATCommand("", 100);
}

// ******************************************************************
boolean isRegistered() {
  // AT+COPS?
  // ok  --> +COPS: 0,0,"E-Plus"
  // nok --> +COPS: 0
  sim800l.println("AT+COPS?");
  if (sim800l.readString().indexOf("+COPS: 0,0,\"") == -1) {
    return false;    
  } else {
    return true;
  }
}

// ******************************************************************
boolean isSimPinRequired() {
  // AT+CPIN?
  // ja   --> +CPIN: SIM PIN
  // nein --> +CPIN: READY
  sim800l.println("AT+CPIN?");
  if (sim800l.readString().indexOf("+CPIN: SIM PIN") != -1) {
    return true;    
  } else {
    return false;
  }
}

// ******************************************************************
WakeUpTyp wakeUpReason() {
  // Call: 
  //   AT+CLIP=1
  //     RING    
  //     +CLIP: "+491234567890",145,"",0,"",0
  // SMS:  +CMTI: "ME",15
  // SMS:
  //   AT+CMGF=1
  //   AT+CNMI=1,2
  //     +CMT: "+491234567890","","24/06/21,08:27:22+08"
  //
  String answer = sim800l.readString();
  String phone_number = "";
  WakeUpTyp ret = WAKEUP_UNKNOWN;
  int idx1, idx2;
  //~ Serial.println("wakeUpReason: >>>"+answer+"<<<");
  // "normaler" Anruf
  if (answer.indexOf("RING") != -1) {
    ret = WAKEUP_CALL;
    // Anrufer-Nummer ermitteln
    idx1 = answer.indexOf("\"");
    idx2 = answer.indexOf("\"", idx1+1);
    phone_number = answer.substring(idx1+1, idx2);
  }
  // ohne Anzeige SMS-Inhalt
  if (answer.indexOf("+CMTI: \"ME\",") != -1) {
    ret = WAKEUP_SMS;
  }
  // mit Anzeige SMS-Inhalt
  if (answer.indexOf("+CMT: \"") != -1) {
    ret = WAKEUP_SMS;
    // Anrufer-Nummer ermitteln
    idx1 = answer.indexOf("\"");
    idx2 = answer.indexOf("\"", idx1+1);
    phone_number = answer.substring(idx1+1, idx2);
  }
  // eventuell SMS-Antwort senden
  Serial.println("caller: >"+phone_number+"<");
  if ((digitalRead(SMS_REPLY_PIN) == LOW) && (phone_number.length() > 0)) {
    sendSms("leds are under fire :-)", phone_number);
  }
  return ret;
}

// ******************************************************************
void registerViaSimPin() {
  long timeout = millis();
  Serial.println("Waiting to register sim card...");
  if (isSimPinRequired()) sendATCommand("AT+CPIN=\"" + String(SIM_PIN) + "\"", 250);
  do {
    if ((timeout + TIMEOUT_WAIT_FOR_REG) < millis()) {
      Serial.println("x");
      delay(5000);
      timeout = millis();
      // wenn SIM-PIN erfoderlich, dann PIN-Eingabe
      if (isSimPinRequired()) sendATCommand("AT+CPIN=\"" + String(SIM_PIN) + "\"", 250);
    }
    Serial.print(".");
  } while (!isRegistered());
  Serial.println("");
  Serial.println("...sim card is registered!");
}

// ************************************************************************
void displayRainbowAnimation(unsigned long duration) {
  unsigned long start = millis();
  while (millis() - start < duration) {
    rainbowCycle(20); // Adjust the speed as needed
  }
  strip.clear();
  strip.show();
}

// ************************************************************************
void rainbowCycle(int wait) {
  uint16_t i, j;
  for (j = 0; j < 256; j++) { // 1 cycle of all 256 colors in the wheel
    for (i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

// ************************************************************************
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

// ************************************************************************
void goToSleep() {
  sleep_enable();
  attachInterrupt(0, wakeUp, LOW);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);
  sleep_cpu();

  // Programm laeuft an dieser Stelle weiter, wenn MCU aus Sleep kommt
  Serial.println("Continue in the program flow...");
}

// ************************************************************************
void wakeUp() {
  Serial.println("");
  Serial.println("MCU wakeup...");
  sleep_disable();
  detachInterrupt(0);
}

// ************************************************************************
void doTask() {
  static boolean animation = false;
  WakeUpTyp wakeup_reason = wakeUpReason(); // Ursache des Interrupts ermitteln (entsprechend reagieren und ggf. Anwort-SMS senden)
  digitalWrite(LED_BUILTIN, HIGH);
  // je nach Ursache des Interrupts verzweigen (Netz verloren oder Anruf/SMS)
  if (!isRegistered()) {
    Serial.println("Net lost, try to register...");
    delay(5000);
    registerViaSimPin();
  } else {
    if (animation && ((CALL_LED_STRIP && (wakeup_reason == WAKEUP_CALL)) || (SMS_LED_STRIP && (wakeup_reason == WAKEUP_SMS)) || ALWAYS_LED_STRIP)) {
	  // Wenn es ein "normaler" Anruf war, dann Annehmen/Auflegen
	  if (wakeup_reason == WAKEUP_CALL) {
		  sendATCommand("ATA", 300);        // Anruf annehmen
		  sendATCommand("ATH", 250);        // Auflegen
	  } 
      Serial.println("Animate led-strip...");
      displayRainbowAnimation(TIME_LED_ANIMATION);
    }
  }
  animation = true;
  Serial.println("Going to sleep...");
  goToSleep();
}

// ************************************************************************
void setup() {

  Serial.begin(9600);
  Serial.println("");
  Serial.println("");
  Serial.println("Start...");
  
  Serial.println("...init serial to sim800l");
  sim800l.begin(9600);
  delay(1000);
  
  // Konfiguration WAKEUP_MCU_PIN als "Aufweck"-Input
  pinMode(WAKEUP_MCU_PIN, INPUT_PULLUP);
  digitalPinToInterrupt(WAKEUP_MCU_PIN);

  // Konfiguration modulinterne LED zur Anzeige diverser Ereignisse
  pinMode(LED_BUILTIN, OUTPUT);

  // Konfiguration SMS_REPLY_PIN als Input (wenn Jumper Low, dann SMS senden...)
  pinMode(SMS_REPLY_PIN, INPUT_PULLUP);
  
  // Initialisierung LED-Strip
  Serial.println("...init LED strip");
  strip.begin();
  strip.clear();
  strip.show();

  // Initialisierung SIM800L-Modul
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("...init SIM800L");
  registerViaSimPin();                // SIM-PIN notwendig/tun; warten bis im Netz registriert
  sendATCommand("AT+CLIP=1", 250);    // Ausgabe anrufende Nummer bei Call einschalten
  sendATCommand("AT+CMGF=1", 250);    // Textmode einschalten
  sendATCommand("AT+CNMI=1,2", 250);  // Anzeige SMS bei Eingang einschalten; SMS wird nicht gespeichert  
  sendATCommand("AT+CFGRI=1", 250);   // Aktivierung RING-Indikator (Pin)
  //~ sendATCommand("AT+CSCLK=1", 250);   // sleep
  digitalWrite(LED_BUILTIN, LOW);
}

// ************************************************************************
// ************************************************************************
// ************************************************************************
void loop() {
  doTask();
}
