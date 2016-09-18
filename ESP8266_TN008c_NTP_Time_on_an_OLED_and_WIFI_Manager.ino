//
// ESP8266 Clock displayed on an OLED shield (64x48) using the Network Time Protocol to update every minute
// (c) D L Bird 2016
//
#include <NTPClient.h> // https://github.com/gmag11/NtpClient
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <Ticker.h>    // Core library

#include <DNSServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h> // SCL = GPIO5 and SDA = GPIO4
#define  OLED_RESET 0  // GPIO0
Adafruit_SSD1306 display(OLED_RESET);

int epoch,local_epoch,current_year,current_month,current_day,DoW,hours,minutes,seconds;
bool DST = false; // DayLightSaving on/off indicator

String day_of_week[7]    = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};  // Sunday is Dow 0
String month_of_year[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"}; // January is month 0

WiFiClient client;
WiFiUDP time_udp;
Ticker  screen_update;

// You can specify the time server source and time-zone offset in milli-seconds.
#define TimeZone 0
// e.g. if your country is ahead of UTC by 2-hours use 2, behind UTC by 2-hours use -2
// if your country is 1-hour 15-mins ahead of UTC use 1.25
NTPClient timeClient(time_udp,"time.nist.gov", TimeZone * 3600, 60000); // (logical name, address of server, time-zone offset in seconds, refresh-rate in mSec)

void setup(){
  Serial.begin(115200);
  //------------------------------
  //WiFiManager intialisation. Once completed there is no need to retain it
  WiFiManager wifiManager;
  
  // Use the following command to reset WiFi settings, uncomment if required then connect your PC to the wireless access point called 'ESP8266_AP'
  // A new OOB ESP8266 will have no credentials, so will connect and not need this to be uncommented and compiled in, a used one will, try it to see how it works
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
  timeClient.begin(); // Start the NTP service for time 
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.print("Synching..");
  display.display(); // Update screen
  screen_update.attach_ms(1000, display_time); // Call display routine every 1-sec. Tick units are 1.024mS
  seconds = 60; // Set to trigger first time update
}

void loop() {
  if (seconds > 59) {
    timeClient.update();                  // NTP updates are allowed no-more than every 60-seconds and the 'display_time' function independently updates the seconds
    epoch   = timeClient.getEpochTime();  // You can test moving time forward or back by adding or subtracting the required number of seconds 1-day = 24*60*60
    seconds = epoch % 60;
  }
  delay(1000); // Essentially do nothing in the main loop, or do something else providing 'timeClient.getEpochTime()' is called every minute, the time client updates Epoch from the NTP Server
}

void display_time(){ // Note Ticker called routines cannot get a time update using timeclient.update()!
  // calculate all date-time information
  if (DST = true) local_epoch = epoch + 3600; else local_epoch = epoch;
  current_year  = 1970+local_epoch/(365.25*24*60*60);
  current_month = int(((1970+local_epoch/(365.25*24*60*60)-current_year)*12))%12; // Months are numbered 0-11 where 0 = Jan
  current_day   = local_epoch / 86400;
  DoW           = (current_day+4) % 7; // Adjust DoW so that Sun = 0
  hours         = (local_epoch % 86400) / 3600;
  minutes       = (local_epoch % 3600)  / 60;
  seconds       = seconds + 1;
  if (seconds > 59) minutes += 1; // improves display transition around minute increase
  int tm_day    = current_day; // Day in this month
  tm_day = tm_day - (((((tm_day * 4) + 2)/1461) * 1461) + 1) / 4; // in 2016 result of this is 116
  int leap = 0;
  if ((current_year % 4) == 0) {
    leap = 1;
    if (tm_day > (58 + leap)) {
      if (leap != 0) {tm_day = tm_day + 1;} else {tm_day = tm_day + 2;}
    }
  }
  current_day = tm_day + 1 - ((((tm_day * 12) + 6)/367 * 367) + 5)/12; // Returns e.g. 14 (of September)
  
  // Check day light saving rule. The UK uses 0100 on the last Sunday in March -> +1 hour forward and 0200 on the last Sunday in October -> -1 hour back
  // Time from the NTP server is always received in UTC
  // See comment below for US and Australian versions
  // ------------------------------------------------
  if ((current_month >= 2 ) && (current_month <= 9)){
    if (current_month > 2 && current_month < 9) DST = true;
    else
    {
      DST = false;
      if ((current_month == 2) && ((current_day-DoW)>24)) DST = true;
      if ((current_month == 9) && ((current_day-DoW)<24)) DST = true;
    }
  }
  else {
    DST = false;
  } 
  display.clearDisplay();
  display.setTextSize(1); // Display size is 10 x 6 characters when set to size=1 where font size is 6x6 pixels
  display.setCursor((64 - day_of_week[DoW].length()*6)/2,0); // Display size is 10 characters per line and 6 rows when set to size=1 when font size is 6x6 pixels
  display.println(day_of_week[DoW]); // Extract and print day of week
  
  display.setCursor(4,8); // centre date display
  if (current_day < 10) display.print("0"); // print a leading zero when day is less than 10
  display.println(String(current_day)+"-"+month_of_year[current_month]+"-"+String(current_year).substring(2)); // print Day-Month-Year
   
  display.setTextSize(2);  // Increase text size for time display
  display.setCursor(2,17); // Move down a litle and remember location is in pixels not lines!
  display.print((hours < 10)||(hours%24==0) ? "0" + String(hours%24) : String(hours%24));
  display.print(minutes < 10 ? ":0" + String(minutes%59) : ":"+String(minutes%59));
  display.fillRect(0,35,63,47,BLACK); // Clear the portion of screen that displays seconds (nn) ready for next update
  display.setTextSize(1);   // Reduce text size to fit in the remaining screen area
  display.setCursor(19,32); // Move down to a position that can accomodate seconds display
  display.print((seconds < 10 || seconds%60==0) ? "(0" + String(seconds)+")" : "("+String(seconds)+")");
  display.setCursor(22,41); // Move down to a position that can accomodate seconds display
  display.print(DST ? "DST": "UTC"); 
  display.display(); //Implement the new screen contents 
}

/*
  // For the USA DST begins on the second Sunday of March and ends on the first Sunday of November
  if ((current_month >= 2 ) && (current_month <= 10)){
    if (current_month > 2 && current_month < 10) {
      DST = true;
    }
    else
    { 
      DST = false;
      if (current_month == 2 && (current_day-DoW)>=6)){ // not perfect as hours have not been implemented
        DST = true;
      }
      if (current_month == 10 && (current_day-DoW)>=6)){ // not perfect as hours have not been implemented
        DST = true;
      }
    }
   }
  else {
    DST = false;
  }
  
  // For Australia DST begins on the first Sunday of October and ends on the first Sunday of April
  if (current_month > 3 || current_month < 9) {
     DST = true;
   }
   else
   {
     DST   = false;
     if ( (current_month == 3) && ((current_day-DoW)<9)){ // not perfect as hours have not been implemented first calc is for March second is October
       DST   = true;
     }
     if ( (current_month == 9) && ((current_day-DoW)<=2)){ // not perfect as hours have not been implemented first calc is for March second is October
       DST   = true;
     }
    }
  }
  else {
    DST = false;
  }
*/
