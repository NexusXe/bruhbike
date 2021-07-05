#include "ArduinoTimer.h"
#include "Filter.h"
#include <string>
#include "index.h"
#include "WiFi.h"
#include <WebServer.h> // dude this library is so fucking bloated it takes up like 35% of my MCU storage like what ????


WebServer server(80);          // this is here instead of in setup because server belongs to both WiFi and WebServer
                               // is there a better way of getting around this issue than this? probably! do i care? nope!

const char* ssid     = "bruhbike";
const char* password = "obamaphone";

static const float wheelCircumference = (2.0 * 3.1415 * 10.0);  // in inches

const int numReadings = 3;

int readings[numReadings];      // the readings from the analog input

int readIndex = 0;              // the index of the current reading
int total = 0;                  // the running total
int average = 1;                // the average
int oldAverage = 1;             // the old average

                                // some of these values are 1 instead of 0 because the MCU throws a fit when you try to divide by 0
                                // shoudn't the compiler notice that? lmao
                                // bold of me to assume that the arduino compiler would go out of its way to do that

int hallValue = 1;

float percentChange = 1;

int rawStrengh = 0;
int smoothStrength = 0;



volatile int revolutions = 0;

volatile float currentSpeed = 0.0;
ArduinoTimer Timer1;


ExponentialFilter<int> FilteredStrength(80,0);


void setup() {
  // initialize serial communication with computer:
  Serial.begin(9600);
  // initialize all the readings to 0:
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0;
  }
  WiFi.mode(WIFI_AP); // access point mode, tried BLE but Android's implementation of BLE advertising is awful
  WiFi.softAP(ssid, password);
  
  server.on("/", handleRoot);
  
  server.on("/readADC", handleADC); // webserver code was originally meant for an ADC value but i cant be assed to change it for literally no reason
 
  server.begin();                  // start (wifi) server
  Serial.println("HTTP server started"); // fuck you and fuck your SSL i am too lazy for that shit
                                         // (also SSL on the ESP32 is so goddamn slow it's unbearable)
  
}



void CalculateSpeedCode( void * parameter ) { // i dont know what this does or what "parameter" is but it works and i dont question it
  currentSpeed = (wheelCircumference / float(Timer1.EllapsedMilliseconds()) * 56.818);
  Serial.print("Current Speed: ");
  Serial.print(currentSpeed);
  Serial.println();
  Timer1.Reset();
  revolutions += 1;  // just debug stuff while i get all of the smoothening and debouncing stuff figured out
  Serial.print("Revolutions: "); // quite useful for noticing when one revolution is viewed as two or more
  Serial.print(revolutions);  // couldnt figure out how to concatenate ints and chars so i am just doing this
  Serial.println();
  server.handleClient();
  vTaskDelete(NULL);
  // TODO: implement decay or smoothening so the speed doesnt jump around AND so it decays while stopped
  // i probably need to change my entire approach to how the speed is processed?
  // deceleration while not moving (i.e sudden stop) can probably just be modeled with a cubic deceleration model
  // the actual readings while slowing down would provide most of the data in that regard
  // smoothening during acceleration could possibly be done with one more magnet in the opposite polarity and some magic on the HTML side (if applicalble)
}



void handleRoot() {
 String s = MAIN_page; // Read HTML contents
 server.send(200, "text/html", s); // Send web page
}



void loop() {
  hallValue = hallRead();
  
 FilteredStrength.Filter(hallValue);



 average = FilteredStrength.Current();
 // Serial.println(average);
 
 if(oldAverage >= 90 and average >= 90) {
    Serial.println("deboucing...");
    delay(10); // delay in between reads for stability and as effective debouncing
               // at normal riding speeds 10ms should be PLENTY, still need to test though
    return;
  }
  
    if (average >= 100) {

      TaskHandle_t CalculateSpeed;
      xTaskCreatePinnedToCore(CalculateSpeedCode, "CalculateSpeed", 10500, NULL, 0, &CalculateSpeed, 0);
    }
  oldAverage = average;
  
  delay(5);        // delay in between reads for stability, can go down to 1ms but no need here
                   // since we're using exponential filtering, something as high as 15ms should work in this use case
                   // however, it doesn't really matter too much since we have processing power to spare
                   // changes to this may neccesitate changes to the exponential filter weight
}

void handleADC() { // gets speed data for webserver
 String adcValue = String(currentSpeed); // gets speedometer data from CalculateSpeedCode
 server.send(200, "text/plane", adcValue); //Send ADC value only to client ajax request
}
