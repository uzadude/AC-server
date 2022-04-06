#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Airwell.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DHTesp.h"
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Update.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

#define AUTO_MODE kAirwellAuto
#define COOL_MODE kAirwellCool
#define DRY_MODE kAirwellDry
#define HEAT_MODE kAirwellHeat
#define FAN_MODE kAirwellFan

#define FAN_AUTO kAirwellFanAuto
#define FAN_MIN kAirwellFanLow
#define FAN_MED kAirwellFanMedium
#define FAN_HI kAirwellFanHigh

Adafruit_SSD1306 display(128, 64, &Wire, -1);

DHTesp dht;
//Define the DHT object
int dhtPin = 13;//Define the dht pin
float roomTemp = 0;
float roomHumidity = 0;
unsigned long lastPrintMS = 0;

const uint16_t kIrLed = 4;  // ESP8266 GPIO pin to use. Recommended: 4 (D2).
IRAirwellAc ac(kIrLed);     // Set the GPIO used for sending messages.

struct state {
  uint8_t temperature = 22, fan = 0, operation = 0;
  bool powerStatus;
  bool prevPowerStatus;
};

state acState;

#define NUM_HIST_POINTS         256
#define HIST_SAMPLE_INTERVAL  60000
#define HIST_DOWNSAMPLE_FACTOR    6
unsigned long updateDataMillis = millis();

struct dataMeasures {
    unsigned long *time_millis;
    double *tmp;
    double *hum;
};

dataMeasures measures;
int rowCount = 0;

File fsUploadFile;

char deviceName[] = "AC Remote Control";
const char* ssid = "";
const char* password = "";

WebServer server(80);

String getContentType(String filename) {
  // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path) {
  //  send the right file to the client (if it exists)
  // Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";
  // If a folder is requested, send the index file
  String contentType = getContentType(path);
  // Get the MIME type
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
    // If the file exists, either as a compressed archive, or normal
    // If there's a compressed version available
    if (SPIFFS.exists(pathWithGz))
      path += ".gz";  // Use the compressed verion
    File file = SPIFFS.open(path, "r");
    //  Open the file
    server.streamFile(file, contentType);
    //  Send it to the client
    file.close();
    // Close the file again
    // Serial.println(String("\tSent file: ") + path);
    return true;
  }
  // Serial.println(String("\tFile Not Found: ") + path);
  // If the file doesn't exist, return false
  return false;
}

void handleFileUpload() {  // upload a new file to the SPIFFS
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    // Serial.print("handleFileUpload Name: "); //Serial.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");
    // Open the file for writing in SPIFFS (create if it doesn't exist)
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
      // Write the received bytes to the file
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {
      // If the file was successfully created
      fsUploadFile.close();
      // Close the file again
      // Serial.print("handleFileUpload Size: ");
      // Serial.println(upload.totalSize);
      server.sendHeader("Location", "/success.html");
      // Redirect the client to the success page
      server.send(303);
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void printState() {
  // Display the settings.
  Serial.println("Airwell A/C remote is in the following state:");
  Serial.printf("  %s\n", ac.toString().c_str());
  Serial.print("WiFi: ");
  Serial.println(WiFi.localIP()); // Print the IP address

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

void addArray(DynamicJsonDocument &doc, std::string name, double arr[]) {
  JsonArray data = doc.createNestedArray(name);
  
  int tsize = rowCount;
  for(int i = 0; i < tsize; i++){
      data.add(((int)(arr[i]*100))/100.0);
  }
}

void addArray(DynamicJsonDocument &doc, std::string name, unsigned long arr[]) {
  JsonArray data = doc.createNestedArray(name);
  
  int tsize = rowCount;
  for(int i = 0; i < tsize; i++){
      data.add(arr[i]);
  }
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

  Serial.println("mounting SPIFFS...");
    if (!SPIFFS.begin()) {
    // Serial.println("Failed to mount file system");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  if(WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Connect Failed! Rebooting...");
    delay(1000);
    ESP.restart();
  }  
  Serial.println(WiFi.localIP()); // Print the IP address

  server.on("/state", HTTP_PUT, []() {
    DynamicJsonDocument root(1024);
    DeserializationError error = deserializeJson(root, server.arg("plain"));
    if (error) {
      Serial.println("error!");
      server.send(404, "text/plain", "FAIL. " + server.arg("plain"));
    } else {
      if (root.containsKey("temp")) {
        acState.temperature = (uint8_t) root["temp"];
      }

      if (root.containsKey("fan")) {
        acState.fan = (uint8_t) root["fan"];
      }

      if (root.containsKey("power")) {
        acState.prevPowerStatus = acState.powerStatus;
        acState.powerStatus = root["power"];
      }

      if (root.containsKey("mode")) {
        acState.operation = root["mode"];
      }

      String output;
      serializeJson(root, output);
      server.send(200, "text/plain", output);
      Serial.println("got request:");
      Serial.println(output);

      delay(200);

      ac.setPowerToggle(acState.powerStatus != acState.prevPowerStatus);

      if (acState.powerStatus) {
        ac.setTemp(acState.temperature);
        if (acState.operation == 0) {
          ac.setMode(AUTO_MODE);
          ac.setFan(FAN_AUTO);
          acState.fan = 0;
        } else if (acState.operation == 1) {
          ac.setMode(COOL_MODE);
        } else if (acState.operation == 2) {
          ac.setMode(DRY_MODE);
        } else if (acState.operation == 3) {
          ac.setMode(HEAT_MODE);
        } else if (acState.operation == 4) {
          ac.setMode(FAN_MODE);
        }

        if (acState.operation != 0) {
          if (acState.fan == 0) {
            ac.setFan(FAN_AUTO);
          } else if (acState.fan == 1) {
            ac.setFan(FAN_MIN);
          } else if (acState.fan == 2) {
            ac.setFan(FAN_MED);
          } else if (acState.fan == 3) {
            ac.setFan(FAN_HI);
          }
        }
      }
      ac.send();
    }
  });

  server.on("/file-upload", HTTP_POST,
  // if the client posts to the upload page
  []() {
    // Send status 200 (OK) to tell the client we are ready to receive
    server.send(200);
  },
  handleFileUpload);  // Receive and save the file

  server.on("/file-upload", HTTP_GET, []() {
    // if the client requests the upload page

    String html = "<form method=\"post\" enctype=\"multipart/form-data\">";
    html += "<input type=\"file\" name=\"name\">";
    html += "<input class=\"button\" type=\"submit\" value=\"Upload\">";
    html += "</form>";
    server.send(200, "text/html", html);
  });

  server.on("/", []() {
    server.sendHeader("Location", String("ui.html"), true);
    server.send(302, "text/plain", "");
  });

  server.on("/state", HTTP_GET, []() {
    DynamicJsonDocument root(1024);
    root["mode"] = acState.operation;
    root["fan"] = acState.fan;
    root["temp"] = acState.temperature;
    root["power"] = acState.powerStatus;
    String output;
    serializeJson(root, output);
    server.send(200, "text/plain", output);
  });

  server.on("/data", HTTP_GET, []() {
    //Serial.println("got data request");

    // trying to estimate the json size
    DynamicJsonDocument doc((rowCount+1) * 100);
    dataMeasures dm = measures;
    addArray(doc, "time_millis", dm.time_millis);
    addArray(doc, "tmp", dm.tmp);
    addArray(doc, "hum", dm.hum);

    String output;
    serializeJson(doc, output);
    //Serial.println(output);
    server.send(200, "text/plain", output);
  });

  server.on("/reset", []() {
    server.send(200, "text/html", "reset");
    delay(100);
    ESP.restart();
  });

  server.serveStatic("/", SPIFFS, "/", "max-age=86400");

  server.onNotFound(handleNotFound);

  server.begin();

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

  measures.time_millis = new unsigned long [NUM_HIST_POINTS];
  measures.tmp = new double [NUM_HIST_POINTS];
  measures.hum = new double [NUM_HIST_POINTS];

	// Display Text
	display.setTextSize(1);
	display.setTextColor(WHITE);
	display.setCursor(0,28);
	display.println("Hello world!");
	display.display();

}

void downsample() {
  if (rowCount>=NUM_HIST_POINTS) {

    Serial.println("Down-sampling data");
    for (int i = 0; i < rowCount - rowCount / HIST_DOWNSAMPLE_FACTOR ; i++) {

        int newRow = i * HIST_DOWNSAMPLE_FACTOR/(HIST_DOWNSAMPLE_FACTOR-1) + 1;

        measures.time_millis[i] = measures.time_millis[newRow];
        measures.tmp[i] = measures.tmp[newRow];
        measures.hum[i] = measures.hum[newRow];
      }
      //reset rowCount to the new, smaller number so any new writes will start after this culling
      rowCount = rowCount - rowCount / HIST_DOWNSAMPLE_FACTOR;
      //Serial.println(rowCount);
    }

}

void addMeasurement(double tmp, double hum) {
  if (millis() - updateDataMillis > HIST_SAMPLE_INTERVAL && updateDataMillis<3131922040) {
    updateDataMillis = millis();
    measures.time_millis[rowCount] = updateDataMillis;
    measures.tmp[rowCount] = tmp;
    measures.hum[rowCount] = hum;
    rowCount++;
  }
  downsample();
}

void loop() {
  server.handleClient();

  if (millis() - lastPrintMS > 10000) {
    flag:TempAndHumidity newValues = dht.getTempAndHumidity();
    if (dht.getStatus() != 0) {
      goto flag;
    }

    roomTemp = newValues.temperature;
    roomHumidity = newValues.humidity;
    addMeasurement(newValues.temperature, newValues.humidity);

    printState();
    lastPrintMS = millis();
  }
  delay(1000);
}