//
// ESP8266 Clock displayed on an OLED shield (64x48) using the Network Time Protocol to update every minute
// (c) D L Bird 2016
//
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

String day_of_week[7]    = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};  // Sunday is DoW 0
String month_of_year[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"}; // January is month 0
String webpage = "";

WiFiClient client;
WiFiUDP time_udp;
Ticker  screen_update;

// You can specify the time server source and time-zone offset in milli-seconds.
float  TimeZone;
bool   AMPM = false;
int  epoch,local_epoch,current_year,current_month,current_day,DoW,hours,UTC_hours,minutes,seconds;
byte set_status;
bool DST = false, dstUK, dstUSA, dstAUS; // DayLightSaving on/off and selected zone indicators

// If your country is ahead of UTC by 2-hours use 2, or behind UTC by 2-hours use -2
// If your country is 1-hour 30-mins ahead of UTC use 1.5, so use decimal hours
// Change your time server to a local one, but they all return UTC!
NTPClient timeClient(time_udp,"uk.pool.ntp.org", TimeZone * 3600, 60000); // (logical name, address of server, time-zone offset in seconds, refresh-rate in mSec)

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
  EEPROM.get(4,AMPM);
  // A simple test of first CPU usage, if first time run the probability of these locations equating to 0 is low, so initilaise the timezone
  if (set_status != 0 ) TimeZone = 0;
  EEPROM.get(5,set_status); 
  EEPROM.get(6,dstUK);
  EEPROM.get(7,dstUSA);
  EEPROM.get(8,dstAUS);

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
  Serial.println(AMPM);
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

void display_time(){ // Note Ticker called routines cannot get a time update using timeclient.update()!
  // calculate all date-time information
  if (DST == true) local_epoch = epoch + 3600; else local_epoch = epoch;
//  local_epoch  += TimeZone * 3600; 
  epoch         = epoch + 1;
  current_year  = 1970+local_epoch/(365.25*24*60*60);
  current_month = int(((1970+local_epoch/(365.25*24*60*60)-current_year)*12))%12; // Months are numbered 0-11 where 0 = Jan
  current_day   = local_epoch / 86400;
  hours         = local_epoch % 86400 / 3600;
  UTC_hours     = epoch % 86400 / 3600;
  minutes       = epoch % 3600 / 60;
  seconds       = epoch % 60;
  int tm_day    = current_day; // Day in this month adjusted for leap years
  tm_day = tm_day - (((((tm_day * 4) + 2)/1461) * 1461) + 1) / 4; // in 2016 result of this is 116
  int leap = 0;
  if ((current_year % 4) == 0) {
    leap = 1;
    if (tm_day > (58 + leap)) {
      if (leap != 0) {tm_day = tm_day + 1;} else {tm_day = tm_day + 2;}
    }
  }
  current_day = tm_day + 1 - ((((tm_day * 12) + 6)/367 * 367) + 5)/12; // Returns e.g. 14 (of September)
  // Now calculate Day of Week
  // WeekDay = (K + [2.6*month-0.2)]- 2C + Y + [Y/4] + [C/4]) MOD 7
  // where [] denotes an integer result
  // k is day (1 to 31)
  // m is month (1 = March, ..., 10 = December, 11 = Jan, 12 = Feb) Treat Jan & Feb as months of the preceding year
  // C is century (1987 has C = 19)
  // Y is year (1987 has Y = 87 except Y = 86 for Jan & Feb)
  // W is week day (0 = Sunday, ..., 6 = Saturday)   
  DoW = (current_day + int(2.6*((current_month+12-2)%12+1) - 0.2) - 2 * int(current_year/100) + current_year%100 + current_year%100/4 + int((current_year/100)/4))%7;
  DST = false; // Until calculated otherwise as follows:
  if (dstUK) {
    // ---------------------------------------------------------------------------------------------------------------------------------------------------
    // Check day light saving rule. The UK uses 0100 on the last Sunday in March -> +1 hour forward and 0200 on the last Sunday in October -> -1 hour back
    // Time from the NTP server is always received in UTC
    // See comment below for US and Australian versions
    // ------------------------------------------------
    // Turn DST ON
    if ((current_month  > 2 &&  current_month < 9 ) || 
        (current_month == 2 && (current_day-DoW) >= 25 && DoW == 0 && UTC_hours >= 0) ||
        (current_month == 2 && (current_day-DoW) >= 25 && DoW >  0)   || // DST starts 2nd Sunday of March;  2am
        (current_month == 9 && (current_day-DoW) <= 25 && DoW >  0) ||
        (current_month == 9 && (current_day-DoW) <= 25 || (DoW == 0 && UTC_hours < 2))
        && DST == false) DST = true;   // DST ends 1st Sunday of November; 2am
    // Turn DST OFF
    if ((current_month  < 2 &&  current_month > 9 ) || 
        (current_month == 2 && (current_day-DoW) <= 25 && DoW == 0 && UTC_hours <= 1) ||
        (current_month == 2 && (current_day-DoW) <= 25 && DoW == 0 ) || // DST starts 2nd Sunday of March;  2am
        (current_month == 9 && (current_day-DoW) >  25 && DoW > 0 )  ||
        (current_month == 9 && (current_day-DoW) >  25 && (DoW == 0 && (UTC_hours >= 1) && UTC_hours < 2) )
        && DST == true) DST = false;  // DST ends 1st Sunday of November; 2am
     // ------------------------------------------------
  }
  
  if (dstUSA) {
    // ---------------------------------------------------------------------------------------------------------------------------------------------------
    // For the USA DST begins on the second Sunday of March and ends on the first Sunday of November
    if ((current_month >= 2 ) && (current_month <= 10)){
      if (current_month > 2 && current_month < 10) { // if we are in April through October DST is on
        DST = true;
      }
      else
      { 
        if (current_month == 2 && (current_day-DoW)>=6){ // not perfect as hours have not been implemented
          DST = true;
        }
        if (current_month == 10 && (current_day-DoW)<=6){ // not perfect as hours have not been implemented
          DST = true;
        }
      }
    }
    else {
      DST = false;
    }
  }
  
  if (dstAUS) {
    // ---------------------------------------------------------------------------------------------------------------------------------------------------
    // For Australia DST begins on the first Sunday of October and ends on the first Sunday of April
    if (current_month > 3 || current_month < 9) DST = true;
    else
    {
      if ( (current_month == 3) && ((current_day-DoW)<9)){ // not perfect as hours have not been implemented first calc is for March second is October
        DST   = true;
      }
      if ( (current_month == 9) && ((current_day-DoW)<=2)) // not perfect as hours have not been implemented first calc is for March second is October
        DST   = true;
    }
  }
  
  if (seconds > 59) minutes = minutes + 1; 
  display.clearDisplay();
  display.setTextSize(1); // Display size is 10 x 6 characters when set to size=1 where font size is 6x6 pixels
  display.setCursor((64 - day_of_week[DoW].length()*6)/2,0); // Display size is 10 characters per line and 6 rows when set to size=1 when font size is 6x6 pixels
  display.println(day_of_week[DoW]); // Extract and print day of week
  
  display.setCursor(4,8); // centre date display
  if (current_day < 10) display.print("0"); // print a leading zero when day is less than 10
  display.println(String(current_day)+"-"+month_of_year[current_month]+"-"+String(current_year).substring(2)); // print Day-Month-Year
   
  display.setTextSize(2);  // Increase text size for time display
  display.setCursor(2,17); // Move down a litle and remember location is in pixels not lines!
  if (AMPM) display.print((hours%12 < 10)||(hours%12==0) ? " " + String(hours%12) : String(hours%12));
    else display.print((hours < 10)||(hours%24==0) ? "0" + String(hours%24) : String(hours%24));

  display.print((minutes < 10) ? ":0" + String(minutes%60) : ":"+String(minutes%60));
  display.fillRect(0,35,63,47,BLACK); // Clear the portion of screen that displays seconds (nn) ready for next update
  display.setTextSize(1);   // Reduce text size to fit in the remaining screen area
 
  if (AMPM) display.setCursor(10,32); else display.setCursor(19,32); // Move down to a position that can accomodate seconds display
  display.print(((seconds%60) < 10 || seconds%60==0) ? "(0" + String(seconds%60)+") " : "("+String(seconds%60)+") ");
  if (AMPM) {
    if ((hours%24 < 12)||(hours%12==0)) display.print("AM"); else display.print("PM");
  }
  display.setCursor(15,41); // Move down to a position that can accomodate seconds display
  display.print(DST ? "DST-": "UTC-"); 
  Serial.println(dstUK);
  Serial.println(dstUSA);
  Serial.println(dstAUS);
  if (dstUK) display.print("UK"); else if (dstUSA) display.print("USA"); else display.print("AUS"); 

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
  webpage += "<div id=\"header\"><h1>NTP OLED Clock Setup</h1></div>"; 
  webpage += "<div id=\"section\"><h2>Time Zone and AM-PM Mode Setting</h2>"; 
  if (AMPM) webpage += "[AM-PM Mode is ON]"; else webpage += "[AM-PM Mode is OFF]";
  webpage += "&nbsp&nbsp&nbsp";
  webpage += "[DST Mode is ";
  if (dstUK) webpage += "UK]"; else if (dstUSA) webpage += "USA]"; else if (dstAUS) webpage += "AUS]";
  
  webpage += "&nbsp&nbsp&nbsp";
  webpage += "[Current Time Zone is : ";
  if (TimeZone > -1 && TimeZone < 0) webpage += "-";
  webpage += String(int(TimeZone)) + ":";
  if (abs(round(60*(TimeZone - int(TimeZone)))) == 0) webpage += "00]"; else webpage += "30]";
  webpage += "<p><a href=\"AMPM_ONOFF\">AM-PM Mode ON/OFF</a></p>";
  
  webpage += "<p><a href=\"TIMEZONE_PLUS\">Time Zone Plus 30-mins</a></p>";
  webpage += "<p><a href=\"TIMEZONE_MINUS\">Time Zone Minus 30-mins</a></p>";

  webpage += "<p><a href=\"DST_UK\">DST UK  Mode</a></p>";
  webpage += "<p><a href=\"DST_USA\">DST USA Mode</a></p>";
  webpage += "<p><a href=\"DST_AUS\">DST AUS Mode</a></p>";

  webpage += "<p><a href=\"RESET_VALUES\">Reset clock parameters</a></p>";
  webpage += "<p><a href=\"EXIT_SETUP\">Exit set-up and run clock</a></p>";
  
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

void reset_values() {
  AMPM       = false;
  TimeZone   = 0;
  set_status = 0;
  dstUK      = true;
  dstUSA     = false;
  dstAUS     = false;
  EEPROM.put(0,TimeZone);
  EEPROM.put(4,AMPM);
  EEPROM.put(5,set_status);
  EEPROM.put(6,dstUK);
  EEPROM.put(7,dstUSA);
  EEPROM.put(8,dstAUS);
  EEPROM.commit();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void exit_setup() {
  server.close();
  server.stop();
  screen_update.attach_ms(1000, display_time); // Call display routine every 1-sec. Tick units are 1.024mS
  timeClient.update();                 // NTP updates are allowed no-more than every 60-seconds and the 'display_time' function independently updates the seconds
  epoch   = timeClient.getEpochTime(); // You can test moving time forward or back by adding or subtracting the required number of seconds 1-day = 24*60*60
  seconds = 60; // Set to trigger first time update
  seconds = epoch % 60;
  epoch = timeClient.getEpochTime();
  display_time();
}

void display_ip(){
  // Print the IP address
  display.println("Connect:");
  display.println("http://");
  display.println(WiFi.localIP());
  display.println("/");
  display.display();
  delay(5000);
}

