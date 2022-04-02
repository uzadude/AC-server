#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Airwell.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DHTesp.h"

Adafruit_SSD1306 display(128, 64, &Wire, -1);

DHTesp dht;
//Define the DHT object
int dhtPin = 13;//Define the dht pin
float roomTemp = 0;
float roomHumidity = 0;

const uint16_t kIrLed = 4;  // ESP8266 GPIO pin to use. Recommended: 4 (D2).
IRAirwellAc ac(kIrLed);     // Set the GPIO used for sending messages.

void printState() {
  // Display the settings.
  Serial.println("Airwell A/C remote is in the following state:");
  Serial.printf("  %s\n", ac.toString().c_str());

  display.clearDisplay();
	display.setCursor(0,0);
	display.setTextSize(1);
	display.println("AC state");
	display.print("power toggle: ");
  display.println(ac.getPowerToggle());
	display.print("Temp: ");
  display.println(ac.getTemp());
	display.print("Mode: ");
  display.print(ac.getMode());
	display.print("     Fan: ");
  display.println(ac.getFan());
  display.println();

	display.println("Room State");
  display.print("Tempratue: ");
  display.println(roomTemp);
	display.print("Humidity: ");
  display.println(roomHumidity);

	display.display();
}

void setup() {
  ac.begin();
  Serial.begin(9600);
  	// initialize with the I2C addr 0x3C
	display.begin(SSD1306_SWITCHCAPVCC, 0x3C); 
  	// Clear the buffer.
	display.clearDisplay();
  dht.setup(dhtPin, DHTesp::DHT11);//Initialize the dht pin and dht object
  delay(200);

  // Set up what we want to send. See ir_Airwell.cpp for all the options.
  Serial.println("Default state of the remote.");
  printState();
  Serial.println("Setting initial state for A/C.");
  //ac.off();
  ac.setFan(kAirwellFanLow);
  ac.setMode(kAirwellCool);
  ac.setTemp(23);
  //ac.setSwing(false);
  printState();

	// Display Text
	display.setTextSize(1);
	display.setTextColor(WHITE);
	display.setCursor(0,28);
	display.println("Hello world!");
	display.display();

}

void loop() {
  // Turn the A/C unit on
  Serial.println("Turn on the A/C ...");
  ac.setPowerToggle(true);
  ac.send();

  flag:TempAndHumidity newValues = dht.getTempAndHumidity();
  if (dht.getStatus() != 0) {
    goto flag;
  }

  roomTemp = newValues.temperature;
  roomHumidity = newValues.humidity;

  printState();
  delay(60000);  // wait 15 seconds
}