#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <SoftwareSerial.h>
#include<HardwareSerial.h>
#include <TimeLib.h>
#include <TimeAlarms.h>
#include <time.h>
#include <LiquidCrystal.h>
#include <ThreeWire.h>  
#include <RtcDS1302.h>
#include <WiFi.h>



// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 19, en = 23, d4 = 18, d5 = 5, d6 = 4, d7 = 15;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);


ThreeWire myWire(26,25,33); // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);

//SoftwareSerial SIM800L(17, 16); // RX, TX

#define genSensePin 2
#define pushButton_pin 34
#define motor 32
#define motorStop 36
//enter wifi credentials 
const char* ssid     = "your-ssid";
const char* password = "your-password";

// UltraSonic Sensor setup
#define SOUND_SPEED 0.034 // cm/us
#define trigPin 13
#define echoPin 14
long duration;
float distanceCm;
double height;


const double pi = 3.14159265;
const double length = 2.0;
const double diameter = 1.8;
const double radius = 0.9;
double volume;

void setupRtcModule();
void setupTimeHelpers();
//void setupGSM();
void triggerAlarm();
double calculateVolume(double hgt);
//void sendMessage(double message, bool genStatus);
double getHeight();
double movingAverage(double *arr, int n);
void displayVolume(double msg);
void printDateTime(const RtcDateTime& dt);
void IRAM_ATTR fillTank();
//void IRAM_ATTR genStateSense();

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  lcd.begin(16, 4);

  WiFi.begin(ssid, password);
  delay(2000);
  if(!WL_CONNECTED)
  {
    Serial.println("Wifi could not connect");
  }

  // setup ultrasonic sensor
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  //setup interrupts to measure fill Tank when you press a push button
 pinMode(pushButton_pin, INPUT_PULLUP);
 attachInterrupt(pushButton_pin, fillTank, FALLING);
 pinMode(motor,OUTPUT);
 pinMode(motorStop, INPUT_PULLUP);
 /*
  //setup generator sensing interrput
 pinMode(genSensePin, INPUT);
 attachInterrupt(genSensePin, genStateSense, CHANGE);  
*/
  //setupGSM();
  setupRtcModule();
  setupTimeHelpers();
  // initialize LCD
  Serial.println("setting up lcd");


  while (!Serial)
    ; // wait for Arduino Serial Monitor
  delay(200);

 /*
  // create the alarms, to trigger at specific times
  Alarm.alarmRepeat(8, 0, 0, triggerAlarm);  // 8:00am every day
  Alarm.alarmRepeat(15, 30, 0, triggerAlarm); // 3:30pm every day

  Serial.println("Alarm set");  
  */
}

void loop()
{
  RtcDateTime now = Rtc.GetDateTime();

    printDateTime(now);
    Serial.println();

    if (!now.IsValid())
    {
        // Common Causes:
        //    1) the battery on the device is low or even missing and the power line was disconnected
        Serial.println("RTC lost confidence in the DateTime!");
    }

  height= getHeight();  
  Serial.println(height);
  displayVolume(calculateVolume(movingAverage(&height, 5)));
  delay(2500);
}

void setupRtcModule()
{
 Rtc.Begin();

    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);

    if (!Rtc.IsDateTimeValid()) 
    {
        // Common Causes:
        //    1) first time you ran and the device wasn't running yet
        //    2) the battery on the device is low or even missing

        Serial.println("RTC lost confidence in the DateTime!");
        Rtc.SetDateTime(compiled);
    }

    if (Rtc.GetIsWriteProtected())
    {
        Serial.println("RTC was write protected, enabling writing now");
        Rtc.SetIsWriteProtected(false);
    }

    if (!Rtc.GetIsRunning())
    {
        Serial.println("RTC was not actively running, starting now");
        Rtc.SetIsRunning(true);
    }

    RtcDateTime now = Rtc.GetDateTime();
    if (now < compiled) 
    {
        Serial.println("RTC is older than compile time!  (Updating DateTime)");
        Rtc.SetDateTime(compiled);
    }
    else if (now > compiled) 
    {
        Serial.println("RTC is newer than compile time. (this is expected)");
    }
    else if (now == compiled) 
    {
        Serial.println("RTC is the same as compile time! (not expected but all is fine)");
    }  
  
}

//set the system time for timelib

void setupTimeHelpers()
{ 
  //setSyncProvider(Rtc.GetDateTime());  
  RtcDateTime now = Rtc.GetDateTime();
  setTime(now.Hour(), now.Minute(), now.Second(), now.Day(), now.Month(), now.Year());

  Serial.print("Time helpers set up");
}
/*
// setting up the gms module
void setupGSM()
{

  SIM800L.begin(9600);
  delay(1000);
  Serial.println("GSM Setup Complete!");
}
*/
// function that is called when the alarm is triggered
/*
void triggerAlarm()
{

  height = getHeight();
  displayVolume(calculateVolume(movingAverage(&height,5))); // Display the volume on LCD screen
 // sendMessage(calculateVolume(movingAverage(&height, 5))), digitalRead(genSensePin));   // send the message using the gsm module
  lcd.setCursor(1, 3);
  lcd.print("alarm!!");
  delay(5000);
  lcd.clear();
  setupTimeHelpers();
  
}*/

// function to get the liquid level using an ultrasonic sensor
double getHeight()
{

  // Clears the trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Reads the echo, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);

  // Calculate the distance
  distanceCm = duration * SOUND_SPEED / 2;
  height = diameter-((distanceCm-30 )/ 100); // height in meters
  /*if(((distanceCm-30)/100)>=height)
  { 
     height=0;   
  }/*else
  {
    height = diameter-((distanceCm-30 )/ 100); // height in meters
   */ if(height<0){
      height= 0;
    }
    
    if ( height >= diameter)
    {
      height= diameter;
    }
  
  
  return height;
}

//code for simple moving average filter
double movingAverage(double *arr, int n)
{
  
  double sum= 0;

   for(int i= 0; i<=n; i++)
   {
       sum+= arr[i];  
   }
   
   return sum/n;
}

// https://mathworld.wolfram.com/HorizontalCylindricalSegment.html
// Function to calculate volume of a cylinder laying parallel to the horizontal
double calculateVolume(double hgt)
{
  double radius_height = radius - height;
  double radian_angle = acos((radius_height) / radius); //*radian;
  double sqr_root = sqrt((2 * radius * height - pow(height, 2)));

  double volume = length * (pow(radius, 2) * radian_angle - radius_height * sqr_root);

  return volume*1000;
}
/*
// function to send message via SMS
void sendMessage(double message, bool genState)
{
  
  SIM800L.println("AT+CMGF=1"); // Set SMS mode to text
  delay(1000);
  SIM800L.println("AT+CMGS=\"+1234567890\""); // Replace with your phone number
  delay(1000);

  if(genState == true)
  {
    SIM800L.print("Generator ON, Fuel/L left in the tank is"); // Replace with your message
    SIM800L.println(message+0x30); // value in ASCII
  }
  else
  { 
    SIM800L.print("Generator OFF, Fuel/L left in the tank is"); // Replace with your message
    SIM800L.println(message); // value in ASCII
  }
  
  delay(1000);
  SIM800L.write(26); // Send CTRL+Z character
  delay(1000);
}

*/

// Function to display a message using the LCD
void displayVolume(double vol)
{
  lcd.setCursor(0, 1);
  lcd.print("Litres left:");
  lcd.setCursor(0,2);
  lcd.print(int(vol));
}

// ISR to handle when one presses a push button to measure volume
void ARDUINO_ISR_ATTR fillTank()
{ 
  while(volume<5000|digitalRead(motorStop==HIGH)) {
    
    digitalWrite(motor, HIGH);  
    height= getHeight();  
    displayVolume(calculateVolume(movingAverage(&height, 5)));
    lcd.setCursor(0,3);
    lcd.print("Filling....");
    delay(1000);

  
  }
    
}
/*
void IRAM_ATTR genStateSense()
{
  if(digitalRead(genSensePin)==HIGH)
  {
   height = getHeight();
   sendMessage(calculateVolume(movingAverage(&height,5)), true);  
  }   
  else     
  {
    
    sendMessage(calculateVolume(movingAverage(&height,5)), false);
  }

}
*/


//function to print date and time in the rtc lib
#define countof(a) (sizeof(a) / sizeof(a[0]))

void printDateTime(const RtcDateTime& dt)
{
    char datestring[20];

    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u/%02u/%04u %02u:%02u"),
            dt.Month(),
            dt.Day(),
            dt.Year(),
            dt.Hour(),
            dt.Minute());
    lcd.setCursor(0, 0);     
    lcd.print(datestring);
}
