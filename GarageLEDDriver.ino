

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
        uint32_t elapsed = microsNOW_ - microsREM_; //Get the elapsed time
		microsREM_ = microsNOW_; //remember the time for the next scan
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


//Project text setup
String ProjectName = "DeckLEDDriver"; //The WiFi hostname is taken from this, do not use any symbols or puncuation marks, hyphens are fine, and spaces will be automatically replaced with underscores


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
bool DIManualMotionOnREM;
bool DIManualMotionOffREM;
unsigned int TimeoutTimeTest = 10; //timeout in seconds 
unsigned int TimeoutTimeShort = 120; //timeout in seconds 
unsigned int TimeoutTimeLong = 240; //timeout in seconds
unsigned int TimeoutTime; //variable the Short and Long is moved into depending on the code
unsigned int TimeoutTimer;
float MaxBrightness = 65535.0; //Set to a byte value or max PWM output bit
float testfloat;
float manualoutput;
bool manualmotion;
bool debug;

//Class Definitions
    LEDDimmer Dimmer(1000,65535); //Set dim time and max PWM output
    LoopTime LT(0);
    PulseTimer _1000ms(1000);
   // PulseTimer _100ms(100);


//Serial Read tags - delete if not doing Serial Reads, Also delete fucntions SerialMonitor and SerialEvent
	#define STRINGARRAYSIZE 8 //Array size of parseable string
	#define SERIAL_BUFFER_SIZE 64
	String sSerialBuffer; //Create a global string that is added to character by character to create a final serial read
	String sLastSerialLine; //Create a global string that is the Last full Serial line read in from sSerialBuffer


void setup() {
  Serial.begin(250000);
  pinMode(DIMotionPIN,INPUT);
  pinMode(DIPowerOnRelayPIN,INPUT);
  InitTimersSafe(); //initialize all timers except for 0, to save time keeping functions

 SetPinFrequency(DOLEDPIN, 240); //setting the frequency to 120 Hz

 Serial.print(WelcomeMessage());	//print a welcome message
}



//MAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOP
//MAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOP
void loop() {

	ProcessSerialCommands(); //Respond to serial commands

  //Read inputs
	bool DIMotion = digitalRead(DIMotionPIN);
	bool DIPowerOnRelay = digitalRead(DIPowerOnRelayPIN);

	//If motion is detected
	if (DIMotion) { //If motion is detected
		if (!DIMotionOnREM) { // a one shot, turn lights on and reset the Timeout Timer
			PWMOutSP = MaxBrightness; //turn lights on
			TimeoutTimer = 0;
			TimeoutTime = TimeoutTimeShort;
			DIMotionOnREM = true;
		}
		DIMotionOffREM = false;
	}
	else {
		if (!DIMotionOffREM) {
			//do nothing, just here for completeness
			DIMotionOffREM = true;
		}
		DIMotionOnREM = false;
	}

	//If manual motion is triggered
	if (manualmotion) { //If motion is detected
		if (!DIManualMotionOnREM) { // a one shot, turn lights on and reset the Timeout Timer
			PWMOutSP = MaxBrightness; //turn lights on
			TimeoutTimer = 0;
			TimeoutTime = TimeoutTimeTest;
			DIManualMotionOnREM = true;
		}
		DIManualMotionOffREM = false;
	}
	else {
		if (!DIManualMotionOffREM) {
			//do nothing, just here for completeness
			DIManualMotionOffREM = true;
		}
		DIManualMotionOnREM = false;
	}

	//If the light switch is turned on
	if (DIPowerOnRelay) {
		if (!DIRelayOnREM) { // a one shot, turn lights on and reset the Timeout Timer
			PWMOutSP = MaxBrightness; //turn lights on
			TimeoutTimer = 0;
			TimeoutTime = TimeoutTimeLong;
			DIRelayOnREM = true;
		}
		DIRelayOffREM = false;
	}
	else {
		if (!DIRelayOffREM) { //on a one shot turn lights off
			PWMOutSP = 0.0; //Turn lights off
			DIRelayOffREM = true;
		}
		DIRelayOnREM = false;
	}

	//If either input is on run the timeout timer
	if (PWMOutSP > 0.0 && _1000ms.pulse) { TimeoutTimer++; }//Run the timeout timer}

	if (PWMOutSP > 0.0 && TimeoutTimer >= TimeoutTime) { PWMOutSP = 0.0; manualoutput = 0.0; }//if the lights are on and the timeout timer has finished, turn them off

	PWMOutACT = Dimmer.tick(PWMOutSP); //Create dimmer effect
	if (manualoutput > 0) { PWMOutACT = manualoutput; }
	pwmWriteHR(DOLEDPIN, (uint16_t)PWMOutACT); //Write output

	if (debug==true) {
		_1000ms.tick();
		// LT.tick();
		if (_1000ms.pulse) {
			LT.tick(); //Keep this at the bottom of the loop
			Serial.print("DIMotion="); Serial.print(DIMotion); Serial.print("   ");
			Serial.print("DIPowerOnRelay="); Serial.print(DIPowerOnRelay); Serial.print("   ");
			Serial.print("PWMOutSP="); Serial.print(PWMOutSP); Serial.print("   ");
			Serial.print("PWMOutACT="); Serial.print((uint16_t)PWMOutACT); Serial.print("   ");
			Serial.print("TimeoutTime="); Serial.print(TimeoutTime); Serial.print("   ");
			Serial.print("TimeoutTimer="); Serial.print(TimeoutTimer); Serial.print("   ");
			Serial.println();
			//LT.printStatistics();
		}
	}
}
//MAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOP
//MAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOPMAINLOOP

//==============================================================================================
void ProcessSerialCommands() {

	if (serialRead() == 0) { return; } //Read Data in from the Serial buffer, immediatly return if there is no new data

	Serial.print(ProcessTextCommand(sLastSerialLine));
	sLastSerialLine = ""; //Clear out buffer, This should ALWAYS be the last line in this if..then
}
//==============================================================================================
bool serialRead(void) {
	/*Read hardware serial port and build up a string.  When a newline, carriage return, or null value is read consider this the end of the string
	RETURN 0 when no new full line has been recieved yet
	RETURN 1 when a new full line as been recieved.  The new line is put into sLastSerialLine
	*/
	//Returns 0 when no new full line, 1 when there is a new full line
	while (Serial.available()) {
		char inChar = (char)Serial.read();// get the new byte:
		if (inChar == '\n' || inChar == '\r' || inChar == '\0' || sSerialBuffer.length() >= SERIAL_BUFFER_SIZE) {//if the incoming character is a LF or CR finish the string
			if (sSerialBuffer.length() >= SERIAL_BUFFER_SIZE) { Serial.print(F("WARNING: Serial Buffer exceeded.  Only send ")); Serial.print(SERIAL_BUFFER_SIZE); Serial.println(F("characters at once")); }
			Serial.flush(); //flush the rest of the buffer in case more than one "end of line character" is recieved.  e.g. \n\r are both recieved, this if..then woudl trigger on the \n, do its thing and destroy the last \r..cause who cares
			sLastSerialLine = sSerialBuffer; //Transfer the entire string into the last serial line
			sSerialBuffer = ""; // clear the string buffer to prepare it for more data:
			return 1;
		}
		sSerialBuffer += inChar;// add it to the inputString:
	}
	return 0;
}

//==============================================================================================
String ProcessTextCommand(String s) {
	//Process a text based command whther from Serial or telnet.  Returns some text to display
	//Pass in a CSV command from the HelpMenu()

	bool b = false; //Set a bool false so that if this routine is processed but no commands are valid, we can return an invalid command, no matter what text is passed in
	String aStringParse[STRINGARRAYSIZE]; //Create a small array to store the parsed strings 0-7
	String sReturn; //Text to return
	String stemp = "";

	//--Split the incoming serial data to an array of strings, where the [0]th element is the number of CSVs, and elements [1]-[X] is each CSV
	//If no Commas are detected the string will still placed into the [1]st array
	StringSplit(s, &aStringParse[0]);
	aStringParse[1].toLowerCase();
	/*
	MANUAL COMMANDS
	Commands can be any text that you can trigger off of.
	It can also be paramaterized via a CSV Format with no more than STRINGARRAYSIZE - 1 CSVs: 'Param1,Param2,Param3,Param4,...,ParamSTRINGARRAYSIZE-1'
	For Example have the user type in '9,255' for a command to manually set pin 9 PWM to 255.
	*/
	if (aStringParse[0].toInt() == 1) {
		//Process single string serial commands without a CSV
		//Do something with the values by adding custom if..then statements
		if (aStringParse[1] == F("?")) { sReturn = HelpMenu(); b = true; } //print help menu
		if (aStringParse[1] == F("di")) { 
			if (manualmotion == true) {
				manualmotion = false;
				sReturn = "turning manual motion control off";
			}
			else {
				manualmotion = true;
				sReturn = "turning manual motion control on";
			}
			b = true;
		} //print help menu
		if (aStringParse[1] == F("debug")) {
			if (debug == true) {
				debug = false;
				sReturn = "turning debug off";
			}
			else {
				debug = true;
				_1000ms.Initialize();
				sReturn = "turning debug on";
			}
			b = true;
		} //print help menu
	}
	else if (aStringParse[0].toInt() > 1) {
		//Process multiple serial commands that arrived in a CSV format
		//Do something with the values by adding custom if..then statements

		//Enable/disable Station mode
		if (aStringParse[1] == "out") {
			if (aStringParse[2].toInt() <= 65535 && aStringParse[2].toInt() >= 0) {
				//valid private IP Range
				manualoutput = aStringParse[2].toInt();
				sReturn += F("Setting output to ");
				sReturn += manualoutput;
				b = true;
			}
			else {
				sReturn = F("output must be in the range 0-65535");
			}


		}

	} //end else if(aStringParse[0].toInt() > 1)
	if (b == false) { sReturn = F("Invalid Command"); }

	sReturn += "\r\n"; //add newline character
	return sReturn;
}

//===============================================================================================
void StringSplit(String text, String *StringSplit) {
	/*
	Perform a CSV string split.
	INPUTS:
	text - CSV string to separate
	StringSplit - Pointer to an array of Strings
	DESCRIPTION:
	text can have CSV or not
	Each CSV is placed into a different index in the Array of Strings "StringSplit", starting with element 1.
	Element 0 (StringSplit[0]) will contain the number of CSV found.  Rember to use String.toInt() to convert element[0]
	*/
	char *p;
	char buf[64]; //create a char buff array
				  //text += "\0"; //add null string terminator
	text.toCharArray(buf, 64); //convert string to char array

							   //Split string
	byte PTR = 1; //Indexer
	p = strtok(buf, ",");
	do
	{
		StringSplit[PTR] = p;//place value into array
		PTR++; //increment pointer for next loop
		p = strtok(NULL, ","); //do next split
	} while (p != NULL);

	//Place the number of CSV elements found into element 0
	StringSplit[0] = PTR - 1;

}
//===============================================================================================
String HelpMenu(void) {
	String temp;
	temp += F("\n\r");
	temp += Line();//====================================================================
	temp += F("HELP MENU\n\r");
	temp += ProjectName;
	temp += F("\n\r");
	temp += F("FUNCTIONAL DESCRIPTION:\n\r");
	temp += F("\t12V power supply powering 12V LED Strips through MOSFETs.\n\r");
	temp += F("\tA 12V motion detector is used as a trigger(input has a voltage dividor to arduino input).\n\r");
	temp += F("\tAlso a 120V relay is used to interface to the 120V lights already on the ceiling.If motion is detected or the preinstalled lights are turned on, this program will fade the lights on in 1 seconds.\n\r");
	temp += F("Uses PWM library to alter standard arduino library from 600hz to 120hz increasing the resolution from 8bit to ~15.1bits or 33196 duty cycles.");
	temp += F("\n\r");
	temp += F("COMMANDS:\n\r");
	temp += F("\t'out,XXXXXX' -Set the LED raw output from 0-65535\n\r");
	temp += F("\t'di' -simulate the same code that triggeres the motio sensor\n\r");
	temp += F("\t'debug' -turn on debug stuff\n\r");
	temp += F("\n\r");
	temp += F("\n\r");
	temp += F("For additional information please contact TerryJMyers@gmail.com 215.262.4148\n\r");
	temp += Line();//====================================================================
	temp += F("\n\r");
	return temp;
}

String WelcomeMessage(void) {
	//Printout a welcome message
	String temp;
	temp += Line(); //====================================================================
	temp += ProjectName;
	temp += F("\n\r");
	temp += F("Send '?' for help (all commands must preceed a LF or CR character\n\r");
	temp += Line(); //====================================================================
	temp += F("\n\r");
	return temp;
}
String Line(void) {
	return F("===============================================================================================\n\r");
}
