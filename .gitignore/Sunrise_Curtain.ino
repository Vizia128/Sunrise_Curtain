#define BLYNK_PRINT Serial
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <TimeLib.h>
#include <TimeAlarms.h>
#include <WidgetRTC.h>
#include <Math.h>

#define PRIVATE 0    //used to determine which shade is being used
#define BLACKOUT 1   
#define ZERO_HALL 2   //the pin used to read the zero-point hall effect sensor
#define LOCATION_HALL 3    //the pin used to read the position hall effect sensor
#define LED_STRIP 4
#define PRI_OPEN 5   //the pin that outputs to open the privacy shade
#define PRI_CLOSE 6    //the pin that outputs to close the privacy shade
#define BLA_OPEN 7   //the pin that outputs to open the blackout shade
#define BLA_CLOSE 8    //the pin that outputs to close the blackout shade
#define PWM_FREQUENCY 128
#define PWM_RANGE 8192
#define SOFTSTART 8   //the larger the number, the longer it will take for the shade to get up to full speed
#define WAKE_DELAY 20   //how long in min it takes to complete the morning wake up routine

char auth[] = "YourAuthToken";    // You should get Auth Token in the Blynk App.
char ssid[] = "YourNetworkName";    // Your WiFi credentials.
char pass[] = "YourPassword";   // Go to the Project Settings (nut icon).

BlynkTimer timer;
WidgetRTC rtc;



int location = 1;   //this represents the current location of the shade


void GoTo(bool, int);
void WakeUp(void);
void ZeroShade(bool);
void OpenShade(bool, int);
void CloseShade(bool, int);
void StopShade(bool);
void LEDstrip(int);

BLYNK_WRITE(V0){
  GoTo(PRIVATE, param.asInt());
}
BLYNK_WRITE(V1){
  GoTo(BLACKOUT, param.asInt());
}
BLYNK_WRITE(V3){
  TimeInputParam t(param);
  void free(AlarmID_t ID); 

  if(t.getStartMinute() < WAKE_DELAY){
    AlarmID_t morningAlarm = Alarm.alarmRepeat(t.getStartHour(), t.getStartMinute() - WAKE_DELAY, 0, WakeUp);
  }
  else{
    AlarmID_t morningAlarm = Alarm.alarmRepeat(t.getStartHour() - 1, t.getStartMinute() + 60 - WAKE_DELAY, 0, WakeUp);
  }  
}
BLYNK_WRITE(V4){
  LEDstrip(param.asInt());   //up to 32
}

BLYNK_CONNECTED() {
  // Synchronize time on connection
  rtc.begin();
}

void setup() {
  analogWriteFreq(PWM_FREQUENCY);   //changes the PWM frequency
  analogWriteRange(PWM_RANGE);
  setSyncInterval(10 * 60);   //updates internal clock every 10 min
  
  pinMode(ZERO_HALL, INPUT);
  pinMode(LOCATION_HALL, INPUT);
  pinMode(PRI_OPEN, OUTPUT);
  pinMode(PRI_CLOSE, OUTPUT);
  pinMode(BLA_OPEN, OUTPUT);
  pinMode(BLA_CLOSE, OUTPUT);
  
  Blynk.begin(auth, ssid, pass);
}

void loop() {
  Blynk.run();
  timer.run();
}



void GoTo(bool curtainType, int destination) {
  
  boolean countFlag = LOW;   //we use this so the program can remember the state of the hall sensor

  if(destination == 0){   //destiantion zero is all the way open
  ZeroShade(curtainType);   //Opens the shade all the way and recalibrates the zero point
  }

  if(destination > location){   //we are closing the shade
     for(int i = 0; location != destination; i++){    //loops until the shade has reached its destination
        CloseShade(curtainType, (i * i) / SOFTSTART + 1);   
        // "(i * i) creates a sloping start, "SOFTSTART" controls how quickly it gets up to speed, "+1" makes sure it always starts moving immediately
        if (digitalRead(LOCATION_HALL) == HIGH) {
          countFlag = HIGH;
        }
        if (digitalRead(LOCATION_HALL) == LOW && countFlag == HIGH) {
        location++;
          countFlag=LOW;
        }
        //adds one to the shade's location when the hall sensor falls from high to low
      }
  }

  else if(destination < location){    //we are opening the shade
     for(int i = 0; location != destination; i++){
        OpenShade(curtainType, (i * i) / SOFTSTART + 1);
        if (digitalRead(LOCATION_HALL) == HIGH) {
          countFlag = HIGH;
        }
        if (digitalRead(LOCATION_HALL) == LOW && countFlag == HIGH) {
        location--;
          countFlag=LOW;
        }
        //subtracts one from the shade's location when the hall sensor falls from high to low
      }
  }
}

void WakeUp(){
  int period = WAKE_DELAY * 60 / sqrt(PWM_RANGE) * 900;
  for(int i = 1; i * i < PWM_RANGE; i++){
    analogWrite(LED_STRIP, i * i);
    Alarm.delay(period);
  }
  digitalWrite(LED_STRIP, HIGH);
}

void ZeroShade(bool curtainType){
  for(int i = 0; ; i++){    //this will allow the shade to have a soft start
    OpenShade(curtainType, (i * i) / SOFTSTART + 1);    //using a quadratic start is much more smooth than a linear one
    if(analogRead(ZERO_HALL) == LOW){    //detects when the zero Hall sensor is triggered
      StopShade(curtainType);   
      location = 0;     //after the zeroHall is triggered the shade stops and its location is reset to zero
      break;
    }
    delay(10);    //for stability
  }
}


void OpenShade(bool curtainType, int power){
  switch(curtainType){    //selects which curtain we want
    case PRIVATE:   //selects private curtain
      digitalWrite(PRI_CLOSE, LOW);   //makes sure this pin is off so the H-bridge doesn't short
      if(power >= PWM_RANGE){   //if we are at maximum power, we don't need pwm. Also makes code above simpler
        digitalWrite(PRI_OPEN, HIGH);   //opens private shade at max power
      }
      else{
        analogWrite(PRI_OPEN, power);   //opens private shade with "(power / 1024)%" amount of power
      }
    case BLACKOUT:    //selects blackout curtain
      digitalWrite(BLA_CLOSE, LOW);
      if(power >= PWM_RANGE){
        digitalWrite(BLA_OPEN, HIGH);
      }
      else{
        analogWrite(BLA_OPEN, power);
      }
  }
}

void CloseShade(bool curtainType, int power){
  switch(curtainType){    //selects which curtain we want
    case PRIVATE:   //selects private curtain
      digitalWrite(PRI_OPEN, LOW);   //makes sure this pin is off so the H-bridge doesn't short
      if(power >= PWM_RANGE){   //if we are at maximum power, we don't need pwm. Also makes code above simpler
        digitalWrite(PRI_CLOSE, HIGH);   //closes private shade at max power
      }
      else{
        analogWrite(PRI_CLOSE, power);   //closes private shade with "(power / 1024)%" amount of power
      }
    case BLACKOUT:    //selects blackout curtain
      digitalWrite(BLA_OPEN, LOW);
      if(power >= PWM_RANGE){
        digitalWrite(BLA_CLOSE, HIGH);
      }
      else{
        analogWrite(BLA_CLOSE, power);
      }
  }
}

void StopShade(bool curtainType){   //stops the specified shade
  switch(curtainType){
    case PRIVATE:
      digitalWrite(PRI_CLOSE, LOW);
      digitalWrite(PRI_OPEN, LOW);
      break;
    case BLACKOUT:
      digitalWrite(BLA_CLOSE, LOW);
      digitalWrite(BLA_OPEN, LOW);
      break;
  }
}

void LEDstrip(int i){
   if(i * i >= PWM_RANGE){
     digitalWrite(LED_STRIP, HIGH);
   }
   else{
     analogWrite(LED_STRIP, i * i);
   }
}


