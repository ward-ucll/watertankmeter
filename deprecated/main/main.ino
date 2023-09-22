// oled settings
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//sonar
const int trigPin = 19; // Trigger Pin of Ultrasonic Sensor
const int echoPin = 21; // Echo Pin of Ultrasonic Sensor
int distance;
long duration;

//wifi
#include <WiFi.h>
const char* ssid       = "6op106";
const char* password   = "5sranijn";

const int led_RED = 13;
const int led_GREEN = 12;
const int led_BLUE = 14;

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

void setup() {
  analogWrite(led_RED, 255);
  Serial.begin(9600); // Starting Serial Terminal

  //display
  Wire.begin(23, 22); // Initialize the I2C interface with SDA on GPIO 21 and SCL on GPIO 22

  //wifi
  initWiFi();
  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  //sonar
  pinMode(echoPin, INPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(led_BLUE, OUTPUT);
  pinMode(led_GREEN, OUTPUT);
  pinMode(led_RED, OUTPUT);
}

void loop() {
  display.clearDisplay();
  distance = getDistance();
  Serial.print(distance);
  Serial.println("cm");

  //wifi
  if (WiFi.status() != WL_CONNECTED) {
    analogWrite(led_RED, 255);
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
  }
  else{
    analogWrite(led_GREEN, 255);
  }


  displayText(String(distance) + "cm", 12, "middle");
  display.display();
  delay(100);
}

int getDistance() {
  int distance;
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  distance = duration / 29 / 2;
  return distance;
}

void displayText(String text, int yOffset, String place) {
  int textWidth = text.length() * 12; // Assuming each character is 12 pixels wide with textSize 2
  int textHeight = 16; // Assuming each character is 16 pixels high with textSize 2
  
  int y = (32 - textHeight) / 2 + yOffset; // Calculate the starting y-coordinate

  int x = 0;

  if (place == "middle"){
    x = (128 - textWidth) / 2; // Calculate the starting x-coordinate
  }
  if (place == "right"){
    x = 128 - textWidth; // Calculate the starting x-coordinate
  }

  display.setTextSize(2); // Set the text size to 2
  display.setTextColor(WHITE); // Set the text color to white

  int textX = x;
  int textY = y + ((32 - textHeight) / 2) - 4; // Adjust the y-coordinate to center the text vertically

  display.setCursor(textX, textY); // Set the cursor to the adjusted coordinates

  display.println(text); // Print the text
}
