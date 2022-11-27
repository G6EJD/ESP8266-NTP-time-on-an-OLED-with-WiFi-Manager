// The MIT License (MIT) Copyright (c) 2019 by David Bird.
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
// ESP8266 Clock displayed on an OLED shield (64x48) using the Network Time Protocol
// (c) D L Bird 2019
//
String   clock_version = "2";
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <time.h>

#include <DNSServer.h>
#include <WiFiManager.h>  //https://github.com/tzapu/WiFiManager

#include <Adafruit_GFX.h>
//*************************************************IMPORTANT******************************************************************
#include "Adafruit_SSD1306.h" // Copy the supplied version of Adafruit_SSD1306.cpp and Adafruit_ssd1306.h to the sketch folder
#define  OLED_RESET 0         // GPIO0
Adafruit_SSD1306 display(OLED_RESET);

String CurrentTime, CurrentDate, webpage = "";
bool display_EU = true;

WiFiClient client;

const char* Timezone    = "GMT1BST,M3.5.0/02,M10.5.0/01";  // Choose your time zone from: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv 
                                                           // See below for examples
const char* ntpServer   = "pool.ntp.org";                  // Or, choose a time server close to you, but in most cases it's best to use pool.ntp.org to find an NTP server
                                                           // then the NTP system decides e.g. 0.pool.ntp.org, 1.pool.ntp.org as the NTP syem tries to find  the closest available servers
                                                           // EU "0.europe.pool.ntp.org"
                                                           // US "0.north-america.pool.ntp.org"
                                                           // See: https://www.ntppool.org/en/                                                           
int  gmtOffset_sec      = 0;    // UK normal time is GMT, so GMT Offset is 0, for US (-5Hrs) is typically -18000, AU is typically (+8hrs) 28800
int  daylightOffset_sec = 3600; // In the UK DST is +1hr or 3600-secs, other countries may use 2hrs 7200 or 30-mins 1800 or 5.5hrs 19800 Ahead of GMT use + offset behind - offset

ESP8266WebServer server(80);  // Set the port you wish to use, a browser default is 80, but any port can be used, if you set it to 5555 then connect with http://nn.nn.nn.nn:5555/

void setup() {
  Serial.begin(115200);
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
  if (!wifiManager.autoConnect("ESP8266_AP")) {
    Serial.println("failed to connect and timeout occurred");
    delay(3000);
    ESP.reset(); //reset and try again
    delay(5000);
  }
  // At this stage the WiFi manager will have successfully connected to a network, or if not will try again in 180-seconds
  //------------------------------
  // Print the IP address

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.println("Sync...");
  display.display();
  delay(1000);
  Serial.print("Use this URL to connect: ");
  Serial.print("http://"); Serial.print(WiFi.localIP()); Serial.println("/");
  display_ip();
  SetupTime();
  UpdateLocalTime();
  server.begin();  // Start the webserver
  Serial.println("Webserver started...");

  server.on("/", NTP_Clock_home_page);
  server.on("/DISPLAY_MODE_TOGGLE", display_mode_toggle); // Define what happens when a client requests attention
  server.on("/EXIT_SETUP", exit_setup);  // Define what happens when a client requests attention
  EEPROM.begin(10);
  // 0 - Display Status 
  EEPROM.get(0,display_EU);
  delay(500);
  EEPROM.commit();
  EEPROM.end();
}

void loop() {
  display_time();
  if (millis() % 60000 == 0) UpdateLocalTime();
  server.handleClient();   // Wait for a client to connect and when they do process their requests
}

boolean SetupTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "time.nist.gov"); //(gmtOffset_sec, daylightOffset_sec, ntpServer)
  setenv("TZ", Timezone, 1);  //setenv()adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
  tzset(); // Set the TZ environment variable
  delay(2000);
  bool TimeStatus = UpdateLocalTime();
  return TimeStatus;
}

boolean UpdateLocalTime() {
  struct tm timeinfo;
  time_t now;
  time(&now);
  //See http://www.cplusplus.com/reference/ctime/strftime/
  char output[30];
  if (display_EU == true) {
    strftime(output, 30, "%d/%m/%y", localtime(&now));
    CurrentDate = output;
    strftime(output, 30, "%H:%M:%S", localtime(&now));
    CurrentTime = output;
  }
  else { 
    strftime(output, 30, "%m/%d/%y", localtime(&now));
    CurrentDate = output;
    strftime(output, 30, "%r", localtime(&now));
    CurrentTime = output;
  }
  Serial.println(CurrentTime);
  Serial.println(CurrentDate);
  return true;
}

void display_time() { 
  display.clearDisplay();
  display.setCursor(8,5);   // center date display
  display.setTextSize(1);   
  display.println(CurrentDate);
  display.setTextSize(2);   
  if (display_EU == true) {
    display.setCursor(0,16);  // center Time display
    display.println(CurrentTime.substring(0,5));
    display.setCursor(7,33); // center Time display
    display.print("(" + CurrentTime.substring(6,8) + ")");
  }
  else
  {
    display.setCursor(8,16);  // center Time display
    if (CurrentTime.startsWith("0")) display.println(CurrentTime.substring(1,5));
    else 
    {
      display.setCursor(0,16);
      display.println(CurrentTime.substring(0,5));
    }
    display.print("(" + CurrentTime.substring(6,8) + ")");
    display.setTextSize(1);
    display.setCursor(40,33); // center Time display
    display.print(CurrentTime.substring(8,11));
  }
  display.display();
  delay(800);
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
  webpage += "#header  {background-color:#6A6AE2; font-family:tahoma; width:1024px; padding:10px; color:white; text-align:center; }";
  webpage += "#body    {background-color:#6A6AE2; font-family:tahoma; width:1024px; padding:10px; color:white; font-size:32px; text-align:center; }";
  webpage += "#section {background-color:#E6E6FA; font-family:tahoma; width:1024px; padding:10px; color_blue;  font-size:22px; text-align:center;}";
  webpage += "#footer  {background-color:#6A6AE2; font-family:tahoma; width:1024px; padding:10px; color:white; font-size:14px; clear:both; text-align:center;}";
  webpage += "</style></head><body>";
}

void update_webpage() {
  append_webpage_header();
  webpage += "<div id=\"header\"><h1>NTP OLED Clock Setup " + clock_version + "</h1></div>";
  webpage += "<div id=\"body\">[Display Mode is ";
  if (display_EU == true) webpage += "UK/EU]"; else webpage += "USA]";
  webpage += "<p><a href=\"DISPLAY_MODE_TOGGLE\">Change Display Mode</a></p>";
  webpage += "<p><a href=\"EXIT_SETUP\">Run the clock</a></p>";
  webpage += "</div>";
  webpage += "<div id=\"footer\">Copyright &copy; D L Bird 2019</div></body></html>";
}

void display_mode_toggle() {
  EEPROM.begin(9);
  if (display_EU) display_EU = false; else display_EU = true;
  EEPROM.put(0, display_EU);
  delay(500);
  EEPROM.end();
  update_webpage();
  server.send(200, "text/html", webpage);
}

void exit_setup() {
  display_time();
  server.send(200, "text/html", webpage);
}

void display_ip() {
  // Print the IP address
  display.print("Connect:");
  display.println("//");
  display.println(WiFi.localIP());
  display.println("/");
  display.display();
  delay(2500);
  display.clearDisplay();
}
