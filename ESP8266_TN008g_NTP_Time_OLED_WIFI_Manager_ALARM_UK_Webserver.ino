// The MIT License (MIT) Copyright (c) 2016 by David Bird.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files
// (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge,
// publish, distribute, but not to use it commercially for profit making or to sub-license and/or to sell copies of the Software or to
// permit persons to whom the Software is furnished to do so, subject to the following conditions: 
//   The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
//   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
//   LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
//   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// See more at http://dsbird.org.uk
//
// ESP8266 Clock displayed on an OLED shield (64x48) using the Network Time Protocol to update every minute
// (c) D L Bird 2016
//
String   clock_version = "10.0";
#include <NTPClient.h> 
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <Ticker.h>    // Core library
#include <EEPROM.h>

#include <DNSServer.h>
#include <WiFiManager.h>      //https://github.com/tzapu/WiFiManager

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h> // SCL = GPIO5 and SDA = GPIO4
#define  OLED_RESET 0         // GPIO0
Adafruit_SSD1306 display(OLED_RESET);

String day_of_week[7]    = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};  // Sunday is dayOfWeek 0
String month_of_year[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"}; // January is month 0
String webpage           = "";

WiFiClient client;
WiFiUDP time_udp;
Ticker  screen_update;

// You can specify the time server source and time-zone offset in milli-seconds.
float TimeZone;
bool  AMPM = false;
int   epoch,local_epoch,current_year,current_month,current_day,dayOfWeek,hours,UTC_hours,minutes,seconds;
byte  set_status, alarm_HR, alarm_MIN, alarmed;
bool  DST = false, dstUK, dstUSA, dstAUS, alarm_triggered = false; // DayLightSaving on/off and selected zone indicators

#define output_pin D1 // the alarm pin that goes high when triggered, note this is the only spare Data pin

// If your country is ahead of UTC by 2-hours use 2, or behind UTC by 2-hours use -2
// If your country is 1-hour 30-mins ahead of UTC use 1.5, so use decimal hours
// Change your time server to a local one, but they all return UTC!
NTPClient timeClient(time_udp);
//Could use the following, but don't use the programme DST option!
//NTPClient timeClient(time_udp,"uk.pool.time.org", TimeZone * 3600, 60000); // (logical name, address of server, time-zone offset in seconds, refresh-rate in mSec)

ESP8266WebServer server(80);  // Set the port you wish to use, a browser default is 80, but any port can be used, if you set it to 5555 then connect with http://nn.nn.nn.nn:5555/
  
void setup(){
  Serial.begin(115200);
  EEPROM.begin(12);
  //  0 - TimeZone 4-bytes for floating point
  //  1
  //  2
  //  3
  //  4 - AMPM Mode true/false
  //  5 - set status 
  //  6 - dst_UK  true/false
  //  7 - dst_USA true/false
  //  8 - dst_AUS true/false
  //  9 - Alarm Hours
  // 10 - Alarm Minutes
  // 11 - alarmed (nor not)
  EEPROM.get(0,TimeZone);
  EEPROM.get(4,AMPM);
  // A simple test of first CPU usage, if first time run the probability of these locations equating to 0 is low, so initilaise the timezone
  if (set_status != 0 ) TimeZone = 0;
  EEPROM.get(5,set_status); 
  EEPROM.get(6,dstUK);
  EEPROM.get(7,dstUSA);
  EEPROM.get(8,dstAUS);
  EEPROM.get(9,alarm_HR);
  EEPROM.get(10,alarm_MIN);
  EEPROM.get(11,alarmed);
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  //------------------------------
  //WiFiManager intialisation. Once completed there is no need to repeat the process on the current board
  WiFiManager wifiManager;
 
  // A new OOB ESP8266 will have no credentials, so will connect and not need this to be uncommented and compiled in, a used one will, try it to see how it works
  // Uncomment the next line for a new device or one that has not connected to your Wi-Fi before or you want to reset the Wi-Fi connection
  // Then restart the ESP8266 and connect your PC to the wireless access point called 'ESP8266_AP' or whatever you call it below
  // wifiManager.resetSettings();
  // Next connect to http://192.168.4.1/ and follow instructions to make the WiFi connection

  // Set a timeout until configuration is turned off, useful to retry or go to sleep in n-seconds
  wifiManager.setTimeout(180);

  //fetches ssid and password and tries to connect, if connections succeeds it starts an access point with the name called "ESP8266_AP" and waits in a blocking loop for configuration
  if(!wifiManager.autoConnect("ESP8266_AP")) {
    Serial.println("failed to connect and timeout occurred");
    delay(3000);
    ESP.reset(); //reset and try again
    delay(5000);
  }
  // At this stage the WiFi manager will have successfully connected to a network, or if not will try again in 180-seconds
  //------------------------------
  // Print the IP address

  timeClient.begin(); // Start the NTP service for time 
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.print("Synching..");
  Serial.print("Use this URL to connect: ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
  display_ip();
  display.display(); // Update screen
  screen_update.attach_ms(1000, display_time); // Call display update routine every 1-sec. Tick units are 1.024mS adjust as required, I've used 1000mS, but should be 976 x 1.024 = 1-sec
  timeClient.update();                         // NTP updates are allowed no-more than every 60-seconds and the 'display_time' function independently updates the seconds
  epoch   = timeClient.getEpochTime();         // You can test moving time forward or back by adding or subtracting the required number of seconds 1-day = 24*60*60
  seconds = epoch % 60;

  server.begin();  // Start the webserver
  Serial.println("Webserver started...");

  server.on("/", NTP_Clock_home_page);  
  server.on("/AMPM_ONOFF", AMPM_ONOFF);         // Define what happens when a client requests attention
  server.on("/TIMEZONE_PLUS", timezone_plus);   // Define what happens when a client requests attention
  server.on("/TIMEZONE_MINUS", timezone_minus); // Define what happens when a client requests attention
  server.on("/DST_UK",  dst_UK);                // Define what happens when a client requests attention
  server.on("/DST_USA", dst_USA);               // Define what happens when a client requests attention
  server.on("/DST_AUS", dst_AUS);               // Define what happens when a client requests attention
  server.on("/ALARM_PLUS",  alarm_PLUS);        // Define what happens when a client requests attention
  server.on("/ALARM_MINUS", alarm_MINUS);       // Define what happens when a client requests attention
  server.on("/ALARM_RESET", alarm_RESET);       // Define what happens when a client requests attention
  server.on("/ALARM_ONOFF", alarm_ONOFF);       // Define what happens when a client requests attention
  server.on("/RESET_VALUES", reset_values);     // Define what happens when a client requests attention
  server.on("/EXIT_SETUP", exit_setup);         // Define what happens when a client requests attention
}

void loop() {
  if (millis()%300000==0) {
    timeClient.update();                 // NTP updates are allowed no-more than every 60-seconds and the 'display_time' function independently updates the seconds
    epoch   = timeClient.getEpochTime(); // You can test moving time forward or back by adding or subtracting the required number of seconds 1-day = 24*60*60
    seconds = epoch % 60;
  }
  server.handleClient();   // Wait for a client to connect and when they do process their requests
}

//Program comes here under interrupt control every 1-sec
void display_time(){ // Note Ticker called routines cannot get a time update using timeclient.update()!
  epoch          = epoch + 1;
  UTC_hours      = epoch % 86400 / 3600;
  if (DST) epoch = epoch + 3600; 
  epoch = epoch + int(TimeZone * 3600);
  calcDate();
  epoch = epoch - int(TimeZone * 3600);
  if (DST) epoch = epoch - 3600; 
  DST = false; // Until calculated otherwise as follows:
  if (dstUK) { // For the UK DST begins at 0100 on the last Sunday in March and ends at 0200 on the last Sunday in October
    if ((current_month > 2 && current_month < 9 ) || 
        (current_month == 2 && ((current_day-dayOfWeek) >= 25) &&  dayOfWeek == 0 && hours >= 1) ||
        (current_month == 2 && ((current_day-dayOfWeek) >= 25) &&  dayOfWeek >  0) || 
        (current_month == 9 && ((current_day-dayOfWeek) <= 24) &&  dayOfWeek >  0) ||
        (current_month == 9 && ((current_day-dayOfWeek) <= 24) && (dayOfWeek == 0 && hours <=2))) DST = true;   
  }
  if (dstUSA) { // For the USA DST begins at 0200 on the second Sunday of March and ends on the first Sunday of November at 0200 
    if ((current_month  > 2 &&   current_month < 10 ) || 
        (current_month == 2 &&  (current_day-dayOfWeek) >= 12 &&  dayOfWeek == 0 && hours > 2) ||
        (current_month == 2 &&  (current_day-dayOfWeek) >= 12 &&  dayOfWeek >  0) || 
        (current_month == 10 && (current_day-dayOfWeek) <= 5  &&  dayOfWeek >  0) ||
        (current_month == 10 && (current_day-dayOfWeek) <= 5  && (dayOfWeek == 0 && hours < 2))) DST = true;   
  }
  if (dstAUS) { // For Australia DST begins on the first Sunday of October @ 0200 and ends the first Sunday in April @ 0200
    if ((current_month  > 9 || current_month < 3 ) || 
        (current_month == 3 && (current_day-dayOfWeek) <= 0  &&  dayOfWeek == 0 && hours < 3) ||
        (current_month == 3 && (current_day-dayOfWeek) <= 0  &&  dayOfWeek >  0) || 
        (current_month == 9 && (current_day-dayOfWeek) <= 29 &&  dayOfWeek >  0) ||
        (current_month == 9 && (current_day-dayOfWeek) <= 29 && (dayOfWeek == 0 && hours > 1))) DST = true;  
  }
  display.clearDisplay();
  display.setTextSize(1); // Display size is 10 x 6 characters when set to size=1 where font size is 6x6 pixels
  display.setCursor((64 - day_of_week[dayOfWeek].length()*6)/2,0); // Display size is 10 characters per line and 6 rows when set to size=1 when font size is 6x6 pixels
  display.println(day_of_week[dayOfWeek]); // Extract and print day of week
  
  display.setCursor(4,8); // centre date display
  display.print((current_day<10?"0":"")+String(current_day)+"-"+month_of_year[current_month]+"-"+String(current_year).substring(2)); // print Day-Month-Year
  display.setTextSize(2);  // Increase text size for time display
  display.setCursor(2,17); // Move dayOfWeekn a litle and remember location is in pixels not lines!
  if (AMPM) {
    if (hours%12==0) display.print("12");
    else
    {
      if (hours%12 < 10) display.print(" " + String(hours%12));else display.print(String(hours%12));
    } 
  } else if ((hours < 10) || (hours%24 == 0)) display.print("0" + String(hours%24)); else display.print(String(hours%24));

  display.print((minutes < 10) ? ":0" + String(minutes%60) : ":"+String(minutes%60));
  display.fillRect(0,35,63,47,BLACK); // Clear the portion of screen that displays seconds (nn) ready for next update
  display.setTextSize(1);   // Reduce text size to fit in the remaining screen area
 
  if (AMPM) display.setCursor(10,32); else display.setCursor(19,32); // Move dayOfWeekn to a position that can accomodate seconds display
  display.print("("); display.print((seconds < 10) ? "0" + String(seconds): String(seconds)); display.print(")");
  if (AMPM){ if (hours%24 < 12) display.print("AM"); else display.print("PM");}
  display.setCursor(12,41); // Move dayOfWeek to a position that can accomodate seconds display
  if (DST)    display.print("DST-"); else display.print("UTC-"); 
  if (dstUK)  display.print("UK");
  if (dstUSA) display.print("USA");
  if (dstAUS) display.print("AUS"); 
  if (hours == alarm_HR && minutes == alarm_MIN && alarmed) {
   if (alarm_triggered) {
     alarm_triggered = false;
     display.setCursor(13,32); display.print("*");
     display.setCursor(42,32); display.print("*");
     pinMode(output_pin, OUTPUT);   // Set D1 pin LOW when alarm goes off
     digitalWrite(output_pin,LOW);
   } 
   else
   {
     alarm_triggered = true;
     display.setCursor(13,32); display.print(" ");
     display.setCursor(42,32); display.print(" ");
     pinMode(output_pin, OUTPUT);   // Set D1 pin HIGH when alarm goes on
     digitalWrite(output_pin,HIGH);
    }
  }
  if (alarmed) {
    display.setCursor(0,32); display.print("A");
    display.setCursor(0,41); display.print("S");
  } else
  {
    display.fillRect(0,32,10,20,BLACK);
  }
  display.display(); //Update the screen
}

void calcDate(){
  uint32_t cDyear_day;
  bool     leapYear;
  seconds      = epoch;
  minutes      = seconds / 60; /* calculate minutes */
  seconds     -= minutes * 60; /* calcualte seconds */
  hours        = minutes / 60; /* calculate hours */
  minutes     -= hours   * 60;
  current_day  = hours   / 24; /* calculate days */
  hours       -= current_day * 24;
  current_year = 1970;         /* Unix time starts in 1970 on a Thursday */
  dayOfWeek    = 4;
  while(1){
    leapYear   = (current_year % 4 == 0 && (current_year % 100 != 0 || current_year % 400 == 0));
    uint16_t daysInYear = leapYear ? 366 : 365;
    if (current_day >= daysInYear) {
      dayOfWeek += leapYear ? 2 : 1;
      current_day   -= daysInYear;
      if (dayOfWeek >= 7) dayOfWeek -= 7;
      ++current_year;
    }
    else
    {
      cDyear_day  = current_day;
      dayOfWeek  += current_day;
      dayOfWeek  %= 7;
      /* calculate the month and day */
      static const uint8_t daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
      for(current_month = 0; current_month < 12; ++current_month) {
        uint8_t dim = daysInMonth[current_month];
        if (current_month == 1 && leapYear) +dim; /* add a day to feburary if this is a leap year */
        if (current_day >= dim) current_day -= dim;
        else break;
      }
      break;
    }
  }
  if (!leapYear) current_day += 1;
}

void NTP_Clock_home_page () { 
  update_webpage();
  server.send(200, "text/html", webpage); 
} 

// A short method of adding the same web page header to some text
void append_webpage_header() {
  // webpage is a global variable
  webpage = ""; // A blank string variable to hold the web page
  webpage += "<!DOCTYPE html><html><head><title>ESP8266 NTP Clock</title>";
  webpage += "<style>";
  webpage += "#header  {background-color:#6A6AE2; font-family:tahoma; width:1280px; padding:10px; color:white; text-align:center; }";
  webpage += "#section {background-color:#E6E6FA; font-family:tahoma; width:1280px; padding:10px; color_blue;  font-size:22px; text-align:center;}";
  webpage += "#footer  {background-color:#6A6AE2; font-family:tahoma; width:1280px; padding:10px; color:white; font-size:14px; clear:both; text-align:center;}";
  webpage += "</style></head><body>";
}

void update_webpage(){
  append_webpage_header();
  webpage += "<div id=\"header\"><h1>NTP OLED Clock Setup "+clock_version+"</h1></div>";
  webpage += "<div id=\"section\"><h2>Time Zone, AM-PM and DST Mode Selection</h2>"; 
  webpage += "[AM-PM Mode: ";
  if (AMPM) webpage += "ON]"; else webpage += "OFF]";
  webpage += "&nbsp&nbsp&nbsp";
  webpage += "[DST Mode: ";
  if (dstUK) webpage += "UK]"; else if (dstUSA) webpage += "USA]"; else if (dstAUS) webpage += "AUS]";
  webpage += "&nbsp&nbsp&nbsp";
  webpage += "[Time Zone: ";
  if (TimeZone > -1 && TimeZone < 0) webpage += "-";
  webpage += String(int(TimeZone)) + ":";
  if (abs(round(60*(TimeZone - int(TimeZone)))) == 0) webpage += "00]"; else webpage += "30]";
  webpage += "&nbsp&nbsp&nbsp";
  webpage += "[Alarm Time: ";
  if (alarm_HR < 10) webpage += "0";
  webpage += String(alarm_HR) + ":";
  if (alarm_MIN < 10) webpage += "0";
  webpage += String(alarm_MIN) + "]";  
  webpage += "&nbsp&nbsp&nbsp";
  webpage += "[Alarm: ";
  if (alarmed) webpage += "ON]"; else webpage += "OFF]";
  
  webpage += "<p><a href=\"AMPM_ONOFF\">AM-PM Mode ON/OFF</a></p>";
  
  webpage += "<a href=\"TIMEZONE_PLUS\">Time Zone Plus 30-mins</a>";
  webpage += "&nbsp&nbsp&nbsp&nbsp";
  webpage += "<a href=\"TIMEZONE_MINUS\">Time Zone Minus 30-mins</a>";

  webpage += "<p><a href=\"DST_UK\">DST UK  Mode</a>";
  webpage += "&nbsp&nbsp&nbsp&nbsp";
  webpage += "<a href=\"DST_USA\">DST USA Mode</a>";
  webpage += "&nbsp&nbsp&nbsp&nbsp";
  webpage += "<a href=\"DST_AUS\">DST AUS Mode</a></p>";

  webpage += "<p><a href=\"ALARM_PLUS\">Alarm Time +</a>";
  webpage += "&nbsp&nbsp&nbsp&nbsp";
  webpage += "<a href=\"ALARM_MINUS\">Alarm Time -</a>";
  webpage += "&nbsp&nbsp&nbsp&nbsp";
  webpage += "<a href=\"ALARM_RESET\">Alarm Reset</a>";
  webpage += "&nbsp&nbsp&nbsp&nbsp";
  webpage += "<a href=\"ALARM_ONOFF\">Alarm ON/OFF</a></p>";
      
  webpage += "<p><a href=\"RESET_VALUES\">Reset clock parameters</a></p>";
  webpage += "<p><a href=\"EXIT_SETUP\">Run the clock</a></p>";
  
  webpage += "</div>"; 
  webpage += "<div id=\"footer\">Copyright &copy; D L Bird 2016</div>"; 
}

void timezone_plus() {
  TimeZone += 0.5;
  EEPROM.put(0,TimeZone);
  EEPROM.put(5,0); // set_status to a known value
  EEPROM.commit();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void timezone_minus() {
  TimeZone -= 0.5;
  EEPROM.put(0,TimeZone);
  EEPROM.put(5,0); // set_status to a known value
  EEPROM.commit();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void AMPM_ONOFF() {
  if (AMPM) AMPM = false; else AMPM = true;
  EEPROM.put(4,AMPM);
  EEPROM.commit();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void dst_UK() {
  dstUK  = true;
  dstUSA = false;
  dstAUS = false;
  EEPROM.put(6,dstUK);
  EEPROM.put(7,dstUSA);
  EEPROM.put(8,dstAUS);
  EEPROM.commit();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void dst_USA() {
  dstUK  = false;
  dstUSA = true;
  dstAUS = false;
  EEPROM.put(6,dstUK);
  EEPROM.put(7,dstUSA);
  EEPROM.put(8,dstAUS);
  EEPROM.commit();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void dst_AUS() {
  dstUK  = false;
  dstUSA = false;
  dstAUS = true;
  EEPROM.put(6,dstUK);
  EEPROM.put(7,dstUSA);
  EEPROM.put(8,dstAUS);
  EEPROM.commit();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void alarm_PLUS(){
  alarm_MIN += 2;
  if (alarm_MIN >= 60) {
    alarm_MIN = 00;
    alarm_HR += 1;
  }
  EEPROM.put(9,alarm_HR);
  EEPROM.put(10,alarm_MIN);
  EEPROM.commit();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void alarm_MINUS(){
  alarm_MIN -= 2;
  if (alarm_MIN = 00 || alarm_MIN >59) { // It's an unsigned byte! so 0 - 1 = 255
    alarm_MIN = 59;
    alarm_HR -= 1;
  }
  EEPROM.put(9,alarm_HR);
  EEPROM.put(10,alarm_MIN);
  EEPROM.commit();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void alarm_RESET(){
  alarm_HR  = 06;
  alarm_MIN = 30; // Default alarm time of 06:30
  alarmed   = false;
  alarm_triggered = false;
  EEPROM.put(9,alarm_HR);
  EEPROM.put(10,alarm_MIN);
  EEPROM.commit();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void alarm_ONOFF() {
  if (alarmed) alarmed = false; else alarmed = true;
  EEPROM.put(11,alarmed);
  EEPROM.commit();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void reset_values() {
  AMPM       = false;
  TimeZone   = 0;
  set_status = 0;
  dstUK      = true;
  dstUSA     = false;
  dstAUS     = false;
  alarm_HR   = 06;
  alarm_MIN  = 30;
  alarmed    = false;
  EEPROM.put(0,TimeZone);
  EEPROM.put(4,AMPM);
  EEPROM.put(5,set_status);
  EEPROM.put(6,dstUK);
  EEPROM.put(7,dstUSA);
  EEPROM.put(8,dstAUS);
  EEPROM.put(9,alarm_HR);
  EEPROM.put(10,alarm_MIN);
  EEPROM.put(11,alarmed);
  EEPROM.commit();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void exit_setup() {
  screen_update.attach_ms(1000, display_time); // Call display routine every 1-sec. Tick units are 1.024mS
  timeClient.update();                 // NTP updates are allowed no-more than every 60-seconds and the 'display_time' function independently updates the seconds
  epoch   = timeClient.getEpochTime(); // You can test moving time forward or back by adding or subtracting the required number of seconds 1-day = 24*60*60
  seconds = 60; // Set to trigger first time update
  seconds = epoch % 60;
  epoch = timeClient.getEpochTime();
  display_time();
  server.send(200, "text/html", webpage);
}

void display_ip(){
  // Print the IP address
  display.println("Connect:");
  display.println("http://");
  display.print(WiFi.localIP());
  display.print("/");
  display.display();
  delay(2500);
}



