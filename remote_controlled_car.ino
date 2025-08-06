#include <PS4Controller.h>
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_err.h"
#include "FastLED.h"
#include <ESP32Servo.h>
#include <DFRobot_DF1201S.h>
#include <HardwareSerial.h>

int speedCurrent = 0, speedCatchup = 0, cycleTrack = 0, prevSpeed = 0, gearShift = 0, shiftComplete = 0, triggered = 0;
int trackL2 = 0, trackR2 = 0, tempCurTime = 0, tempTotTime = 0, tempDiff = 0;
int speedCuCircle = 0, speedCaCircle = 0;
int actualSpeed = 0;
int lightTurn = 0, onlyOne = 0;
int frontHead[3] = {220, 247, 241}, rearBrakeReg[3] = {31, 0, 0}, rearBrakeAct[3] = {255, 0, 0}, hazard[3] = {255, 100, 0};
int accspeed = 1, deaccspeed = 1;
int mp3CurFile = 0;
int LstickX = 0, mappedLstickX = 0, trTrack = 0, sqTrack = 0, ciTrack = 0, crTrack = 0, rBumpTrack = 0, lBumpTrack = 0;
unsigned long lastTimeStamp = 0, LTSbuttons = 0, hazardTS = 0;
bool tr = PS4.Triangle(), sq = PS4.Square(), ci = PS4.Circle(), cr = PS4.Cross(), rt = PS4.R2(), lt = PS4.L2();
bool rBump = PS4.R1(), lBump = PS4.L1();

HardwareSerial DFP(2);
DFRobot_DF1201S DF;

Servo steering, motor;

#define LED_PIN D3
#define NUM_LEDS 4
#define EVENTS 0
#define BUTTONS 1
#define JOYSTICKS 1
#define SENSORS 0

CRGB leds[NUM_LEDS];

void setup(){
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(96);
  brakeReg();
  Serial.begin(115200);
  DFP.begin(115200, SERIAL_8N1, D11, D10);
  if (!DF.begin(DFP)) { // If the object did not initialize, then its configuration is invalid
    Serial.println("Invalid EspSoftwareSerial pin configuration, check config"); 
    while (1) { // Don't continue with invalid configuration
      delay (1000);
    }
  }
  DF.setVol(0);
  DF.switchFunction(DF.MUSIC);
  delay(2000);
  DF.setPlayMode(DF.SINGLE);
  DF.setVol(15);
  DF.playFileNum(1);
  mp3CurFile = millis();
  PS4.attach(stateCheck);
  PS4.attachOnConnect(onConnect);
  PS4.attachOnDisconnect(onDisConnect);
  PS4.begin();
  Serial.println("");
  steering.attach(D12);
  motor.attach(D13, 1000, 2000);
  steering.write(90);

  /*Serial.printf("Throttle range setup!!!\n");
  Serial.printf("Neutral!\n");
  motor.writeMicroseconds(1500);
  delay(3000);
  Serial.printf("Forward!\n");
  motor.writeMicroseconds(2000);
  delay(3000);
  Serial.printf("Reverse!\n");
  motor.writeMicroseconds(1000);
  delay(3000);
  Serial.printf("Done!\n");*/
}

void onConnect(){
  Serial.println("Connected!");
}

void onDisConnect(){
  Serial.println("Disconnected!");
}

void steer(){
  LstickX = PS4.LStickX();
  mappedLstickX = map(LstickX, -127, 127, 0, 165);
  if(mappedLstickX >= 85 && mappedLstickX <= 95){
    steering.write(90);
  }else{
    steering.write(mappedLstickX);
  }
}

void accelspeed(){
  if(speedCurrent == 0){
    accspeed = 1;
  }else{
    if(PS4.L2()){
      accspeed = 1;
      //Serial.printf("accspeed for reverse is only 1\n");
    }else{
      if(speedCurrent >= 1 && speedCurrent <= 85){
        accspeed = 1;
        //Serial.printf("accspeed is 1\n");
      }
      else if (speedCurrent >= 86 && speedCurrent <= 170){
        accspeed = 2;
        //Serial.printf("accspeed is 2\n");
      }
      else if (speedCurrent >= 171 && speedCurrent <= 255){
        accspeed = 4;
        //Serial.printf("accspeed is 4\n");
      }
    }
  }
}

int accdeacc(int current, int catchup){
  if(PS4.L2() || catchup < 0){
    if (current == catchup){
      return catchup;
    }else if(current > catchup){
      catchup += deaccspeed;
    }else if (current < catchup){
      catchup -= accspeed;
    }
    if (catchup < -96 && PS4.L2()){
      catchup = -96;
    }
    if (catchup > 0 && !PS4.L2() && !PS4.R2()){
      catchup = 0;
    }
  }else if(PS4.R2() || catchup > 0){
    if (current == catchup){
      return catchup;
    }else if(current > catchup){
      catchup += accspeed;
    }else if (current < catchup){
      catchup -= deaccspeed;
    }
    if (catchup > 144 && PS4.R2()){
      catchup = 144;
    }
    if (catchup < 0 && !PS4.L2() && !PS4.R2()){
      catchup = 0;
    }
  }
  return catchup;
}

void drive(){
  if (millis() - lastTimeStamp > 25){
    if(PS4.Cross()){
      speedCurrent = 0;
    }else{
      if(PS4.R2() && speedCatchup >= 0){
        speedCurrent = PS4.R2Value();
      }else if(PS4.L2() && speedCatchup <= 0){
        speedCurrent = PS4.L2Value()*(-1);
      }else if(!PS4.R2() || !PS4.L2()){
        speedCurrent = PS4.R2Value();
      }
    }
    accelspeed();
    if (!ci){
      speedCatchup = accdeacc(speedCurrent, speedCatchup);
      //Map to correct values; 1.0 full reverse, 1.5 neutral, 2.0 full forward
      actualSpeed = map(speedCatchup, -255, 255, 1000, 2000);
    }else{
      //Alternative when circle is pressed
      speedCatchup = accdeacc(speedCurrent, speedCatchup);
      speedCaCircle = accdeacc(speedCuCircle, speedCaCircle);
      //Map to correct values; 1.0 full reverse, 1.5 neutral, 2.0 full forward
      actualSpeed = map(speedCaCircle, -255, 255, 1000, 2000);
    }
    //Send impulse to DC motor 
    motor.writeMicroseconds(actualSpeed);
    cycleTrack++;
    if(cycleTrack >= 9){
      cycleTrack = 0;
      prevSpeed = speedCatchup;
    }
    lastTimeStamp = millis();
  }
}

void sound(){
  if((millis() - mp3CurFile >= DF.getTotalTime()*995-tempDiff*995) || (PS4.R2() && trackR2 == 0 && prevSpeed == 0) || (PS4.R2() && trackR2 == 0) || (!PS4.R2() && trackR2 == 1) || (PS4.L2() && trackL2 == 0 && prevSpeed == 0) || (PS4.L2() && trackL2 == 0) || (!PS4.L2() && trackL2 == 1)){
    Serial.printf("Entry reason:\t");
    if(PS4.R2() && trackR2 == 0 && prevSpeed == 0){
      Serial.printf("Sudden acceleration from idle!");
      triggered = 3;
    }else if(PS4.R2() && trackR2 == 0){
      Serial.printf("Sudden acceleration!");
      if(shiftComplete == 0){
        Serial.printf("\tTriggered 4");
        triggered = 4;
      }else{
        Serial.printf("\tTriggered 5");
        triggered = 5;
      }
    }else if(!PS4.R2() && trackR2 == 1){
      Serial.printf("Sudden deacceleration!");
      if(shiftComplete == 1){
        Serial.printf("\tTriggered 6");
        triggered = 6;
      }else{
        Serial.printf("\tTriggered 7");
        triggered = 7;
      }
    //This part is for reverse sound logic
    }else if(PS4.L2() && trackL2 == 0 && prevSpeed == 0){
      Serial.printf("Sudden reverse acceleration from idle!");
      triggered = 8;
    }else if(PS4.L2() && trackL2 == 0){
      Serial.printf("Sudden reverse acceleration!");
      triggered = 9;
    }else if(!PS4.L2() && trackL2 == 1){
      Serial.printf("Sudden reverse deacceleration!");
      triggered = 10;
    }else{
      Serial.printf("Normal entry!");
      triggered = 0;
    }
    Serial.printf("\n");
    
    if(speedCatchup >= -16 && speedCatchup <= 16 && !PS4.R2() && !PS4.L2()){
      Serial.printf("Idle\tSpeedCatchup = %d\n", speedCatchup);
      DF.playFileNum(2);
      tempDiff = 0;
      gearShift = 0;
      shiftComplete = 0;
    }
    else if((speedCatchup > 0 && prevSpeed < speedCatchup && gearShift == 0) || triggered == 3 || triggered == 4){
      tempCurTime = DF.getCurTime();
      tempTotTime = DF.getTotalTime();
      if(tempCurTime < tempTotTime && shiftComplete == 0 && DF.getCurFileNumber() != 2){
        Serial.printf("Accel gear 1 w/ FF!\n");
        DF.playFileNum(3);
        DF.fastForward(tempTotTime - tempCurTime);
        tempDiff = tempTotTime - tempCurTime;
      }else{
        Serial.printf("Accel gear 1\n");
        DF.playFileNum(3);
        tempDiff = 0;
      }
      gearShift = 1;
    }
    else if((speedCatchup > 0 && prevSpeed <= speedCatchup && gearShift == 1 && triggered != 6) || triggered == 5){
      tempCurTime = DF.getCurTime();
      tempTotTime = DF.getTotalTime();
      if(tempCurTime < tempTotTime && shiftComplete == 1){
        Serial.printf("Accel gear 2 w/ FF!\n");
        DF.playFileNum(4);
        DF.fastForward(tempTotTime - tempCurTime);
        tempDiff = tempTotTime - tempCurTime;
      }else{
        Serial.printf("Accel gear 2\n");
        DF.playFileNum(4);
        tempDiff = 0;
      }
      shiftComplete = 1;
    }                                                     
    else if((speedCatchup > 0 && prevSpeed >= speedCatchup && gearShift == 1 && shiftComplete == 1) || triggered == 6){
      tempCurTime = DF.getCurTime();
      tempTotTime = DF.getTotalTime();
      if(tempCurTime < tempTotTime){
        Serial.printf("Deaccel gear 2 w/ FF!\n");
        DF.playFileNum(5);
        DF.fastForward(tempTotTime - tempCurTime);
        tempDiff = tempTotTime - tempCurTime;
      }else{
        Serial.printf("Deaccel gear 2\n");
        DF.playFileNum(5);
        tempDiff = 0;
      }
      gearShift = 0;
    }                                        
    else if((speedCatchup > 0 && prevSpeed >= speedCatchup && gearShift == 0 && shiftComplete == 1 && speedCatchup <= 184) || triggered == 7){
      tempCurTime = DF.getCurTime();
      tempTotTime = DF.getTotalTime();
      if(tempCurTime < tempTotTime){
        Serial.printf("Deaccel gear 1 w/ FF!\n");
        DF.playFileNum(6);
        DF.fastForward(tempTotTime - tempCurTime);
        tempDiff = tempTotTime - tempCurTime;
      }else{
        Serial.printf("Deaccel gear 1\n");
        DF.playFileNum(6);
        tempDiff = 0;
      }
      shiftComplete = 0;
      gearShift = 0;
    }else if((speedCatchup < 0 && gearShift == 0) || triggered == 8 || triggered == 9){
      tempCurTime = DF.getCurTime();
      tempTotTime = DF.getTotalTime();
      if(tempCurTime < tempTotTime){
        Serial.printf("Reverse accel w/ FF!\n");
        DF.playFileNum(7);
        DF.fastForward(tempTotTime - tempCurTime);
        tempDiff = tempTotTime - tempCurTime;
      }else{
        Serial.printf("Reverse accel!\n");
        DF.playFileNum(7);
        tempDiff = 0;
      }
      gearShift = 1;
    }else if(speedCatchup < 0 && gearShift == 1 && triggered != 10){
      tempCurTime = DF.getCurTime();
      tempTotTime = DF.getTotalTime();
      if(tempCurTime < tempTotTime){
        Serial.printf("Reverse top end w/ FF!\n");
        DF.playFileNum(8);
        DF.fastForward(tempTotTime - tempCurTime);
        tempDiff = tempTotTime - tempCurTime;
      }else{
        Serial.printf("Reverse top end!\n");
        DF.playFileNum(8);
        tempDiff = 0;
      }
    }else if((speedCatchup < 0 && gearShift == 1) || triggered == 10){
      tempCurTime = DF.getCurTime();
      tempTotTime = DF.getTotalTime();
      if(tempCurTime < tempTotTime){
        Serial.printf("Reverse deaccel w/ FF!\n");
        DF.playFileNum(9);
        DF.fastForward(tempTotTime - tempCurTime);
        tempDiff = tempTotTime - tempCurTime;
      }else{
        Serial.printf("Reverse deaccel!\n");
        DF.playFileNum(9);
        tempDiff = 0;
      }
      gearShift = 0;
    }

    if(PS4.R2()){
      trackR2 = 1;
    }else{trackR2 = 0;}
    if(PS4.L2()){
      trackL2 = 1;
    }else{trackL2 = 0;}

    mp3CurFile = millis();
  }
}

//Shuts off all front lights
void shut(){
  for(int i=0; i < 2; i++){     //Cycles though the first and second LEDs
    leds[i] = CRGB(0, 0, 0);    //Sets the R, G & B values to 0 which is OFF
  }
  Serial.printf("Off!\n");      //Prints status to serial monitor when connected to PC
}

//Makes front light white/very light blue and act as headlights
void head(){
  for(int i = 0; i < 2; i++){
    leds[i] = CRGB(frontHead[0], frontHead[1], frontHead[2]);
  }
}

//Makes all lights turn orange
void haza(){
  for(int i = 0; i < 4; i++){
    leds[i] = CRGB(hazard[0], hazard[1], hazard[2]);
  }
}

//Right hand side lights act as right lane change indicators
void rLane(){
  leds[1] = CRGB(hazard[0], hazard[1], hazard[2]);
  leds[2] = CRGB(hazard[0], hazard[1], hazard[2]);
}

//Left hand side lights act as left lane change indicators
void lLane(){
  leds[0] = CRGB(hazard[0], hazard[1], hazard[2]);
  leds[3] = CRGB(hazard[0], hazard[1], hazard[2]);
}

//Makes the brake lights normal (no brakes are applied)
void brakeReg(){
  for(int i = 2; i < 4; i++){
    leds[i] = CRGB(rearBrakeReg[0], rearBrakeReg[1], rearBrakeReg[2]);
  }
}

//Makes the brake lights brighter (technically just more red)
void brakeAct(){
  for(int i = 2; i < 4; i++){
    leds[i] = CRGB(rearBrakeAct[0], rearBrakeAct[1], rearBrakeAct[2]);
  }
}

//Whole bunch of light stuff
void lights(){
  if (millis() - hazardTS > 500){   //Only fires if half a second passed since the last time this fired
    if (trTrack == 1){              //Checks if hazard lights should be on
      if(lightTurn == 0){           //Checks which state should the lights be in; 0 = hazard, 1 = headlights/off  |
        haza();                     //Turns on hazard lights                                                      |
        lightTurn = 1;              //Switches state for next loop                                               /
      }else{                        //                                                                      <---
        if(sqTrack == 0){           //Checks if front lights should act as headlights or be off
          shut();                   //Turns front lights off
        }else{
          head();                   //Turns front lights into headlights
        }
        if(crTrack == 0){           //Checks if brakes are applied
          brakeReg();               //Makes rear lights normal
        }else{
          brakeAct();               //Makes rear lights as if brakes applied
        }
        lightTurn = 0;              //Switches state for next loop
      }
    }
    if(rBumpTrack == 1){            //Same concept, just for right side lights
      if(lightTurn == 0){
        rLane();
        lightTurn = 1;
      }else{
        if(sqTrack == 0){
          shut();
        }else{
          head();
        }
        if(crTrack == 0){
          brakeReg();
        }else{
          brakeAct();
        }
        lightTurn = 0;
      }
    }
    if(lBumpTrack == 1){            //Same concept, just for left side lights
      if(lightTurn == 0){
        lLane();
        lightTurn = 1;
      }else{
        if(sqTrack == 0){
          shut();
        }else{
          head();
        }
        if(crTrack == 0){
          brakeReg();
        }else{
          brakeAct();
        }
        lightTurn = 0;
      }
    }
    hazardTS = millis();            //Updates timekeeping variable
  }
  FastLED.show();                   //Applies whatever state the lights are in
}

//Gets the state of all buttons     
void stateCheck(){                  
  #if BUTTONS
    tr = PS4.Triangle();            //Triangle button - Hazard lights
    sq = PS4.Square();              //Square button - Headlights
    ci = PS4.Circle();              //Circle button - Clutch
    cr = PS4.Cross();               //Cross button - Brakes
    rBump = PS4.R1();               //Right bumper - Right turn indicators
    lBump = PS4.L1();               //Left Bumper - Left turn indicators
    stateChange();                  //Calls function to update states of other components
  #endif
}

//Changes a lot of things, from clutch state to light states
void stateChange(){
  //Flag alteration, npr. when circle is pressed or hazard lights change
  //Circle is pressed, DC motor winds down while the sound plays as usual
  if (ci && ciTrack == 0){
    Serial.printf("Alt mode entered!\n");
    ciTrack = 1;
    speedCuCircle = 0;
    speedCaCircle = speedCatchup;
  }else if(!ci && ciTrack == 1){
    Serial.printf("Alt mode exited!\n");
    ciTrack = 0;
  }

  //While brakes are applied, the deacceleration variable is increased and the car should slow down 4x faster
  //Rear brake lights should light up when X is pressed
  if (cr && crTrack == 0){
    Serial.printf("Brakes applied!\n");
    crTrack = 1;
    deaccspeed = 4; //Maybe 8 if 4 is not enough
    brakeAct();
    FastLED.show();
  }else if(!cr && crTrack == 1){
    Serial.printf("Brakes unapplied!\n");
    crTrack = 0;
    deaccspeed = 1;
    brakeReg();
    FastLED.show();
  }

  //By having a 100ms time buffer, I'm trying to avoid repeat triggering of these ifs for the lights
  if (millis() - LTSbuttons > 100){
    if(tr && trTrack == 0){
      //Turn hazard lights on
      trTrack = 1;
    }else if(tr && trTrack == 1){
      trTrack = 0;
      if (sqTrack == 0){
        shut();
      }else{
        head();
      }
      if(crTrack == 0){
        brakeReg();
      }else{
        brakeAct();
      }
    }
    if(sq && sqTrack == 0){
      Serial.printf("Headlights on!\n");
      head();
      FastLED.show();
      sqTrack = 1;
    }else if(sq && sqTrack == 1){
      Serial.printf("Headlights off!\n");
      for(int i=0; i < 2; i++){
        leds[i] = CRGB(0, 0, 0);
      }
      sqTrack = 0;
      FastLED.show();
    }
    if(rBump && rBumpTrack == 0 && onlyOne == 0){
      rBumpTrack = 1;
      lBumpTrack = 0;
      onlyOne = 1;
    }else if(rBump && rBumpTrack == 1){
      if (sqTrack == 0){
        shut();
      }else{
        head();
      }
      if(crTrack == 0){
        brakeReg();
      }else{
        brakeAct();
      }
      rBumpTrack = 0;
      onlyOne = 0;
    }
    if(lBump && lBumpTrack == 0 && onlyOne == 0){
      lBumpTrack = 1;
      rBumpTrack = 0;
      onlyOne = 1;
    }else if(lBump && lBumpTrack == 1){
      if (sqTrack == 0){
        shut();
      }else{
        head();
      }
      if(crTrack == 0){
        brakeReg();
      }else{
        brakeAct();
      }
      lBumpTrack = 0;
      onlyOne = 0;
    }
    LTSbuttons = millis();
  }
}

void loop(){
  steer();
  drive();
  sound();
  lights();
}