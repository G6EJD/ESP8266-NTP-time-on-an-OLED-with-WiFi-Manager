//
// ESP8266 Clock displayed on an OLED shield (64x48) using the Network Time Protocol to update every minute
// (c) D L Bird 2016
//
#include <NTPClient.h> https://github.com/arduino-libraries/NTPClient
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

int epoch,local_epoch,current_year,current_month,current_day,DoW,hours,UTC_hours,minutes,seconds;
bool DST = false; // DayLightSaving on/off indicator

String day_of_week[7]    = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};  // Sunday is DoW 0
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
  timeClient.update();                  // NTP updates are allowed no-more than every 60-seconds and the 'display_time' function independently updates the seconds
  epoch   = timeClient.getEpochTime(); // You can test moving time forward or back by adding or subtracting the required number of seconds 1-day = 24*60*60
  seconds = 60; // Set to trigger first time update
  seconds = epoch % 60;
}

void loop() {
  if (millis()%300000==0) {
    timeClient.update();                 // NTP updates are allowed no-more than every 60-seconds and the 'display_time' function independently updates the seconds
    epoch   = timeClient.getEpochTime(); // You can test moving time forward or back by adding or subtracting the required number of seconds 1-day = 24*60*60
    seconds = epoch % 60;
  }
  delay(1000); // Essentially do nothing in the main loop, or do something else providing 'timeClient.getEpochTime()' is called every minute, the time client updates Epoch from the NTP Server
}

void display_time(){ // Note Ticker called routines cannot get a time update using timeclient.update()!
  // calculate all date-time information
  if (DST == true) local_epoch = epoch + 3600; else local_epoch = epoch;
  epoch         = epoch + 1;
  current_year  = 1970+local_epoch/(365.25*24*60*60);
  current_month = int(((1970+local_epoch/(365.25*24*60*60)-current_year)*12))%12; // Months are numbered 0-11 where 0 = Jan
  current_day   = local_epoch / 86400;
  hours         = (local_epoch % 86400) / 3600;
  UTC_hours     = (epoch % 86400) / 3600;
  minutes       = (local_epoch % 3600)  / 60;
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
  display.print((hours < 10)||(hours%24==0) ? "0" + String(hours%24) : String(hours%24));
  display.print((minutes < 10) ? ":0" + String(minutes%60) : ":"+String(minutes%60));
  display.fillRect(0,35,63,47,BLACK); // Clear the portion of screen that displays seconds (nn) ready for next update
  display.setTextSize(1);   // Reduce text size to fit in the remaining screen area
  display.setCursor(19,32); // Move down to a position that can accomodate seconds display
  display.print(((seconds%60) < 10 || seconds%60==0) ? "(0" + String(seconds%60)+")" : "("+String(seconds%60)+")");
  display.setCursor(22,41); // Move down to a position that can accomodate seconds display
  display.print(DST ? "DST": "UTC"); 
  display.display(); //Implement the new screen contents 
}

/*
 * 
 * 
 * if ((month  >  3 && month < 11 ) || 
      (month ==  3 && day_of_month >= 8 && day_of_week == 0 && hour >= 2) ||  // DST starts 2nd Sunday of March;  2am
      (month == 11 && day_of_month <  8 && day_of_week >  0) ||
      (month == 11 && day_of_month <  8 && day_of_week == 0 && hour < 2)) {   // DST ends 1st Sunday of November; 2am
    timezone++;
  }

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
