
///////////////////////////////////////////////////////
// Example IoT temperature sensor code for the esp32 //
//          IoT in Manufacturing Industry            //
//             University of Limerick                //
// Mihai Penica                                      //
// AngelDev0                                         // 
///////////////////////////////////////////////////////

#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// OLED Screen configuration
#define SCREEN_WIDTH   128   // OLED display width in pixels
#define SCREEN_HEIGHT  64    // OLED display height in pixels
#define OLED_RESET     -1    // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C  // See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// WiFi credentials
#define WIFI_SSID "Wokwi-GUEST"   // If using on a real esp32 use your own credentials
#define WIFI_PASSWORD ""          // If using on a real esp32 use your own credentials

// Server conection
#define SERVER_IP "192.168.x.x"   // CHANGE THIS TO YOUR SERVER'S IP
#define SERVER_PORT "5000"        // CHANGE THIS TO YOUR SERVER'S PORT NUMBER

// Constants for ADC and LM35 sensor
#define ADC_VREF_mV     3300.0    // in millivolt
#define ADC_RESOLUTION  4096.0
#define ONWIRE_PIN      12        // ESP32-S2 pin GPIO12 (ADC0) connected to sensor

// For esp32 devkit-c
//#define ONWIRE_PIN      36        // ESP32-DevKitC-1 pin GPIO36 (ADC0) connected to sensor

// Server details
const String server = SERVER_IP;
const String resource = "/predict_outcome";
const String portNumber = SERVER_PORT;
const unsigned long HTTP_TIMEOUT = 10000;   // max response time from server

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONWIRE_PIN);
// Pass oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

// Variables to store temperature readings
float temperatureC = 0;
float temperatureF = 0;
float tempArray[5] = {0, 0, 0, 0, 0}; // Array to store temperature readings
int tempIndex = 0; // Index to keep track of the current position in the array
unsigned long lastSendTime = 0; // Last time data was sent
unsigned long lastTempReadTime = 0; // Last time temperature was read

// HTTP client instance
HTTPClient http;

void setup() {
  Serial.begin(115200); // Initialize serial communication at 115200 baud rate
  Wire.begin(20, 21);
  
  // Send ereror if display address alocation fails
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }

  // Start temperature sensor
  sensors.begin();

  // Connect to wifi (OLED)
  display.clearDisplay();
  display.setTextSize(1);           
  display.setTextColor(SSD1306_WHITE);      
  display.setCursor(20,0); 
  display.println("Connecting to ");
  display.setCursor(20,15);
  display.println(WIFI_SSID);
  display.display();

  // Connect to WiFi
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  delay(500);
  // Serial.println("Connecting to WiFi...");
  display.clearDisplay();
  display.setCursor(20,20);
  display.println("Connecting...");
  display.display();
  
  // Wait for the WiFi to connect
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  
  Serial.println();
  // Print WiFi details once connected
  // OLED
  display.clearDisplay();
  display.setCursor(20,5);
  display.println("WiFi connected");
  display.print("");
  display.print("MAC : ");
  display.println(WiFi.macAddress());
  display.println("");
  display.print("IP : ");
  display.println(WiFi.localIP());
  display.print("DNS0: ");
  display.println(WiFi.dnsIP(0));
  display.print("DNS1: ");
  display.println(WiFi.dnsIP(1));
  display.display();

  delay(2000);

  // Serial
  Serial.print("WiFi (");
  Serial.print(WiFi.macAddress());
  Serial.print(") connected with IP ");
  Serial.println(WiFi.localIP());
  Serial.print("DNS0: ");
  Serial.println(WiFi.dnsIP(0));
  Serial.print("DNS1: ");
  Serial.println(WiFi.dnsIP(1));

  // Print information about server connection
  // Oled
  display.clearDisplay();
  display.setCursor(30,0);
  display.println("Connected");
  display.println("");
  display.print("ESP will connect to server at IP ");
  display.print(SERVER_IP);
  display.print(" and port ");
  display.print(SERVER_PORT);
  display.display();

  // Serial
  Serial.println();
  Serial.print("ESP will connect to server with ip ");
  Serial.print(SERVER_IP);
  Serial.print(" at port ");
  Serial.print(SERVER_PORT);
  delay(1500);
}

void loop() {
  // Check if 6 seconds have passed to read the temperature
  if (millis() - lastTempReadTime >= 6000) {
    // Request temperature from sensor using DDallas
    sensors.requestTemperatures();

    // Convert the voltage to the temperature in °C
    float temperatureC = sensors.getTempCByIndex(0);
    // Convert the voltage to the temperature in °F
    float temperatureF = sensors.getTempFByIndex(0);
    
    // Store the temperature reading in the array
    tempArray[tempIndex] = temperatureC;
    tempIndex = (tempIndex + 1) % 5; // Move to the next index, loop back to 0 after reaching 4

    // Print sensor values for debugging
    // OLED
    display.clearDisplay();
    display.setCursor(0,0);
    display.print("IP ");
    display.println(WiFi.localIP());
    display.println("");
    display.print(temperatureC);
    display.println(" degrees C");
    display.print(temperatureF);
    display.println(" degrees F");
    display.display();

    // Serial
    Serial.println("");
    Serial.print(temperatureC);
    Serial.println(" °C");
    Serial.print(temperatureF);
    Serial.println(" °F");
    
    lastTempReadTime = millis(); // Update the last temperature read time
  }

  // Check if 30 seconds have passed to send data to the server
  if (millis() - lastSendTime >= 30000) {
    postDataToServer(); // Call the function to post data to the server
    lastSendTime = millis(); // Update the last send time
  }

  delay(1000); // Delay to prevent rapid looping
}

void postDataToServer() {
  if (WiFi.status() == WL_CONNECTED) { // Check if the WiFi is connected
    Serial.println("Posting JSON data to server...");

    // Construct the URL for the HTTP request
    String url = "http://" + server + ":" + portNumber + resource;
    http.begin(url); // Initialize the HTTP connection
    http.addHeader("Content-Type", "application/json"); // Set the content type to JSON

    // Create a JSON document to hold the data
    StaticJsonDocument<200> doc;
    doc["device_name"] = "Device1";       // Give the device a name
    doc["user_name"] = "Mihai Penica";    // Add a username

    // Add temperature array to JSON document
    JsonArray data = doc.createNestedArray("data");
    for (int i = 0; i < 5; i++) {
      data.add(tempArray[i]);
    }

    // Serialize the JSON document to a string
    String requestBody;
    serializeJson(doc, requestBody);

    // Send the HTTP POST request
    int httpResponseCode = http.POST(requestBody);

    // Check the response from the server
    if (httpResponseCode > 0) {
      String response = http.getString(); // Get the response payload
      Serial.println(httpResponseCode);
      Serial.println(response);

      // Parse the server response
      StaticJsonDocument<200> responseDoc;
      DeserializationError error = deserializeJson(responseDoc, response);

      // Check if the response parsing was successful
      if (!error) {
        const char* result = responseDoc["result"];
        // Print the result from the server response
        Serial.print("Your device is working in ");
        Serial.print(result);
        Serial.println(" conditions");
      } else {
        Serial.println("Error parsing the response");
      }
    } else {
      // Print an error message if the HTTP POST request failed
      Serial.printf("Error occurred while sending HTTP POST: %s\n", http.errorToString(httpResponseCode).c_str());
    }

    http.end(); // End the HTTP connection
  } else {
    Serial.println("WiFi not connected"); // Print a message if the WiFi is not connected
  }
}
