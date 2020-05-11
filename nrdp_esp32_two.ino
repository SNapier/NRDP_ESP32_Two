#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include "DHTesp.h"
#include "Ticker.h"

#ifndef ESP32
#pragma message(THIS EXAMPLE IS FOR ESP32 ONLY!)
#error Select ESP32 board.
#endif

/* 
--

 NRDP_ESP32_Two                                             
 version 1.0.0

 DHT Metrics Collection:
 Send DHT11 sensor data to NagiosXI wihtout evaluation via NRDP.
 Temperature (C)
 Humidity (%)
 DewPoint (C)
 HeatIndex (C)
 
 DHT Critical Evaluations:
 Send DHT11 sensor data to NagiosXI after evaluating it against a fixed value.
 Temperature is greater than 27.00 (C)
 Humidity is greater than 75.00 (%)

 DHT Warning Evaluations:
 Send DHT11 sensor data to NagiosXI after evaluating it against a varailbe range.
 Temperature is greater than 25.00 (C) and is less than 27.00 (C)
 Humidity is greater than 70.00 (%) and is less than 75.00 (%)

 NagiosXI Host Check:
 Send host check data to NagiosXI server via NRDP. 

 NagiosXI Performance Data:
 All checks send performace data along with the checks output.
 Evaluated checks will inclide the warning and critical values in the perfdata string.

--

 This example depends and is based heavily on the DHTesp library example. 
 Please install DHTesp library first 
 https://github.com/beegee-tokyo/DHTesp                          
 
 This example depends on the ESP32Ticker library to wake up 
 the monitoring task on a scheduled interval to update the
 nrdp check data.
 Please install Ticker-esp32 library first                  
 bertmelis/Ticker-esp32                                     
 https://github.com/bertmelis/Ticker-esp32                  
*/

//Lests get started
DHTesp dht;

//Hold the NRDP check data
JSONVar checkdata;

//Check state boolean
bool crit = false;
bool warn = false;

//Hostname
String nrdpHostname = "NRDP-ESP32-Two";


void tempTask(void *pvParameters);
void hostTask(void *pvParameters);
bool getTemperature();
bool checkTempCritical();
bool checkTempWarning();
bool checkHumidityCritical();
bool checkHumidityWarning();
bool nrdphostcheck();
void triggerGetTemp();
void triggerSendHost();

/** Task handle for the light value read task */
TaskHandle_t tempTaskHandle = NULL;
/** Ticker for temperature reading */
Ticker tempTicker;

/** Task handle for the host check task */
TaskHandle_t hostTaskHandle = NULL;
/** Ticker for host checks */
Ticker hostTicker;

/** Comfort profile */
ComfortState cf;

/* 
 Flag if monitoring tasks should run 
 Default is FALSE, the tasks are enabled in the Setup Function 
*/
bool tasksEnabled = false;

/** Pin number for DHT11 data pin */
int dhtPin = 15;

//Wifi Setup
const char* ssid = "<change-me>";
const char* password = "<change-me>";

/**
 * initTemp
 * Setup DHT library
 * Setup task and timer for repeated measurement
 * @return bool
 *    true if task and timer are started
 *    false if task or timer couldn't be started
 */
bool initTemp() {
  byte resultValue = 0;
  // Initialize temperature sensor
	dht.setup(dhtPin, DHTesp::DHT11);
	Serial.println("DHT initiated");

  // Start task to get temperature
	xTaskCreatePinnedToCore(
			tempTask,                       /* Function to implement the task */
			"tempTask ",                    /* Name of the task */
			7000,                           /* Stack size in words */
			NULL,                           /* Task input parameter */
			5,                              /* Priority of the task */
			&tempTaskHandle,                /* Task handle. */
			1);                             /* Core where the task should run */

  if (tempTaskHandle == NULL) {
    Serial.println("Failed to start task for temperature update");
    return false;
  } else {
    // Start update of environment data every 60 seconds
    tempTicker.attach(28, triggerGetTemp);
  }
  return true;
}

/**
 * initHostCheck
 * Setup task and timer for repeated host check updates
 * @return bool
 *    true if task and timer are started
 *    false if task or timer couldn't be started
 */
bool initHostCheck() {
  byte resultValue = 0;
  Serial.println("Host Check Initiated");

  // Start task to get temperature
  xTaskCreatePinnedToCore(
      hostTask,                       /* Function to implement the task */
      "hostTask ",                    /* Name of the task */
      4000,                           /* Stack size in words */
      NULL,                           /* Task input parameter */
      5,                              /* Priority of the task */
      &hostTaskHandle,                /* Task handle. */
      1);                             /* Core where the task should run */

  if (hostTaskHandle == NULL) {
    Serial.println("Failed to start task for Host Check");
    return false;
  } else {
    // Start update of environment data every 60 seconds
    hostTicker.attach(29, triggerSendHost);
  }
  return true;
}

/**
 * triggerGetTemp
 * Sets flag dhtUpdated to true for handling in loop()
 * called by Ticker getTempTimer
 */
void triggerGetTemp() {
  if (tempTaskHandle != NULL) {
	   xTaskResumeFromISR(tempTaskHandle);
  }
}


/**
 * triggerSendHost
 * Sets flag hostUpdate to true for handling in loop()
 * called by Ticker getHostTimer
 */
void triggerSendHost() {
  if (hostTaskHandle != NULL) {
     xTaskResumeFromISR(hostTaskHandle);
  }
}

/**
 * Task to reads temperature from DHT11 sensor
 * @param pvParameters
 *    pointer to task parameters
 */
void tempTask(void *pvParameters) {
	Serial.println("tempTask loop started");
	while (1) // tempTask loop
  {
    if (tasksEnabled) {
      // Get temperature values
			getTemperature();
		}
    // Got sleep again
		vTaskSuspend(NULL);
	}
}

/**
 * Task to send NRDP Host Check to NagiosXI
 * @param pvParameters
 *    pointer to task parameters
 */
void hostTask(void *pvParameters) {
  Serial.println("hostTask loop started");
  while (1) // tempTask loop
  {
    if (tasksEnabled) {
      // Send the NRDP Host Check Data
      nrdphostcheck();
    }
    // Got sleep again
    vTaskSuspend(NULL);
  }
}

/**
 * getTemperature
 * Reads temperature from DHT11 sensor
 * @return bool
 *    true if temperature could be aquired
 *    false if aquisition failed
*/
bool getTemperature() {
  /* BEGIN DHT11 DATA UPDATE */
	// Reading temperature for humidity takes about 250 milliseconds!
	// Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
  TempAndHumidity newValues = dht.getTempAndHumidity();
  
	// Check if any reads failed and exit early (to try again).
	if (dht.getStatus() != 0) {
		Serial.println("DHT11 error status: " + String(dht.getStatusString()));
		//Unknown
		return false;
	}
  float t = newValues.temperature;
  float h = newValues.humidity;
	float hi = dht.computeHeatIndex(newValues.temperature, newValues.humidity);
  float dp = dht.computeDewPoint(newValues.temperature, newValues.humidity);
  float cr = dht.getComfortRatio(cf, newValues.temperature, newValues.humidity);
  /*END DHT11 DATA UPDATE*/

  /* BEGIN NRDP CHECK DATA */
  //Convert DHT11 sensor readings to strings to be used in te NRDP check data
  //TEMP
  String stmp;
  stmp = String(t);
  stmp += "C";

  //HUMIDITY
  String shum;
  shum += String(h);
  shum += "%";
  
  //HEAT-INDEX
  String shi;
  shi = String(hi);
  shi += "C";
  
  //DEW-POINT
  String sdp;
  sdp = String(dp);
  sdp += "C";


  //Temperature Metrics Array
  checkdata["checkresults"][1]["checkresult"]["type"] = "service";
  checkdata["checkresults"][1]["checkresult"]["checktype"] = "1";
  checkdata["checkresults"][1]["hostname"] = nrdpHostname;
  
  // Use an idompotent naming convention to allow for easier parsing
  // Single value per check
  checkdata["checkresults"][1]["servicename"] = "esp32--dht11--temp--metric";
  checkdata["checkresults"][1]["state"] = "0";

  // Check Output string
  String output1 = "OK:";
  output1 += "ESP32-DHT11-TEMP(Temperature=";
  output1 += stmp;
  output1 += ")|temperature=";
  output1 += stmp;
  output1 += ";";

  checkdata["checkresults"][1]["output"] = output1;

  //Humidity Array
  checkdata["checkresults"][2]["checkresult"]["type"] = "service";
  checkdata["checkresults"][2]["checkresult"]["checktype"] = "1";
  checkdata["checkresults"][2]["hostname"] = nrdpHostname;
  
  // Use an idompotent naming convention to allow for easier parsing
  // Single value per check
  checkdata["checkresults"][2]["servicename"] = "esp32--dht11--humidity--metric";
  checkdata["checkresults"][2]["state"] = "0";

  // Check Output string
  String output2 = "OK:";
  output2 += "ESP32-DHT11-HUMIDITY(Humidity=";
  output2 += shum;
  output2 += ")|humidity=";
  output2 += shum;
  output2 += ";";

  checkdata["checkresults"][2]["output"] = output2;

  //HeatIndex Array
  checkdata["checkresults"][3]["checkresult"]["type"] = "service";
  checkdata["checkresults"][3]["checkresult"]["checktype"] = "1";
  checkdata["checkresults"][3]["hostname"] = nrdpHostname;
  
  // Use an idompotent naming convention to allow for easier parsing
  // Single value per check
  checkdata["checkresults"][3]["servicename"] = "esp32--dht11--heatindex--metric";
  checkdata["checkresults"][3]["state"] = "0";

  // Check Output string
  String output3 = "OK:";
  output3 += "ESP32-DHT11-HEATINDEX(HeatIndex=";
  output3 += shi;
  output3 += ")|heatindex=";
  output3 += shi;
  output3 += ";";

  checkdata["checkresults"][3]["output"] = output3;

  //DewPoint Array
  checkdata["checkresults"][4]["checkresult"]["type"] = "service";
  checkdata["checkresults"][4]["checkresult"]["checktype"] = "1";
  checkdata["checkresults"][4]["hostname"] = nrdpHostname;
  
  // Use an idompotent naming convention to allow for easier parsing
  // Single value per check
  checkdata["checkresults"][4]["servicename"] = "esp32--dht11--dewpoint--metric";
  checkdata["checkresults"][4]["state"] = "0";

  // Check Output string
  String output4 = "OK:";
  output4 += "ESP32-DHT11-DEWPOINT(DewPoint=";
  output4 += sdp;
  output4 += ")|dewpoint=";
  output4 += sdp;
  output4 += ";";

  checkdata["checkresults"][4]["output"] = output4;



  //Evaluated critical alert
  //Temp Critical Array
  String output5 = "";
  checkdata["checkresults"][5]["checkresult"]["type"] = "service";
  checkdata["checkresults"][5]["checkresult"]["checktype"] = "1";
  checkdata["checkresults"][5]["hostname"] = nrdpHostname;
  
  // Use an idompotent naming convention to allow for easier parsing
  // Single value per check
  checkdata["checkresults"][5]["servicename"] = "esp32--dht11--temp--critical";

  //evaluate temp and set the check state
  //check temp will return true for critical and false for OK
  crit = checkTempCritical(t);
  if(!crit){
      checkdata["checkresults"][5]["state"] = "0";
       output5 += "OK:";
    }
    else{
      checkdata["checkresults"][5]["state"] = "2";
      output5 += "CRITICAL:";
    }

  // Check Output string
  output5 += "ESP32-DHT11-TEMP-CRIT(Temperature=";
  output5 += stmp;
  output5 += ")|temperature=";
  output5 += stmp;
  output5 += ";25;27";

  checkdata["checkresults"][5]["output"] = output5;

  //Evaluated warning alert
  //Temp Warning Array
  String output6 = "";
  checkdata["checkresults"][6]["checkresult"]["type"] = "service";
  checkdata["checkresults"][6]["checkresult"]["checktype"] = "1";
  checkdata["checkresults"][6]["hostname"] = nrdpHostname;
  
  // Use an idompotent naming convention to allow for easier parsing
  // Single value per check
  checkdata["checkresults"][6]["servicename"] = "esp32--dht11--temp--warning";

  //evaluate temp and set the check state
  //check temp warning will return true for warning range and false for OK
  warn = checkTempWarning(t);
  if(!warn){
      checkdata["checkresults"][6]["state"] = "0";
       output6 += "OK:";
    }
    else{
      checkdata["checkresults"][6]["state"] = "2";
      output6 += "WARNING:";
    }

  // Check Output string
  output6 += "ESP32-DHT11-TEMP-WARN(Temperature=";
  output6 += stmp;
  output6 += ")|temperature=";
  output6 += stmp;
  output6 += ";25;27";

  checkdata["checkresults"][6]["output"] = output6;

  //Evaluated critical alert
  //Humidity Critical Array
  String output7 = "";
  checkdata["checkresults"][7]["checkresult"]["type"] = "service";
  checkdata["checkresults"][7]["checkresult"]["checktype"] = "1";
  checkdata["checkresults"][7]["hostname"] = nrdpHostname;
  
  // Use an idompotent naming convention to allow for easier parsing
  // Single value per check
  checkdata["checkresults"][7]["servicename"] = "esp32--dht11--humidity--critical";

  //evaluate temp and set the check state
  //check humidity critical will return true for warning range and false for OK
  crit = checkHumidityCritical(h);
  if(!crit){
      checkdata["checkresults"][7]["state"] = "0";
       output7 += "OK:";
    }
    else{
      checkdata["checkresults"][7]["state"] = "2";
      output7 += "CRITICAL:";
    }

  // Check Output string
  output7 += "ESP32-DHT11-HUMIDITY-CRIT(Huminity=";
  output7 += shum;
  output7 += ")|humidity=";
  output7 += shum;
  output7 += ";70;75";

  checkdata["checkresults"][7]["output"] = output7;

  //Evaluated Warning alert
  //Humidity Warning Array
  String output8 = "";
  checkdata["checkresults"][8]["checkresult"]["type"] = "service";
  checkdata["checkresults"][8]["checkresult"]["checktype"] = "1";
  checkdata["checkresults"][8]["hostname"] = nrdpHostname;
  // Use an idompotent naming convention to allow for easier parsing
  // Single value per check
  checkdata["checkresults"][8]["servicename"] = "esp32--dht11--humidity--warning";

  //evaluate temp and set the check state
  //check humidity warning will return true for warning range and false for OK
  warn = checkHumidityWarning( h);
  if(!warn){
      checkdata["checkresults"][8]["state"] = "0";
       output8 += "OK:";
    }
    else{
      checkdata["checkresults"][8]["state"] = "2";
      output8 += "CRITICAL:";
    }

  // Check Output string
  output8 += "ESP32-DHT11-HUMIDITY-WARN(Huminity=";
  output8 += shum;
  output8 += ")|humidity=";
  output8 += shum;
  output8 += ";70;75";

  checkdata["checkresults"][8]["output"] = output8;

/* END NRDP CHECK DATA*/

  Serial.println();
  Serial.println("-----------------------------------------------------------------------------------------");
  Serial.println("DHT11 -- DATA Update Successful");
  Serial.println("DHT11 -- Temperature:" + String(newValues.temperature) + " Humidity:" + String(newValues.humidity) + " HeatIndex:" + String(hi) + " DewPoint:" + String(dp) + " " + comfortStatus);
  Serial.println("-----------------------------------------------------------------------------------------");
  Serial.println();
  
  return true;
}

/* 
Evaluate the current temp
If the temp is above the crit_t value return true
Else return false 
*/
bool checkTempCritical(float t) {
  if(t > 29.00){
      return true;
    }
  else{
      return false;
    }
  }

/* 
Evaluate the current temp
If the temp is above the warn_t value and below the crit_t value return true
Else return false 
*/
bool checkTempWarning(float t) {
  if((t > 25.00) and (t < 27.00)){
      return true;
    }
  else{
      return false;
    }
  }

/* 
Evaluate the current humidity
If the humidity is above the crit_h value return true
Else return false 
*/
bool checkHumidityCritical(float h) {
  if(h > 75.00){
      return true;
    }
  else{
      return false;
    }
  }

/* 
Evaluate the current humidity
If the humidity is above the warn_h and less than the crit_h value return true
Else return false 
*/
bool checkHumidityWarning(float h) {
  if((h > 70.00) and (h < 75.00)){
      return true;
    }
  else{
      return false;
    }
  }

/* 
NRDP Host Check 
*/
boolean nrdphostcheck() {
  // NagiosXI Host Check
  // This will trigger the auto-config of this host if it is unknown to the XI server.
  // See Auto-Configuration Wizard
  checkdata["checkresults"][0] = "OK:NRDP-ESP32-Two(0.0.1)";
  checkdata["checkresults"][0]["checkresult"]["type"] = "host";
  checkdata["checkresults"][0]["checkresult"]["checktype"] = "1";
  checkdata["checkresults"][0]["hostname"] = nrdpHostname;
  checkdata["checkresults"][0]["state"] = "0";
  
  String nrdpHostCheck = nrdpHostname;
  //Add the host perf data
  nrdpHostCheck += "|up=100;85;95;0";
 
  checkdata["checkresults"][0]["output"] = nrdpHostCheck;

  Serial.println();
  Serial.println("-----------------------------------------------------------------------------------------");
  Serial.println("NRDP -- " + nrdpHostname + "-DATA Update Successful");
  Serial.println("-----------------------------------------------------------------------------------------");
  Serial.println();
  
  return true;
} 

void sendnrdp(JSONVar checkdata){
  // JSON.stringify(myVar) can be used to convert the json var to a String
  // We prepend the checkdata= to the string for processing via Nagios
  // See NRDP Documentation for full description
  String cdata = "jsondata=";
  String checkJSONString = JSON.stringify(checkdata);
  cdata += checkJSONString;

  //Submit check data in JSON format to NRDP via HTTP(Post) 
  HTTPClient http;
  
  // The NRDP url from the Nagios server
  // Includes the query operator
  String url = "https://<change-me>/nrdp/?";
  
  //NRDP Token (With trialing apmersand)
  url += "token=<change-me>&";
  
  // NRDP Command (Submit Check)
  url += "cmd=submitcheck&";
  url += cdata;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  //Submit the check to NagiosXI server and capture the response
  http.POST("");
  String result = http.getString();
  
  //Convert the response string of httpclient into JSON
  JSONVar doc = result;
  
  //Parse the JSON object doc into an array
  JSONVar myResult = JSON.parse(doc);
  
  //The response from nagios is in the result array
  //Console Out
  Serial.println();
  Serial.println("-----------------------------------------------------------------------------------------");
  Serial.print("NRDP-POST-RESPONSE=");
  Serial.println(myResult["result"]);
  Serial.println("-----------------------------------------------------------------------------------------");
  Serial.println();
  
  http.writeToStream(&Serial);
  http.end();
  }

void setup()
{
  Serial.begin(115200);
  Serial.println();
  delay(10);

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("---------------------------------------------");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();
  Serial.println("SIGNAL STRENGTH:");
  Serial.println(WiFi.RSSI());
  Serial.println("---------------------------------------------");
  Serial.println();
  Serial.println("---------------------------------------------");
  Serial.println("BEGIN NRDP_ESP32_Two HOST Monitoring Tasks");
  initHostCheck();
  Serial.println("---------------------------------------------");
  Serial.println();
  Serial.println("---------------------------------------------");
  Serial.println("BEGIN NRDP_ESP32_Two DHT11 Monitoring Tasks");
  initTemp();
  Serial.println("---------------------------------------------");
  Serial.println();
  // Signal end of setup() to tasks
  tasksEnabled = true;
}

void loop() {
  if (!tasksEnabled) {
    // Wait 2 seconds to let system settle down
    delay(2000);
    // Enable the monitoring tasks
    tasksEnabled = true;
    //temperature monitoring tasks
    if (tempTaskHandle != NULL) {
		vTaskResume(tempTaskHandle);
	}
    // NRDP Host Check
    if (hostTaskHandle != NULL) {
        vTaskResume(hostTaskHandle);
    }
  }
  // Every 30 seconds send the check data to NagiosXI
  delay(30000);
  sendnrdp(checkdata);
  
  //The magic yield function
  yield();
}
