# Nick-ESP8266
Official repository of NICK, ESP8266 Nixie Clock

# About the project
Nick is an open source nixie clock based on ESP8266 designed to be as simple as possible and as compact as possible. There are three versions of Nick you can make, one is direct driven IN-12, second version is duplexed IN-12 and finally a tiny duplexed IN-16 version.
  
3d printed case here:  
https://www.thingiverse.com/thing:4014357
  
**UPDATE 14/4/2020: ESP8266 can occasionaly stutter in multiplex, especially if the wifi signal is bad, so I can't recommend it. If you want to use ESP8266, choose the direct-drive version with 4 x 74141 IC which will always be flawless. Or use separate microcontroller for the multiplexing.**
  
**UPDATE 11/12/2020: Firmware for duplexed IN-12 version is done. With this firmware, the duplexed version runs smoothly at ~70hz. Little to no stutter can be observed only at the lowest brightness (low duty cycle). The firmware contains simple config portal, check out screenshot below. **

![alt text](https://github.com/mcer12/Nick-ESP8266/raw/master/Media/IN16_duplexed.jpg)
  
![alt text](https://github.com/mcer12/Nick-ESP8266/raw/master/Media/IN12.jpg)

![alt text](https://github.com/mcer12/Nick-ESP8266/raw/master/Media/config_portal.png)
