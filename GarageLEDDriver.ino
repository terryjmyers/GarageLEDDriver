/*
Garage LED Driver
12V power supply powering 12V LED Strips through MOSFETs. 
A 12V motion detector is used as a trigger (input has a voltage dividor to arduino input).
Also a 120V relay is used to interface to the 120V lights already on the ceiling.  If motion is detected or the preinstalled lights are turned on, this program will fade the lights on in 1 seconds

Uses PWM library to alter standard arduino library from 600hz to 120hz increasing the resolution from 8bit to ~15.1bits or 33196 duty cycles.



v1.0 Terry Myers


*/

#include <PWM.h>


class PulseTimer {

  public:
    PulseTimer(uint32_t t) { //Class definition, set the pulse timer in ms
      TimerPRE_ = t; //Set the internal timer preset
      millisREM_ = millis(); //Capture the first millis Remember value
    };
    void tick() { //Call this function only once each loop for each created class
      pulse=false; //Reset the pulse each time
      uint32_t millisNOW_ = millis(); //get the current millis()
      uint32_t elapsed = millisNOW_ - millisREM_; //Get the elapsed time

      if (elapsed >= TimerPRE_) { //If enough time has elapsed initate the pulse
        pulse=true; //Set the pulse bit true for one loop
        uint32_t error = elapsed - TimerPRE_; //Calculate the error that the timer has accumulcated
        millisREM_ = millisNOW_ - error; //Capture the next time stamp and subtract the error
      }
    }
    bool pulse;
    uint32_t preset() {return TimerPRE_;}
    void Initialize() {millisREM_ = millis();} //Initialize the timer.  Typically not needed as the Class call does the same but provided here just in case you want to reset this
    
  private:
    uint32_t millisREM_; //Millis remember register
    uint32_t TimerPRE_; //Internal timer preset.  Set with the PulseTimer class call
    
};




class LoopTime {

  public:
    LoopTime(uint16_t t) {
      t=0;
    }
    void tick() {//Update scan time statistics.  This routine should run every scan of the program

      //Calculate the current scan time
        uint32_t microsCurrent_ = micros(); // Record the current micros
        uint32_t LastScanTime_ = microsCurrent_ - microsREM_; //calculate the last scan time
        microsREM_ = microsCurrent_; //Remember for next time

      //Calculate min/max scan time
        if (LastScanTime_ > MaxScanTimeuS_) { //Capture the maximum scan time
          MaxScanTimeuS_ = LastScanTime_;
        }
        if (LastScanTime_ < MinScanTimeuS_) { //Capture the fastest scan time
          MinScanTimeuS_ = LastScanTime_;
        }
  
      //Calculate average and reset statistics one a second
        ScanCounter_++; //count the nunmber of scans to determine averae
        uint32_t millisNOW_ = millis(); //get the current millis()
        uint16_t elapsed = millisNOW_ - millisREM_; //Get the elapsed time
        if (elapsed >= TimerPRE_) {
          //Calculate average
            AvgScanTimeuS_ = float(elapsed) * 1000.0 / float(ScanCounter_); 
          //Calculate scans per second
            ScansPerSec_ = ScanCounter_*1000/elapsed; //An integer is used instead of a more precise float for speed.
            ScanCounter_ = 0;
          //Reset statistics
            MaxScanTimeuS_=0;
            MinScanTimeuS_=4294967295;
            millisREM_ = millisNOW_;
        }
    }
    //A return units of ms was choosen for readability
    float Avg() {return AvgScanTimeuS_/1000.0;} //Return the Average Scan time in ms
    float Min() {return MinScanTimeuS_/1000.0;} //Return the minimum Scan time in ms
    float Max() {return MaxScanTimeuS_/1000.0;} //Return the maximum Scan time in ms
    uint32_t ScansPerSec() {return ScansPerSec_;} //Return the number of scans per second
    void printStatistics() {
      //print full array of statistics to serial monitor.
          Serial.print(F("Avg="));
          Serial.print(Avg(),4);
          Serial.print(F("ms "));
          Serial.print(F("scans/sec:"));
          Serial.print(ScansPerSec());
          Serial.print(F(" Min="));
          Serial.print(Min(),4);
          Serial.print(F("ms "));
          Serial.print(F("Max="));
          Serial.print(Max(),4);
          Serial.println(F("ms"));
    }
    
  private:
    uint32_t MinScanTimeuS_ = 4294967295; //smallest recorded ScanTime in us.  Set to the max number so that when a smaller number is recorded during the program its updated
    uint32_t MaxScanTimeuS_; //largest ScanTime in us
    uint32_t ScanCounter_; //used for average
    float AvgScanTimeuS_; //ScanTime in us averaged over one sec
    uint32_t microsREM_; //last micros() to remember
    uint32_t millisREM_; //Used for timer function
    uint16_t TimerPRE_ = 1009; //Set the timer to calculate average.  A prime number is used to assist with overlapping other timers
    uint32_t ScansPerSec_; //An integer is used instead of a more precise float for speed.
    
};

class LEDDimmer {
    //Fade to a value in a certain amount of time.  The rate of change is fixed by the class call
    
  public:
      LEDDimmer(uint32_t FadeTime, uint16_t Max) { //Class definition, number of ms to dim to Max value (typically 255 for 8bit PWM output, 65536 when using pwmWriteHR() in PWM library)
          FadeTimeus_ = FadeTime*1000; //number of ms to dim
          Max_ = Max; //Max Number that the fade time should be divided by, typically your max PWM output.  e.g. 255 on arduino
      };
      
      float tick(float sp) { 
        //pass in the setpoint to dim too, returns the dimmed number.  Call this routine every scan.  
        //The rate of change is fixed by the class call at FadeTime / Max.
        // e.g. FadeTime = 2000ms, Max = 255 If you are currently at 127 (half way), and you change to 0, it will take 1000ms
        
        uint32_t microsNOW_ = micros(); //get the current millis()
        microsREM_ = microsNOW_; //remember the time for the next scan
        uint32_t elapsed = microsNOW_ - microsREM_; //Get the elapsed time
        float ChangeThisTick = float(Max_) / (float(FadeTimeus_)/float(elapsed)); //Calculate the number to add or subtract to the actual value this tick
        if (act_ <= sp) {act_ = act_ + ChangeThisTick;} //Add to the actual value a little bit
        if (act_ >= sp) {act_ = act_ - ChangeThisTick;} //subtract from the actual value a little bit
        if (abs(act_ - sp) < ChangeThisTick*2) {act_ = sp;} //Set the actual to the setpoint when in range
         
         //Use a gamma corrected value or not based on http://www.poynton.com/PDFs/SMPTE93_Gamma.pdf
            if (gammaCorrection > 0 ) {
              //Perform a gamma correction.  LEDs do nto have a linear brightness.  This helps to simulate that.
              // Pass in a value between 0 and Max, and a gamma corrected value will be returned
              return constrain(pow(act_ / float(Max_), gammaCorrection) * Max_ +0.49,0.0,Max_); //The 0.49 is there to move the first value up a little
            } else {
              return act_;
            } 
      };
  private:
    float gammaCorrection = 2.2; //2.2 typically gives a good linearization of light output to human perception.  Adafruit choose 2.8, see what works for you.  Set to 0 to not use gamma correction
    uint32_t FadeTimeus_; //us fade time
    uint16_t Max_; //Maximum number that you will fade to.
    float act_; //actual value to return
    uint32_t microsREM_; //Remember value for micros
    
};



//#define debug
#define DIMotionPIN 2
#define DIPowerOnRelayPIN 5
#define DOLEDPIN 9

float PWMOutSP;
float PWMOutACT;
bool DIMotionOnREM;
bool DIMotionOffREM;
bool DIRelayOnREM;
bool DIRelayOffREM;
unsigned int TimeoutTimeShort = 120; //timeout in seconds 
unsigned int TimeoutTimeLong = 240; //timeout in seconds
unsigned int TimeoutTime; //variable the Short and Long is moved into depending on the code
unsigned int TimeoutTimer;
float MaxBrightness = 65535.0; //Set to a byte value or max PWM output bit
float testfloat;

//Class Definitions
    LEDDimmer Dimmer(1000,65535); //Set dim time and max PWM output
    LoopTime LT(0);
    PulseTimer _1000ms(1000);
   // PulseTimer _100ms(100);

    
void setup() {
  Serial.begin(250000);
  pinMode(DIMotionPIN,INPUT);
  pinMode(DIPowerOnRelayPIN,INPUT);
  InitTimersSafe(); //initialize all timers except for 0, to save time keeping functions

 SetPinFrequency(DOLEDPIN, 240); //setting the frequency to 120 Hz
}



//MAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOP
//MAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOP
void loop() {


  //Read inputs
      bool DIMotion=digitalRead(DIMotionPIN);
      bool DIPowerOnRelay=digitalRead(DIPowerOnRelayPIN);

  //If motion is detected
      if (DIMotion) { //If motion is detected
          if (!DIMotionOnREM) { // a one shot, turn lights on and reset the Timeout Timer
              PWMOutSP = MaxBrightness; //turn lights on
              TimeoutTimer = 0;
              TimeoutTime=TimeoutTimeShort;
              DIMotionOnREM=true;
          }
        DIMotionOffREM=false;
      } else {
          if (!DIMotionOffREM) {
              //do nothing, just here for completeness
              DIMotionOffREM=true;
          }
        DIMotionOnREM=false;
      }

  //If the light switch is turned on
      if (DIPowerOnRelay) { 
          if (!DIRelayOnREM) { // a one shot, turn lights on and reset the Timeout Timer
              PWMOutSP = MaxBrightness; //turn lights on
              TimeoutTimer = 0;
              TimeoutTime=TimeoutTimeLong;
              DIRelayOnREM=true;
          }
        DIRelayOffREM=false;
      } else {
         if (!DIRelayOffREM) { //on a one shot turn lights off
              PWMOutSP = 0.0; //Turn lights off
              DIRelayOffREM=true;
          }
        DIRelayOnREM=false;
      }

  //If either input is on run the timeout timer
      if (PWMOutSP>0.0 && _1000ms.pulse) {TimeoutTimer++;}//Run the timeout timer}

  if (PWMOutSP>0.0 && TimeoutTimer>=TimeoutTime) {  PWMOutSP = 0.0;}//if the lights are on and the timeout timer has finished, turn them off

PWMOutACT = Dimmer.tick(PWMOutSP); //Create dimmer effect

pwmWriteHR(DOLEDPIN,(uint16_t)PWMOutACT); //Write output
    
    #ifdef debug
       _1000ms.tick();
      LT.tick();
      if (_1000ms.pulse) {
          LT.tick(); //Keep this at the bottom of the loop
          Serial.print("DIMotion="); Serial.print(DIMotion); Serial.print("   ");
          Serial.print("DIPowerOnRelay="); Serial.print(DIPowerOnRelay); Serial.print("   ");
          Serial.print("PWMOutSP="); Serial.print(PWMOutSP); Serial.print("   ");
          Serial.print("PWMOutACT="); Serial.print((uint16_t)PWMOutACT); Serial.print("   ");
          Serial.print("TimeoutTime="); Serial.print(TimeoutTime); Serial.print("   ");
          Serial.print("TimeoutTimer="); Serial.print(TimeoutTimer); Serial.print("   ");
          Serial.println();
          LT.printStatistics();
        }
      #endif
}
//MAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOP
//MAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOP


