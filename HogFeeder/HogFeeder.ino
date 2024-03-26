#define FONA_RST 4
#include <Wire.h>
#include <TimeLib.h>
#include <Time.h>
#include <TimeAlarms.h>
#include <SoftwareSerial.h>
#include <AccelStepper.h>
#include <neotimer.h>
#include "HUSKYLENS.h" 
#include "ParserLib.h"
#include "Adafruit_FONA.h"

#define FONA_RX 11
#define FONA_TX 10

char replybuffer[255];
char callerIDbuffer[32] = "+639066672304";
long augerTime = 20000;

AccelStepper stepper(AccelStepper::FULL4WIRE, 4, 5, 6, 7);

SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial *fonaSerial = &fonaSS;

Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout = 0);

char smsBuffer[250];
char fonaNotificationBuffer[64];
Neotimer stepperTimer = Neotimer(augerTime); 
Neotimer hourTimer = Neotimer(1000); 

HUSKYLENS huskylens;
void printResult(HUSKYLENSResult result);

boolean isClean = true;

void setup() {
  while (!Serial);
  Serial.begin(115200);
  Wire.begin();
  while (!huskylens.begin(Wire))
  {
      Serial.println(F("Begin failed!"));
      Serial.println(F("1.Please recheck the \"Protocol Type\" in HUSKYLENS (General Settings>>Protocol Type>>I2C)"));
      Serial.println(F("2.Please recheck the connection."));
      delay(100);
  }
  stepper.setMaxSpeed(5000);
  stepper.setSpeed(4000);  

  //Serial.println(F("FONA SMS caller ID test"));
  //Serial.println(F("Initializing....(May take 3 seconds)"));
  // make it slow so its easy to read!
  fonaSerial->begin(4800);
  if (! fona.begin(*fonaSerial)) {
    Serial.println(F("Couldn't find FONA"));
    while(1);
  }
  Serial.println(F("FONA is OK"));

  // Print SIM card IMEI number.
  char imei[16] = {0}; // MUST use a 16 character buffer for IMEI!
  uint8_t imeiLen = fona.getIMEI(imei);
  if (imeiLen > 0) {
    Serial.print("SIM card IMEI: "); Serial.println(imei);
  }

  fonaSerial->print("AT+CNMI=2,1\r\n");

  Serial.println("FONA Ready");  


  char timeBuffer[23];
  while(!fona.getTime(timeBuffer, 23));
  Serial.print(F("Time = ")); Serial.println(timeBuffer);
  setTime(((timeBuffer[10] - 48) * 10) + (timeBuffer[11] - 48),
          ((timeBuffer[13] - 48) * 10) + (timeBuffer[14] - 48),
          ((timeBuffer[16] - 48) * 10) + (timeBuffer[17] - 48),
          ((timeBuffer[7] - 48) * 10) + (timeBuffer[8] - 48),
          ((timeBuffer[4] - 48) * 10) + (timeBuffer[5] - 48),
          ((timeBuffer[1] - 48) * 10) + (timeBuffer[2] - 48)
         );

  hourTimer.start();
 // Alarm.alarmRepeat(14,59,00, AlarmSetOne);
 // Alarm.alarmRepeat(14,57,00, AlarmSetTwo);
 // Alarm.alarmRepeat(14,58,00, AlarmSetThree);



    fona.deleteSMS(1);
    fona.deleteSMS(2);
    fona.deleteSMS(3);
}

void loop() {
  stepper.setMaxSpeed(5000);
  stepper.setSpeed(4000);  
  char* bufPtr = fonaNotificationBuffer;    //handy buffer pointer
  if (fona.available())      //any data available from the FONA?
  {
    int slot = 0;            //this will be the slot number of the SMS
    int charCount = 0;
    //Read the notification into fonaInBuffer
    do  {
      *bufPtr = fona.read();
      Serial.write(*bufPtr);
      delay(1);
    } while ((*bufPtr++ != '\n') && (fona.available()) && (++charCount < (sizeof(fonaNotificationBuffer)-1)));
    
    //Add a terminal NULL to the notification string
    *bufPtr = 0;

    //Scan the notification string for an SMS received notification.
    //  If it's an SMS message, we'll get the slot number in 'slot'
    if (1 == sscanf(fonaNotificationBuffer, "+CMTI: " FONA_PREF_SMS_STORAGE ",%d", &slot)) {
      Serial.print("slot: "); Serial.println(slot);
      
      //char callerIDbuffer[32];  //we'll store the SMS sender number in here
      
      // Retrieve SMS sender address/phone number.
      if (! fona.getSMSSender(slot, callerIDbuffer, 31)) {
        Serial.println("Didn't find SMS message in slot!");
      }
      Serial.print(F("FROM: ")); Serial.println(callerIDbuffer);

        // Retrieve SMS value.
        uint16_t smslen;
        if (fona.readSMS(slot, smsBuffer, 250, &smslen)) { // pass in buffer and max len!
          //Serial.println(smsBuffer);
          AlarmSetup(smsBuffer);
        }
      //Send back an automatic response
      Serial.println("Sending reponse...");
      /*
      if (!fona.sendSMS(callerIDbuffer, (char *)"Hog Feeder Powered On")) {
        Serial.println(F("Failed"));
      } else {
        Serial.println(F("Sent!"));
      }
      */
      if (fona.deleteSMS(slot)) {
        Serial.println(F("OK!"));
      } else {
        Serial.print(F("Couldn't delete SMS in slot ")); Serial.println(slot);
        fona.print(F("AT+CMGD=?\r\n"));
      }
    }
  }
  
  if(hourTimer.done()){
    //Serial.print(hourFormat12());
   // printDigits(minute());
   // printDigits(second());
    //Serial.print(" ");
    //Serial.println(dayStr(weekday()));
    //Serial.print(monthStr(month()));  
    //Serial.print("  ");
    //Serial.print(day());
    //Serial.print("  ");
    //Serial.print(year());
    //Serial.print("  ");
    Alarm.delay(0); // wait one second between clock display

    if (!huskylens.request()){}
    else if(!huskylens.isLearned()){} 
    else if(!huskylens.available()){}
    if (huskylens.available()){
            HUSKYLENSResult result = huskylens.read();
            //printResult(result);

            if (result.ID){
              //Serial.println(": Plate Not Clean");
              isClean = false;
            }  
   }
   else {
      isClean = true;
      //Serial.println(": Plate Clean");
   }
    hourTimer.start();
  }
  
  if(stepperTimer.waiting()){
      //Serial.println("stepperTimer.waiting()");  
      while(stepper.runSpeed());
  }

  if(stepperTimer.done()){
      stepper.stop();
  }
}

void delsms(){
  for (int i=0; i<10; i++){
      int pos=fona.deleteSMS(0);
      if (pos!=0){ 
        if (fona.deleteSMS(pos)==1){}
        else {}
      }
  } 
}

void AlarmSetup(char timeBuffer[]){
  Serial.print("timeBUffer: "); Serial.println(timeBuffer);
  int itemLength = strlen(timeBuffer);
  Parser parser((byte*)timeBuffer, itemLength);

  int i          = parser.Read_Int32(); 
                   parser.Skip(1);
  int hourTime   = parser.Read_Int16();
                   parser.Skip(1);
  int minuteTime = parser.Read_Int32();
                   parser.Skip(1);
  int secondTime = parser.Read_Int32();
                   parser.Skip(1);

  Serial.print("i: "); Serial.println(i);
  Serial.print("hourTime: "); Serial.println(hourTime);
  Serial.print("minuteTime: "); Serial.println(minuteTime);
  Serial.print("secondTime: "); Serial.println(secondTime);
                   
  switch (i){
    case 1:  Alarm.alarmRepeat(hourTime,minuteTime,secondTime, AlarmSetOne);  break;                
    case 2:  Alarm.alarmRepeat(hourTime,minuteTime,secondTime, AlarmSetTwo); break; 
    case 3:  Alarm.alarmRepeat(hourTime,minuteTime,secondTime, AlarmSetThree); break; 
    case 4:  Alarm.alarmRepeat(hourTime,minuteTime,secondTime, AlarmSetFour);  break;
    //5:20000:0:0 format to change augerTime
    case 5:  augerTime = hourTime;
    augerTime = augerTime + 5;
    augerTime = augerTime * 1000;  
    //stepperTimer = Neotimer(augerTime); 
    stepperTimer.set(augerTime);
    //Serial.print("augerTime: "); Serial.println(augerTime);
    fona.sendSMS(callerIDbuffer, (char *)"New Set Time");
    break;
    default: break;
  }
  parser.Reset();
}
void AlarmSetOne(){
  Serial.println("AlarmSetOne");  
  augerSpin(10000);
}

void AlarmSetTwo(){ 
  Serial.println("AlarmSetTwo");  
  augerSpin(10000);
}

void AlarmSetThree(){ 
  Serial.println("AlarmSetThree");
  augerSpin(10000);
}

void AlarmSetFour(){ 
  Serial.println("AlarmSetFour"); 
  augerSpin(10000);
  Serial.println("Plate Not Clean") ;         
}

void augerSpin(int rotate){
    if (isClean){
      stepperTimer.start();
      fona.sendSMS(callerIDbuffer, (char *)"Hog Feeder Dispensing");
    }
    else {
      fona.sendSMS(callerIDbuffer, (char *)"Plate is not clean");
    }
}

void printDigits(int digits){
  Serial.print(":");
  if(digits < 10)
  {
    Serial.print('0');
  }  
  Serial.print(digits);
}
