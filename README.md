# ESP8266-NTP-time-on-an-OLED-with-WiFi-Manager
AN ESP8266 display accurate time using NTP on an OLED display, using the WiFi Manager to make a network connection

DST adjustment rules have been added to TN008a for the UK and US, but should be easily adaptable for other countries using the same logic.

Working out DST change over requires you to determine the range of when a first or last (depends on country and rules used) Sunday occurs, most transitions occur on a Sunday. In the UK the rule is:

'In the UK DST starts at 0100 on the last Sunday in March when time is advanced by 1-hour and ends at 0200 on the last Sunday in October'

You need to calculate (Day of Month - DoW) to then determine the minimum value that must be tested for. For example in March the last Sunday (for the UK rule) can only ever occur on the 25, 26, 27, 28, 29, 30 , or 31. Next calculate the (Day in month - DoW) and test the result for when it is >= 25, if true then DST is on. Repeat for October for the second test. If the date is within the DST timeframe there is no need to calculate, for example if DST runs from Mar to Oct and the current date is within Apr through to Sep then DST is on.

Get the NtpClient from here: https://github.com/arduino-libraries/NTPClient

Ensure you have selected the correct display size in the adafruit_SSD1306.h file so that it looks like this:
find the line that beings '#define
SSD1306_64_48
//   #define SSD1306_128_64
//   #define SSD1306_128_32
//   #define SSD1306_96_16

Then use comments to take out the unwanted display sizes, you need 96x48 pixels.
