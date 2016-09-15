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

int epoch,current_year,current_month,current_day,DoW,hours,minutes,seconds;
bool DST = false; // DayLightSaving on/off indicator

String day_of_week[7]    = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};  // Sunday is Dow 0
String month_of_year[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"}; // January is month 0

WiFiClient client;
WiFiUDP time_udp;
Ticker  screen_update;

// You can specify the time server source and time-zone offset in milli-seconds.
NTPClient timeClient(time_udp,"time.nist.gov", 0, 60000); // No time offset from UTC and update every minute

void setup(){
  Serial.begin(115200);

  //WiFiManager intialisation. Once completed there is no need to retain it
  WiFiManager wifiManager;
  
  // Use the following command to reset WiFi settings, uncomment if required then connect your PC to the wireless access point called 'ESP8266_AP'
  // A new OOB ESP8266 will have no credentials, so will connect and not need this to be uncommented and compiled in, a used one will, try it to see how it works
  //wifiManager.resetSettings();
  // Next connect to http://192.168.4.1/ and follow instructions to make the WiFi connection

  // Set a timeout until configuration is turned off, useful to retry or go to sleep in n-seconds
  wifiManager.setTimeout(180);

  //fetches ssid and password and tries to connect, if connections succeeds it starts an access point with the name called "ESP8266_AP" and waits in a blocking loop for configuration
  if(!wifiManager.autoConnect("ESP8266_AP")) {
    Serial.println("failed to connect and timeout occurred");
    delay(3000);
    ESP.reset(); //reset and try again, or maybe put it to deep sleep
    delay(5000);
  }
  // At this stagem WiFi manager will have successfully connected to a network
  
  timeClient.begin(); // Start the NTP service for time 
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.print("Synching..");
  display.display();
  screen_update.attach_ms(986, display_time); // Call display routine every 1-sec. Tick units are 1.024mS
  seconds = 60;
}

void loop() {
  if (seconds > 59) {
    timeClient.update(); // NTP updates are allowed no-more than every 60-seconds and the 'display_time' function independently updates the seconds
    epoch   = timeClient.getEpochTime();
    seconds = epoch % 60;
  }
  delay(1000); // Essentially do nothing in the main loop, or do something else providing 'time.client.update()' is called every minute
}

void display_time(){ // Note Ticker called routines cannot get a time update using timeclient.update()!
  current_year  = 1970+epoch/(365.25*24*60*60);
  current_month = (1970+epoch/(365.25*24*60*60)-current_year)*12;
  current_month = current_month % 12; // Months are numbered 0-11 where 0 = Jan
  current_day   = epoch / 86400;
  DoW           = (current_day+4) % 7;
  hours         = (epoch % 86400L) / 3600;
  minutes       = (epoch % 3600) / 60;
  seconds       = seconds + 1;
  int tm_day    = current_day;
  int tm_year   = (((tm_day * 4) + 2)/1461) + 70;
  tm_day = tm_day - (((((tm_day * 4) + 2)/1461) * 1461) + 1) / 4;
  int leap = 0;
  if ((tm_year % 4) == 0) {
    leap = 1;
    if (tm_day > (58 + leap)) {
      if (leap != 0) {tm_day = tm_day + 1;} else {tm_day = tm_day + 2;}
    }
  }
  int tm_mon  = ((tm_day * 12) + 6)/367;
  int tm_mday = tm_day + 1 - ((tm_mon * 367) + 5)/12;
  current_day = tm_mday; // Returns e.g. 14 (of September)

  // Check day light saving rule. The UK uses 0100 on the last Sunday in March -> +1 hour forward and 0200 on the last Sunday in October -> -1 hour back
  // Time from the NTP server is always received in UTC
  if ((current_month >= 2 ) && (current_month <= 9)){
    if (current_month > 2 && current_month < 9) {
      DST   = true;
      hours = hours + 1;
    }
    else
    {
      if ( (current_day-DoW)>25 && (current_day-DoW)<25){ // not perfect as hours have not been implemented
      DST   = true;
      hours = hours + 1;
      }
    }
  }
  else {
    DST = false;
  } // See comment blow for US version

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0); // Display size is 10 characters per line and 6 rows when set to size=1
  for (int i = 1; i <= (10-day_of_week[DoW].length())/2; i++) { display.print(" ");} // 10 character screen width
  display.println(day_of_week[DoW]); // Extract and print day of week
  display.println(" "+String(current_day)+"-"+month_of_year[current_month]+"-"+String(current_year-2000)); // print Day-Month-Year 
  display.setTextSize(2); // Increase text size for time
  display.setCursor(0,17); // Move down a litle and remember location is in pixels not lines!
  if (hours < 10)   display.print("0"+String(hours)); else display.print(String(hours));          // add a leading 0 when hours < 10
  if (minutes < 10) display.print(":0"+String(minutes)); else display.print(":"+String(minutes)); // add a leading 0 when minutes < 10
  display.fillRect(0,35,63,47,BLACK); // Clear the portion of screen that displays seconds (nn) ready for next update
  display.setTextSize(1);   // Reduce text size to fit in the remaining screen area
  display.setCursor(20,32); // Move down to a position that can accomodate seconds display
  if ((seconds < 10)||(seconds%60 == 0)) display.println("(0"+String(seconds%60)+")"); else display.println("("+String(seconds%60)+")"); // add a leading 0 when seconds modulus 60 = 0 
  display.setCursor(22,41); // Move down to a position that can accomodate seconds display
  if (DST == true) display.print("DST"); else display.print("UTC"); // Indicate that DST is on or off
  display.display(); //Implement the new screen contents 
}

  // For the USA DST begins on the second Sunday of March and ends on the first Sunday of November
  //if ((current_month >= 2 ) && (current_month <= 101)){
  //  if (current_month > 2 && current_month < 10) {
  //    DST = true;
  //    hours = hours + 1;
  //  }
  //  else
  //  {
  //    if ( (current_day-DoW)>=8 && (current_day-DoW)>=6)){ // not perfect as hours have not been implemented
  //    DST = true;
  //    hours = hours + 1;
  //    }
  //  }
  // }
  //else {
  //  DST = false;
  //}

