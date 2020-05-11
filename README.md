# NRDP_ESP32_Two
## DHT Metrics Collection:
Send DHT11 sensor data to NagiosXI wihtout evaluation via NRDP.
 Temperature (C)
 Humidity (%)
 DewPoint (C)
 HeatIndex (C)
 
 ## DHT Critical Evaluations:
 Send DHT11 sensor data to NagiosXI after evaluating it against a fixed value.
 Temperature is greater than 27.00 (C)
 Humidity is greater than 75.00 (%)

 ## DHT Warning Evaluations:
 Send DHT11 sensor data to NagiosXI after evaluating it against a varailbe range.
 Temperature is greater than 25.00 (C) and is less than 27.00 (C)
 Humidity is greater than 70.00 (%) and is less than 75.00 (%)

 ## NagiosXI Host Check:
 Send host check data to NagiosXI server via NRDP. 

 ## NagiosXI Performance Data:
 All checks send performace data along with the checks output.
 Evaluated checks will inclide the warning and critical values in the perfdata string.

## Arduino Contributed Libraries
 This example depends and is based heavily on the DHTesp library example. 
 Please install DHTesp library first 
 https://github.com/beegee-tokyo/DHTesp                          
 
 This example depends on the ESP32Ticker library to wake up 
 the monitoring task on a scheduled interval to update the
 nrdp check data.
 Please install Ticker-esp32 library first                  
 bertmelis/Ticker-esp32                                     
 https://github.com/bertmelis/Ticker-esp32                  

# Usage
General uage documentation

## Script Modifications
The example will not work out of the box. You must make some changes to the networking
and NRDP server configuration.

### WiFi Connection Settings
Line # 105
Change the SSID to be that of your test network.
Line # 106
Change the password for your test network

### NagiosXI NRDP Settings
Line # 599
Change the hostname in the nrdp url string to be that of your NagiosXI Server
Line # 606
Change the token value to be that of your NagiosXI Server

## NagiosXI Inbound Connection Settings
In order for the ESP32 device to communicate your NagiosXI Server need to be configured to allow;
1. Inbound Connections to NRDP
2. NRDP Token configured

## Unconfigured Objects
New hosts will appear in unconfigured objects. Adding the device to your NagiosXI Server will require
either maually adding the host and services from Admin Tools or the enablement of automatic processing
of these items.
