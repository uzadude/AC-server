#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Electra.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);

const uint16_t kIrLed = 4;  // ESP8266 GPIO pin to use. Recommended: 4 (D2).
IRElectraAc ac(kIrLed);     // Set the GPIO used for sending messages.

void printState() {
  // Display the settings.
  Serial.println("Electra A/C remote is in the following state:");
  Serial.printf("  %s\n", ac.toString().c_str());

  display.clearDisplay();
	display.setCursor(0,0);
	display.setTextSize(1);
	display.println("state:");
	display.print("power: ");
  display.println(ac.getPower());
	display.print("Temp: ");
  display.println(ac.getTemp());
	display.print("Mode: ");
  display.println(ac.getMode());
	display.print("Fan: ");
  display.println(ac.getFan());
	display.display();
}

void setup() {
  ac.begin();
  Serial.begin(9600);
  	// initialize with the I2C addr 0x3C
	display.begin(SSD1306_SWITCHCAPVCC, 0x3C); 
  	// Clear the buffer.
	display.clearDisplay();
  delay(200);

  // Set up what we want to send. See ir_Electra.cpp for all the options.
  Serial.println("Default state of the remote.");
  printState();
  Serial.println("Setting initial state for A/C.");
  ac.off();
  ac.setFan(kElectraAcFanLow);
  ac.setMode(kElectraAcCool);
  ac.setTemp(25);
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
  ac.on();
  ac.send();
  printState();
  delay(15000);  // wait 15 seconds
  // and set to cooling mode.
  Serial.println("Set the A/C mode to cooling ...");
  ac.setMode(kElectraAcCool);
  ac.send();
  printState();
  delay(15000);  // wait 15 seconds

  // Increase the fan speed.
  Serial.println("Set the fan to high and the swing on ...");
  ac.setFan(kElectraAcFanHigh);
  //ac.setSwing(true);
  ac.send();
  printState();
  delay(15000);

  // Change to Fan mode, lower the speed, and stop the swing.
  Serial.println("Set the A/C to fan only with a low speed, & no swing ...");
  //ac.setSwing(false);
  ac.setMode(kElectraAcFan);
  ac.setFan(kElectraAcFanLow);
  ac.send();
  printState();
  delay(15000);

  // Turn the A/C unit off.
  Serial.println("Turn off the A/C ...");
  ac.off();
  ac.send();
  printState();
  delay(15000);  // wait 15 seconds
}