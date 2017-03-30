//
// ESP8266 Clock displayed on an OLED shield (64x48) using the Network Time Protocol to update every minute
// (c) D L Bird 2016
//
String   clock_version = "12.0";
#include <NTPClient.h> 
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <Ticker.h>       // Core library
#include <EEPROM.h>

#include <DNSServer.h>
#include <WiFiManager.h>  //https://github.com/tzapu/WiFiManager

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h> // Only the version via the WeMos site
#define  OLED_RESET 0         // GPIO0
Adafruit_SSD1306 display(OLED_RESET);

String day_of_week[7]    = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};  // Sunday is dayOfWeek 0
String month_of_year[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"}; // January is month 0
String webpage = "";

WiFiClient client;
WiFiUDP time_udp;
Ticker  screen_update;

// You can specify the time server source and time-zone offset in milli-seconds.
float TimeZone;
bool  AMPM = false;
int   epoch,local_epoch,current_year,current_month,current_day,dayOfWeek,hours,UTC_hours,minutes,seconds;
byte  set_status;
bool  DST = false, dstUK, dstUSA, dstAUS, output_pin_state; // DayLightSaving on/off and selected zone indicators
#define output_pin D1

// If your country is ahead of UTC by 2-hours use 2, or behind UTC by 2-hours use -2
// If your country is 1-hour 30-mins ahead of UTC use 1.5, so use decimal hours
// Change your time server to a local one, but they all return UTC!
NTPClient timeClient(time_udp);
//Could use the following, but don't use the programme DST option!
//NTPClient timeClient(time_udp,"uk.pool.time.org", TimeZone * 3600, 60000); // (logical name, address of server, time-zone offset in seconds, refresh-rate in mSec)

ESP8266WebServer server(80);  // Set the port you wish to use, a browser default is 80, but any port can be used, if you set it to 5555 then connect with http://nn.nn.nn.nn:5555/
  
void setup(){
  Serial.begin(115200);
  EEPROM.begin(9);
  //  0 - TimeZone 4-bytes for floating point
  //  1
  //  2
  //  3
  //  4 - AMPM Mode true/false
  //  5 - set status 
  //  6 - dst_UK  true/false
  //  7 - dst_USA true/false
  //  8 - dst_AUS true/false
  EEPROM.get(0,TimeZone);
  delay(500);
  AMPM = EEPROM.read(4);
  delay(500);
  // A simple test of first CPU usage, if first time run the probability of these locations equating to 0 is low, so initialise the timezone
  if (set_status != 0 ) TimeZone = 0;
  set_status = EEPROM.read(5); 
  delay(500);
  dstUK  = EEPROM.read(6);
  delay(500);
  dstUSA = EEPROM.read(7);
  delay(500);
  dstAUS = EEPROM.read(8);
  output_pin_state = EEPROM.read(9);
  delay(500);
  EEPROM.commit();
  EEPROM.end();
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
  display.print("Sync...");
  Serial.print("Use this URL to connect: ");
  Serial.print("http://"); Serial.print(WiFi.localIP()); Serial.println("/");
  display_ip();
  display.display(); // Update screen
  screen_update.attach_ms(1000, display_time); // Call display update routine every 1-sec. Tick units are 1.024mS adjust as required, I've used 1000mS, but should be 976 x 1.024 = 1-sec
  timeClient.update();                         // NTP updates are allowed no-more than every 60-seconds and the 'display_time' function independently updates the seconds
  epoch = timeClient.getEpochTime();           // You can test moving time forward or back by adding or subtracting the required number of seconds 1-day = 24*60*60
  epoch =  epoch + 3600 * TimeZone;
  server.begin();  // Start the webserver
  Serial.println("Webserver started...");

  server.on("/", NTP_Clock_home_page);  
  server.on("/AMPM_ONOFF", AMPM_ONOFF);         // Define what happens when a client requests attention
  server.on("/TIMEZONE_PLUS", timezone_plus);   // Define what happens when a client requests attention
  server.on("/TIMEZONE_MINUS", timezone_minus); // Define what happens when a client requests attention
  server.on("/DST_UK",  dst_UK);                // Define what happens when a client requests attention
  server.on("/DST_USA", dst_USA);               // Define what happens when a client requests attention
  server.on("/DST_AUS", dst_AUS);               // Define what happens when a client requests attention
  server.on("/OUTPUT_TOGGLE", toggle_output);   // Define what happens when a client requests attention
  server.on("/RESET_VALUES", reset_values);     // Define what happens when a client requests attention
  server.on("/EXIT_SETUP", exit_setup);         // Define what happens when a client requests attention
}

void loop() {
  if (millis()%300000==0) {
    timeClient.update();                 // NTP updates are allowed no-more than every 60-seconds and the 'display_time' function independently updates the seconds
    epoch   = timeClient.getEpochTime(); // You can test moving time forward or back by adding or subtracting the required number of seconds 1-day = 24*60*60
    epoch =  epoch + 3600 * TimeZone;
    seconds = epoch % 60;
  }
  server.handleClient();   // Wait for a client to connect and when they do process their requests
}

void calcDate(){
  bool     leapYear;
  uint32_t cDyear_day;
  seconds      = epoch;
  minutes      = seconds / 60; /* calculate minutes */
  seconds     -= minutes * 60; /* calculate seconds */
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

void display_time(){ // Note Ticker called routines cannot get a time update using timeclient.update()!
  epoch          = epoch + 1;
  UTC_hours      = epoch % 86400 / 3600;
  if (DST) epoch = epoch + 3600; 
  calcDate();
  if (DST) epoch = epoch - 3600; 
  DST = false; // Until calculated otherwise as follows:
  if (dstUK) { // For the UK DST begins at 0100 on the last Sunday in March and ends at 0200 on the last Sunday in October
    if ((current_month > 2 && current_month < 9 ) || 
        (current_month == 2  && ((current_day-dayOfWeek) >= 25) &&  dayOfWeek == 0 && hours >= 1) ||
        (current_month == 2  && ((current_day-dayOfWeek) >= 25) &&  dayOfWeek >  0) || 
        (current_month == 9  && ((current_day-dayOfWeek) <= 24) &&  dayOfWeek >  0) ||
        (current_month == 9  && ((current_day-dayOfWeek) <= 24) && (dayOfWeek == 0 && hours <= 1))) DST = true;   
  }
  if (dstUSA) { // For the USA DST begins at 0200 on the second Sunday of March and ends on the first Sunday of November at 0200 
    if ((current_month > 2 && current_month < 10 ) || 
        (current_month == 2  && ((current_day-dayOfWeek) >= 12) &&  dayOfWeek == 0 && hours >= 2) ||
        (current_month == 2  && ((current_day-dayOfWeek) >= 12) &&  dayOfWeek > 0) ||
        (current_month == 10 && ((current_day-dayOfWeek) <= 4)  &&  dayOfWeek > 0) ||
        (current_month == 10 && ((current_day-dayOfWeek) <= 4)  && (dayOfWeek == 0 && hours <= 1))) DST = true;  
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
      if (hours%12 < 10) display.print(" " + String(abs(hours%12)));else display.print(String(abs(hours%12)));
  } else if ((hours < 10) || (hours%24 == 0)) display.print("0" + String(hours%24)); else display.print(String(hours%24));
  display.print((minutes < 10) ? ":0" + String(minutes%60) : ":"+String(minutes%60));
  display.fillRect(0,35,63,47,BLACK); // Clear the portion of screen that displays seconds (nn) ready for next update
  display.setTextSize(1);   // Reduce text size to fit in the remaining screen area
  if (AMPM) display.setCursor(11,32); else display.setCursor(19,32); // Move dayOfWeek to a position that can accomodate seconds display
  display.print("("); display.print((seconds < 10) ? "0" + String(seconds): String(seconds)); display.print(")");
  if (AMPM){ if (hours%24 < 12) display.print("AM"); else display.print("PM");}
  display.setCursor(10,41); // Move dayOfWeekn to a position that can accomodate seconds display
  if (DST)    display.print("DST-"); else display.print("UTC-"); 
  if (dstUK == true)  display.print("UK");
  if (dstUSA == true) display.print("USA");
  if (dstAUS == true) display.print("AUS"); 
  display.display(); //Update the screen
}

void NTP_Clock_home_page () { 
  update_webpage();
  server.send(200, "text/html", webpage); 
} 

// A short methoid of adding the same web page header to some text
void append_webpage_header() {
  // webpage is a global variable
  webpage = ""; // A blank string variable to hold the web page
  webpage += "<!DOCTYPE html><html><head><title>ESP8266 NTP Clock</title>";
  webpage += "<style>";
  webpage += "#header  {background-color:#6A6AE2; font-family:tahoma; width:1024px; padding:10px; color:white; text-align:center; }";
  webpage += "#section {background-color:#E6E6FA; font-family:tahoma; width:1024px; padding:10px; color_blue;  font-size:22px; text-align:center;}";
  webpage += "#footer  {background-color:#6A6AE2; font-family:tahoma; width:1024px; padding:10px; color:white; font-size:14px; clear:both; text-align:center;}";
  webpage += "</style></head><body>";
}

void update_webpage(){
  append_webpage_header();
  webpage += "<div id=\"header\"><h1>NTP OLED Clock Setup "+clock_version+"</h1></div>";
  webpage += "<div id=\"section\"><h2>Time Zone, AM-PM and DST Mode Selection</h2>"; 
  if (AMPM) webpage += "[AM-PM Mode is ON]"; else webpage += "[AM-PM Mode is OFF]";
  webpage += "&nbsp;&nbsp;";
  webpage += "[DST Mode is ";
  if (dstUK) webpage += "UK]"; else if (dstUSA) webpage += "USA]"; else if (dstAUS) webpage += "AUS]";

  webpage += "&nbsp;&nbsp;";
  webpage += "[Output is ";
  if (output_pin_state) webpage += "ON]"; else webpage += "OFF]";
  
  webpage += "&nbsp;&nbsp";
  webpage += "[Current Time Zone is : ";
  if (TimeZone > -1 && TimeZone < 0) webpage += "-";
  webpage += String(int(TimeZone)) + ":";
  if (abs(round(60*(TimeZone - int(TimeZone)))) == 0) webpage += "00]"; else webpage += "30]";
  webpage += "<p><a href=\"AMPM_ONOFF\">AM-PM Mode ON/OFF</a></p>";
  
  webpage += "<p><a href=\"TIMEZONE_PLUS\">Time Zone Plus 30-mins</a>&nbsp;&nbsp";
  webpage += "<a href=\"TIMEZONE_MINUS\">Time Zone Minus 30-mins</a></p>";

  webpage += "<p><a href=\"DST_UK\">DST UK  Mode</a>&nbsp;&nbsp;";
  webpage += "<a href=\"DST_USA\">DST USA Mode</a>&nbsp;&nbsp;";
  webpage += "<a href=\"DST_AUS\">DST AUS Mode</a></p>";

  webpage += "<p><a href=\"OUTPUT_TOGGLE\">Output ON/OFF</a></p>";

  webpage += "<p><a href=\"RESET_VALUES\">Reset clock parameters</a></p>";
  webpage += "<p><a href=\"EXIT_SETUP\">Run the clock</a></p>";
  
  webpage += "</div>"; 
  webpage += "<div id=\"footer\">Copyright &copy; D L Bird 2016</div></body></html>"; 
}

void timezone_plus() {
  EEPROM.begin(9);
  TimeZone += 0.5;
  EEPROM.put(0,TimeZone);
  delay(500);
  EEPROM.put(5,0); // set_status to a known value
  delay(500);
  EEPROM.end();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void timezone_minus() {
  EEPROM.begin(9);
  TimeZone -= 0.5;
  EEPROM.put(0,TimeZone);
  delay(500);
  EEPROM.put(5,0); // set_status to a known value
  delay(500);
  EEPROM.end();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void AMPM_ONOFF() {
  EEPROM.begin(9);
  if (AMPM) AMPM = false; else AMPM = true;
  EEPROM.put(4,AMPM);
  delay(500);
  EEPROM.end();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void toggle_output() {
  EEPROM.begin(9);
  if (output_pin_state) output_pin_state = false; else output_pin_state = true;
  if (output_pin_state) { // if true then make it HIGH
    pinMode(output_pin, OUTPUT);
    digitalWrite(output_pin,HIGH);
  }
  else {
    pinMode(output_pin, OUTPUT);
    digitalWrite(output_pin,LOW);
  }
  EEPROM.put(9,output_pin_state);
  delay(500);
  EEPROM.end();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void dst_UK() {
  EEPROM.begin(9);
  dstUK  = true;
  dstUSA = false;
  dstAUS = false;
  EEPROM.put(6,dstUK);
  delay(500);
  EEPROM.put(7,dstUSA);
  delay(500);
  EEPROM.put(8,dstAUS);
  delay(500);
  EEPROM.end();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void dst_USA() {
  EEPROM.begin(9);
  dstUK  = false;
  dstUSA = true;
  dstAUS = false;
  EEPROM.put(6,dstUK);
  delay(500);
  EEPROM.put(7,dstUSA);
  delay(500);
  EEPROM.put(8,dstAUS);
  delay(500);
  EEPROM.end();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void dst_AUS() {
  EEPROM.begin(9);
  dstUK  = false;
  dstUSA = false;
  dstAUS = true;
  EEPROM.put(6,dstUK);
  delay(500);
  EEPROM.put(7,dstUSA);
  delay(500);
  EEPROM.put(8,dstAUS);
  delay(500);
  EEPROM.end();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void reset_values() {
  EEPROM.begin(9);
  AMPM             = false;
  TimeZone         = 0;
  set_status       = 0;
  dstUK            = true;
  dstUSA           = false;
  dstAUS           = false;
  output_pin_state = false;
  EEPROM.put(0,TimeZone);
  delay(500);
  EEPROM.put(4,AMPM);
  delay(500);
  EEPROM.put(5,set_status);
  delay(500);
  EEPROM.put(6,dstUK);
  delay(500);
  EEPROM.put(7,dstUSA);
  delay(500);
  EEPROM.put(8,dstAUS);
  delay(500);
  EEPROM.put(9,output_pin_state);
  delay(500);
  EEPROM.end();
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
  display.println(WiFi.localIP());
  display.println("/");
  display.display();
  delay(2500);
}



