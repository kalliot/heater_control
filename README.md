heater_control
=====================
Arduino based house heating system control application. This project is for controlling small house oil burner. It can be modified or parametrised to control also other type heating systems. 
My aim is to control boiler temperature according to needs. For example during nighttime the boiler temperature may be lowered, to diminish loss of heat from boiler surface. 
In the morning the temperature will be raised, by time, or when noticing somebody has need for hot water. Statistics of operation will be sent to AT&T:s excellent M2X IoT cloud. Arduino does not have a realtime clock. Time is currently read from [NTP] (http://en.wikipedia.org/wiki/Network_Time_Protocol) service. 

Getting Started
==========================
1. Get an Arduno board, I have [Mega] (http://arduino.cc/en/Main/arduinoBoardMega2560), but [Uno] (http://arduino.cc/en/Main/arduinoBoardUno) is also ok.
2. Get an Arduino [Ethernet] (http://arduino.cc/en/Main/ArduinoEthernetShield) or [WiFi] (http://arduino.cc/en/Main/ArduinoWiFiShield) shield to get connected to M2X.
3. Get [arduino dev environment] (http://arduino.cc/en/guide/Environment).
4. Create account for [AT&T's M2X] (https://m2x.att.com/developer/get-started) , to get your own feedId and access key.
5. Get arduino libraries, atleast for Ethernet.


LICENSE
=======
This software is released under the MIT license. See [`LICENSE`](LICENSE) for the terms.
