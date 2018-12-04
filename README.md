# ESP8266-NTP-time-on-an-OLED-with-WiFi-Manager

**** YOU MUST USE THE DISPLAY DRIVER (1306) PROVIDED OR USE THE LINK BELOW **** 

An ESP8266 displays accurate time using NTP on an OLED display, using the WiFi Manager to make a network connection.
Two versions:
1. Standard with no alarm.
2. With Alarm

DST adjustment rules have been refined and now support correct time change-over.

Working out DST change over requires you to determine the range of when a first or last (depends on country and rules used) Sunday occurs, most transitions occur on a Sunday. In the UK the rule is:

'In the UK DST starts at 0100 on the last Sunday in March when time is advanced by 1-hour and ends at 0200 on the last Sunday in October'

You need to calculate (Day of Month - DoW) to then determine the minimum value that must be tested for. For example in March the last Sunday (for the UK rule) can only ever occur on the 25, 26, 27, 28, 29, 30 , or 31. Next calculate the (Day in month - DoW) and test the result for when it is >= 25, if true then DST is on. Repeat for October for the second test. If the date is within the DST timeframe there is no need to calculate, for example if DST runs from Mar to Oct and the current date is within Apr through to Sep then DST is on.

Get the NTPClient from here: https://github.com/arduino-libraries/NTPClient
Get the WeMos OLED drivers frome here: https://github.com/mcauser/Adafruit_SSD1306
