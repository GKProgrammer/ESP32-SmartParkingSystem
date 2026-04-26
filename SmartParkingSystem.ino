#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

// --- Wi-Fi Credentials ---
const char* ssid = "SSID";
const char* password = "Password";

// --- OLED Display Setup ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Pin Definitions ---
const int irSlotPins[4] = {32, 33, 25, 26}; // P01, P02, P03, P04
const int ledPins[4] = {19, 18, 5, 17};     // P01, P02, P03, P04
const int irGatePin = 27;
const int servoPin = 16;

Servo gateServo;
WebServer server(80);

// --- Global Logic Variables ---
bool slotStatus[4] = {false, false, false, false}; // true = occupied, false = available
int availableSlots = 4;

// Non-blocking timer variables for the gate
unsigned long gateTimer = 0;
bool isGateOpen = false;
const unsigned long gateOpenDuration = 3000; // Keep gate open for 3 seconds

void setup() {
  Serial.begin(115200);

  // Initialize Sensors and LEDs
  for (int i = 0; i < 4; i++) {
    pinMode(irSlotPins[i], INPUT_PULLUP);
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW); // Ensure LEDs start off
  }
  pinMode(irGatePin, INPUT);

  // Initialize Servo
  gateServo.attach(servoPin);
  gateServo.write(0); // 0 degrees = Gate Closed

  // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Loop forever if display fails
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  display.println("Connecting WiFi...");
  display.display();

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  // Setup Web Server Routes
  server.on("/", handleRoot);
  server.begin();

  updateOLED(); // Initial UI draw
}

void loop() {
  server.handleClient();       // Listen for HTTP requests
  checkParkingSlots();         // Read slot sensors & update LEDs
  handleGateLogic();           // Handle entry gate
  updateOLED();                // Refresh physical display
  delay(50);                   // Small delay for stability
}

// --- Core Logic Functions ---

void checkParkingSlots() {
  availableSlots = 0;
  for (int i = 0; i < 4; i++) {
    // Assuming IR sensor outputs LOW when object (car) is detected
    bool isOccupied = (digitalRead(irSlotPins[i]) == LOW);
    slotStatus[i] = isOccupied;

    // Turn White LED ON if occupied, OFF if available
    digitalWrite(ledPins[i], isOccupied ? HIGH : LOW);

    if (!isOccupied) {
      availableSlots++;
    }
  }
}

void handleGateLogic() {
  bool carAtGate = (digitalRead(irGatePin) == LOW);

  // Open gate IF car is waiting AND there is space AND gate isn't already open
  if (carAtGate && availableSlots > 0 && !isGateOpen) {
    gateServo.write(90); // 90 degrees = Gate Open
    isGateOpen = true;
    gateTimer = millis(); 
  }

  // Handle closing the gate with a non-blocking timer
  if (isGateOpen && (millis() - gateTimer >= gateOpenDuration)) {
    if (!carAtGate) { 
      // Only close if the car has actually moved past the sensor
      gateServo.write(0); 
      isGateOpen = false;
    } else {
      // Reset timer if the car is still sitting in the gate
      gateTimer = millis(); 
    }
  }
}

void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  // Display Server IP
  display.print("Server IP:");
  display.setCursor(0, 10);
  display.println(WiFi.localIP());
  display.drawLine(0, 20, 128, 20, WHITE);

  // Display Slot Status
  display.setCursor(0, 25);
  if (availableSlots == 0) {
    display.println("No slots available");
    display.println("at the moment");
  } else {
    display.println("Available Slots:");
    display.setCursor(0, 40);
    display.setTextSize(1);
    String availableStr = "";
    for (int i = 0; i < 4; i++) {
      if (!slotStatus[i]) {
        availableStr += "P0" + String(i + 1) + " ";
      }
    }
    display.println(availableStr);
  }
  display.display();
}

void handleRoot() {
  // Build a simple, clean Web UI using HTML and embedded CSS
  String html = "<!DOCTYPE html><html><head><title>Smart Parking Dashboard</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<meta http-equiv='refresh' content='3'> "; // Auto-refresh the page every 3 seconds
  html += "<style>body{font-family:sans-serif; text-align:center; background-color:#121212; color:white; margin-top:40px;}";
  html += ".slot{display:inline-block; padding:20px; margin:10px; border-radius:8px; width:100px; font-weight:bold; font-size:1.2rem;}";
  html += ".available{background-color:#28a745;} .occupied{background-color:#dc3545;}</style></head><body>";
  
  html += "<h1>Smart Parking System</h1>";

  if (availableSlots == 0) {
     html += "<h2 style='color:#ffc107;'>No slots available at the moment</h2>";
  } else {
     html += "<h2>Total Available Slots: " + String(availableSlots) + "</h2>";
  }

  html += "<div>";
  for (int i = 0; i < 4; i++) {
    String slotName = "P0" + String(i + 1);
    if (!slotStatus[i]) {
      html += "<div class='slot available'>" + slotName + "<br><span style='font-size:0.8rem; font-weight:normal;'>Available</span></div>";
    } else {
      html += "<div class='slot occupied'>" + slotName + "<br><span style='font-size:0.8rem; font-weight:normal;'>Occupied</span></div>";
    }
  }
  html += "</div></body></html>";

  server.send(200, "text/html", html);
}
