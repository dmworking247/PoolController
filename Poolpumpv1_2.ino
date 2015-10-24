/* Information about this sketch :
Created by: Dale Murray
21st August 2015

Purpose: Monitors weather conditions through a basic ambient light sensor to determine whether
to switch on or off a device (such as a pool pump) within a defined operating window each day.
Sketch uses an RTC module for date/time, and a SD module for logging data.

Designed for optimizing available solar energy, without any complex or dangerous electrical devices.

The sketch can be customized to your needs. You may wish to update the 'getlight'
function and the 'targetlight' array for some other form of input (voltage from
a small solar cell, command from an RF module, etc).

You could also replace the use of a relay for the switch on/off function, to another
form of trigger such as a servo pushing on a power switch or an IR transmitter to
turn something on remotely.

Hardware:
- Arduino UNO
- Generic RTC module DS3231, sourced from Deal Extreme SKU 222910
- Generic 5528 light sensor for measuring ambient light
// - Barometric BMP180 module  for monitoring air pressure (excluded in v1.0)
- SD card module
- 5v RELAY (250vAC / 10A)

Connections:
- All modules powered from 3.3v
- RTC module SDA to Uno A4, SCL to Uno A5
- 5528 light sensor module signal to Uno A1
- 5V relay to pin 8
*/

#include <Wire.h> // generic wire library from Arduino
#include <Time.h> // generic time library from Arduino
#include <DS1307RTC.h> // library suitable for the RTC module
// #include <SFE_BMP180.h> // include BMP180 library for barometer module
#include <SD.h> // include SD library for creating a log file
#include <SPI.h> // include SPI library from Arduino


/* ENVIRONMENTAL VARIABLES 
Configure these to suit your needs and circumstances */
int startwindow[] = {10,10,10,10,10,10,10,10,10,10,10,10,10}; // start window of solar usage, for each month (0-12). First position is ignored (invalid month)
int endwindow[] = {17,17,17,17,16,16,16,16,16,16,17,17,17}; // end window of solar usage, for each month (0-12). First position is ignored (invalid month)
int maxminutes[] = {360,360,360,360,300,240,240,240,300,300,360,360,360}; // maximum minutes to run in one day, for each month. First position is ignored (invalid month)
int targetlight[] = {798,798,798,798,798,798,798,798,798,798,798,798,798}; // define the target average light level to switch on the circuit, for each month. first position is ignored (invalid month). May take some experimentation for your circumstances. 
const int updateinterval = 5; // amount of minutes between updates 
// const int maxsamples = 6; // maximum number of samples (updates) used for working out average light
int samples[] = {0,0,0};
float lastSample;
const int minimumruntime = 30; // minimum amount in minutes that the switch should stay on
const int powerled = 2;
const int lightlevelled = 3;
const int switchstateled = 4;
const int relaypin = 8;

int currentlight = 0; // define variable for detected light reading
int currenttime = 9999; // create variable for combined time in HMM format. Erroneous value assigned by default.
int currenthour = 99; // create variable for hours. Erroneous value assigned by default.
int prevhour = 0; // create variable for previous hour used in calculations. 
int currentminute = 99; // create variable for minutes. Erroneous value assigned by default.
int currentday = 99; // create variable for month. Erroneous value assigned by default.
int currentmonth = 99; // create variable for month. Erroneous value assigned by default.
int currentyear = 9999; // create variable for year. Erroneous value assigned by default.
int currentpressure = 0; // create variable for pressure measurement (if adding BPM module)
int currenttemp = 0; // create variable for temperature measurement (if adding BPM module)
String currentstate = "Off";
String message = "Hello World";
String currentdate = "yy-mm-dd"; // create variable for date


// define variables for use during loop routine
int avglight = 0;
// int totallight = 0;
// int totalpressure = 0;
// int avgpressure = 0;
float timeswitchedon = 0;
float totaltimetoday = 0;
boolean maxtimeexceeded = false;




// setup variables for SD log
File myFile;
char* filename = "logfile.txt";


// You will need to create an SFE_BMP180 object, here called "pressure":
// SFE_BMP180 pressure;




void setup() {
  Serial.begin(9600);
  
pinMode(relaypin,OUTPUT);
pinMode(switchstateled,OUTPUT);
pinMode(lightlevelled,OUTPUT);
pinMode(powerled,OUTPUT);

for (int i=0; i <= 5; i++) // flash power light to prove we're alive
  {
      digitalWrite(powerled,HIGH); // turn on a power LED to show device is alive
      delay(250);
      digitalWrite(powerled,LOW); // turn on a power LED to show device is alive
      delay(250);
   } 



switchoff(); // default switch condition to off


  
   Serial.print(F("Initializing SD card..."));
    // On many ethernet shields, CS is pin 4. It's set as an output by default.
    // Note that even if it's not used as the CS pin, the hardware SS pin
    // (10 on most Arduino boards, 53 on the Mega) must be left as an output
    // or the SD library functions will not work.
    pinMode(10, OUTPUT);
    
           if (!SD.begin(10)) 
           {
            Serial.println(F("initialization failed!"));
            return;
            }
        Serial.println(F("initialization done."));
        // open the file. note that only one file can be open at a time,
        // so you have to close this one before opening another.
  
if (millis() < 10000) // initial delay of 10 seconds to allow sensors to calibrate
{
delay(10000); 
}

dumpfile(); // dump the contents of the current SD log file to the serial port for analysis with a PC

getlight(); // test current light level
updateaverages(); // call function to measure current light level (initial check);

} // end of setup


void loop() 
{
// flash power LED to show heartbeat of hardware
digitalWrite(powerled,HIGH); // flash the power LED to show device is alive
delay(1000);
digitalWrite(powerled,LOW); // turn off power LED (keeping it on is a waste of power and doesn't prove the Arduino 'heart' is still beating). 

// test current conditions
getlight(); // get current light conditions
gettimenew(); // call function to get time and date from RTC

// getweather(); // call function to measure current pressure and temperature levels

if (currenthour <= startwindow[currentmonth]) // if current time is before our operating hours
{
totaltimetoday = 0; // reset total time so that nothing is carried over from yesterday
maxtimeexceeded = false; // reset boolean which flags maximum time has been exceeded for the day
}

// debug
Serial.print(F("Total Time Today:"));
if (currentstate == "On")
    {
    Serial.println((totaltimetoday + (millis() - timeswitchedon)) / 60000);
    }
    else
    {
    Serial.println(totaltimetoday / 60000);
    }


// Work out whether the maximum run time for today has been exceeded

// Add total time from previous runs to current run (if on)
if ( (maxtimeexceeded == false) && (currentstate == "On") && (((totaltimetoday + (millis() - timeswitchedon)) / 60000) > maxminutes[currentmonth]))
{
Serial.println("Maximum run time exceeded for the day... turning off");
switchoff();
maxtimeexceeded = true;  
message = "max daily runtime";
logtofile(); // create or update log file
}

// Test total time if current state is now off
if ( (maxtimeexceeded == false) && (currentstate == "Off") && ((totaltimetoday / 60000) > maxminutes[currentmonth]))
{
message = "max daily runtime";
Serial.println("Maximum run time exceeded for the day.");
maxtimeexceeded = true;  
logtofile(); // create or update log file
}

// determine if the current time is within our operation window for the respective month, and we have not exceeded daily maximums
if ((currenthour >= startwindow[currentmonth]) && (currenthour <= endwindow[currentmonth]) && (maxtimeexceeded == false))
{
Serial.println(F("Within operating window..."));


// print light values 
Serial.print(F("Current light: "));
Serial.print(currentlight);

Serial.print(F(" ; Average light: "));
Serial.println(avglight);

  
  
// we are within the time window to consider switching on. Test conditions.
  if ((avglight >= targetlight[currentmonth]) && (currentstate == "Off"))
  {
  switchon();
  }

// consider switching off if light level has dropped
  if ((avglight < targetlight[currentmonth]) && (currentstate == "On")) 
  {
  Serial.println(F("conditions have dropped below target levels"));
  Serial.print(F("time elapsed: "));
  Serial.println((millis() - timeswitchedon) / 60000);
    if ((millis() - timeswitchedon ) > (minimumruntime * 60000)) // allow minimum time to stay switched on
    {
    Serial.println(F("Minimum on time elapsed"));
    switchoff();
    message = "min time elapsed in lowlight ";
    logtofile(); // create or update log file
    }

  }
}

// switch off if after window
if ((currentstate == "On") && (currenthour > endwindow[currentmonth]))
{
switchoff();
message = "switch off after operating hours";
logtofile(); // create or update log file
}


delay(59000); // delay 59 seconds (the other 1 second delay happens at the start of the loop for the power led flash)
 

// every 5 minutes
if ( (millis() - lastSample) > (updateinterval * 60000) )
{
lastSample = millis();
getlight();
updateaverages();
checklogsize(); // delete log file if it is over sized
printstats(); // write update to serial port
}


} // end of loop

void print2digits(int number) {
  if (number >= 0 && number < 10) {
    Serial.print('0');
  }
  Serial.print(number);
}

void logtodigits(int number) {
  if (number >= 0 && number < 10) 
  {
    myFile.print('0');
  }
  myFile.print(number);
}


void gettimenew()
{
  tmElements_t tm;

    if (RTC.read(tm))
    {
    currenthour = (tm.Hour); // set global variable to current time (hours)
    currentminute = (tm.Minute); // set global variable to current time (minutes)
    currenttime = ((currenthour * 100) + currentminute);
    // currenttime = String(currenthour); // store hour
    // currenttime += String(currentminute); // append minutes
    currentday = (tm.Day); // set global variable to current (date as number)
    currentmonth = (tm.Month); // set global variable to current  (month as a number)
    currentyear =  (tmYearToCalendar(tm.Year));


    
    }
    
    else 
    {
        if (RTC.chipPresent()) 
        {
          Serial.println(F("The DS1307 is stopped.  Please run the SetTime"));
          Serial.println(F("example to initialize the time and begin running."));
          Serial.println();
        } 
        else 
        {
          Serial.println(F("DS1307 read error!  Please check the circuitry."));
          Serial.println();
        }
    }

}




void getlight()
{
currentlight = analogRead(A1);

            if (currentlight > targetlight[currentmonth]) // measures current light level compared to target
            {
            digitalWrite(lightlevelled,HIGH); // turn on light if current light levels are good
            }
            else
            {
            digitalWrite(lightlevelled,LOW); // else turn off light level led
            }

return;
}



void checklogsize()
{
  myFile = SD.open(filename, FILE_WRITE);
  int logsize = myFile.size();
  Serial.print(F("Log Size:"));
  Serial.println(logsize);
  myFile.close();         // close the file:
  if (logsize > 1000000) // if over 1mb
  {
  Serial.println("Log file oversized");
  if (SD.exists(filename)) 
      {
        SD.remove(filename);
        Serial.print(filename);
        Serial.println(F(" deleted"));
      }
  }

  
  /*

  */
}

// /* comment out this function if you don't need to monitor the serial port. It will save memory.
void printstats()
{
  /*
            Serial.print(F("Log Date"));
            Serial.print(F(","));
            Serial.print(F("Log Time"));
            Serial.print(F(","));
            Serial.print(F("TempC"));
            Serial.print(F(","));
            Serial.print(F("Pressure"));
            Serial.print(F(","));
            Serial.print(F("LightLevel"));
            Serial.print(F(","));
            Serial.println(F("State"));
  */
            
// logging date in yyyy-mm-dd format
        Serial.print(currentyear);
        Serial.print(F("-"));
        print2digits(currentmonth);
        Serial.print(F("-"));
        print2digits(currentday);
        
        Serial.print(F(","));
        
        // logging time in HH:MM format
        print2digits(currenthour);
        Serial.print(F(":"));
        print2digits(currentminute);
        
        Serial.print(F(","));
        
        // logging temperature
        Serial.print(currenttemp);
        
        Serial.print(F(","));
        
        // logging pressure
        Serial.print(currentpressure);
        
        Serial.print(F(","));
        
        // logging light level
        Serial.print(currentlight);
        
        Serial.print(F(","));
        
        // logging state of servo/relay
        Serial.println(currentstate);
            
} // end of function
// */

void logtofile()
 {

      if (SD.exists(filename)) 
      {
        myFile = SD.open(filename, FILE_WRITE);
        // if the file opened okay, write to it:
        if (myFile) {
        // log new data:
        // logging date in yyyy-mm-dd format
        myFile.print(currentyear);
        myFile.print("-");
        logtodigits(currentmonth);
        myFile.print("-");
        logtodigits(currentday);
        
        myFile.print(",");
        
        // logging time in HH:MM format
        logtodigits(currenthour);
        myFile.print(":");
        logtodigits(currentminute);
        
        myFile.print(",");
        
        // logging temperature
        myFile.print(currenttemp);
        
        myFile.print(",");
        
        // logging pressure
        myFile.print(currentpressure);
        
        myFile.print(",");
        
        // logging light level
        myFile.print(currentlight);
        
        myFile.print(",");
        
        // logging state of servo/relay
        myFile.print(currentstate);
        
        myFile.print(",");
        myFile.println(message);
        // end log new data
        myFile.close();         // close the file:
        
        message = ""; // clear message string
        } 
        else 
        {
        // if the file didn't open, print an error:
        Serial.println(F("error opening file"));
        }
    
      }
      else 
      {
        Serial.println(F("Log file doesn't exist. Creating..."));  
        
        myFile = SD.open(filename, FILE_WRITE); // create the file
        // if the file opened okay, write to it:
        if (myFile) 
        {
        // create the header
            myFile.print("Log Date");
            myFile.print(",");
            myFile.print("Log Time");
            myFile.print(",");
            myFile.print("Temp(c)");
            myFile.print(",");
            myFile.print("Pressure");
            myFile.print(",");
            myFile.print("LightLevel");
            myFile.print(",");
            myFile.println("State");
        // end header
        myFile.close();         // close the file:
        } 
        else 
        {
        // if the file didn't open, print an error:
        Serial.println(F("error opening file"));
        }
      }

    } // end function
    
    
void switchon()
{
currentstate = "On";
Serial.println(F("********************************"));
Serial.println(F("Switching on"));
Serial.println(F("********************************"));
digitalWrite(switchstateled,HIGH); // switch state LED
digitalWrite(relaypin,HIGH); // relay circuit on
delay(1000);
timeswitchedon = millis();
message = "Switch On";
logtofile();
}


void switchoff()
{
currentstate = "Off";
Serial.println(F("********************************"));
Serial.println(F("Switching off"));
Serial.println(F("********************************"));
digitalWrite(switchstateled,LOW); // switch state LED
digitalWrite(relaypin,LOW); // relay circuit off
delay(1000);
totaltimetoday = (totaltimetoday + timeswitchedon);
timeswitchedon = millis();
}


void updateaverages()
{
// cycle through samples so that we keep the last three
samples[2] = samples[1];
samples[1] = samples[0];
samples[0] = currentlight;

// force current light values if previous samples are empty (eg during startup)
if (samples[2] < 1) 
  {samples[2] = currentlight;}
if (samples[1] < 1) 
  {samples[1] = currentlight;}
  
// calculate new average for the last 3 samples
avglight = ( (samples[2] + samples[1] + samples[0]) / 3);

message = "Interval Update";
logtofile();
}


void dumpfile()  // if the file is available, write to it:
{
File dataFile = SD.open(filename);
  if (dataFile) 
  {
   Serial.println(F("BEGIN LOG FILE DUMP **************"));
    while (dataFile.available()) 
    {
      Serial.write(dataFile.read());
    }
    dataFile.close();
    Serial.println(F("END LOG FILE DUMP **************"));
  }
  // if the file isn't open, pop up an error:
  else 
  {
    Serial.print(F("error opening "));
    Serial.println(dataFile);
  }
}
