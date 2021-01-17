

#include <ESP32Servo.h>
#include <SPIFFS.h>
#include <WiFiSettings.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <Restfully.h>
#include <EEPROM.h>
#include <timeout.h>

#define EEPROM_SIZE 1

Servo myservo;
int servoPin = 18;
char _hostString[32] = {0};
WebServer server(5000);
RestRequestHandler restHandler;
Timeout timeout;
int timeoutWhatToDo = 0;


void setup() {
  EEPROM.begin(EEPROM_SIZE);
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  myservo.setPeriodHertz(50);
  myservo.attach(servoPin, 500, 2400);

  Serial.begin(115200);
  SPIFFS.begin(true);  // On first run, will format after failing to mount

  WiFiSettings.onSuccess  = []() { startRESTServer(); };

  WiFiSettings.connect();

  
}

void startRESTServer() {
  if (!MDNS.begin("tulikivi2"))
    {
        Serial.println("Error setting up MDNS responder!");
    }
    Serial.println("mDNS responder started");
    MDNS.addService("http", "tcp", 5000);
    MDNS.addServiceTxt("http", "tcp", "netwalkType", "chimney");
    MDNS.addServiceTxt("http", "tcp", "ssl", "5001");
    MDNS.addServiceTxt("http", "tcp", "configuration", "{\"name\":\"Chimney\", \"location\":\"\", \"status\":\"UNKNOWN\"}");

    server.begin();
    server.on("/", handleRoot);
    server.addHandler(&restHandler);

    restHandler.on("/chimney/open").GET(handleOpenChimney);
    restHandler.on("/chimney/close").GET(handleCloseChimney);
    restHandler.on("/chimney/open-timeout/:timeout(integer)").GET(handleOpenChimneyTimeout);
    restHandler.on("/chimney/close-timeout/:timeout(integer)").GET(handleCloseChimneyTimeout);
}

void handleRoot() {
  String html = "<html><head><title>Simple Rest Server</title>";
  html += "</head>";
  html += "<body>";
  // header
  html += "<h1>Simple Rest Server</h1>";
  // title area
  html += "<div class='title'><h2><label>Site</label> ";
  html += "hostname";
  html += "</h2></div>";
  // ... add more here ...
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void openChimney() {
  if (EEPROM.read(0) == 1)
    return;
    
  for (int pos = 0; pos <= 90; pos += 1) { // goes from 0 degrees to 180 degrees
    // in steps of 1 degree
    myservo.write(pos);    // tell servo to go to position in variable 'pos'
    delay(15);             // waits 15ms for the servo to reach the position
  }
  EEPROM.write(0, 1);
  EEPROM.commit();
  MDNS.addServiceTxt("http", "tcp", "configuration", "{\"name\":\"Chimney\", \"location\":\"\", \"status\":\"OPEN\"}");
}

void closeChimney() {
  if (EEPROM.read(0) == 0)
    return;

  for (int pos = 90; pos >= 0; pos -= 1) { // goes from 180 degrees to 0 degrees
    myservo.write(pos);    // tell servo to go to position in variable 'pos'
    delay(15);             // waits 15ms for the servo to reach the position
  }
  EEPROM.write(0, 0);
  EEPROM.commit();
  MDNS.addServiceTxt("http", "tcp", "configuration", "{\"name\":\"Chimney\", \"location\":\"\", \"status\":\"CLOSED\"}");
}

int handleOpenChimney(RestRequest& request) {
  openChimney();
  return 200;
}
int handleOpenChimneyTimeout(RestRequest& request) {
  long timeoutValue = request["timeout"];
  timeout.start(timeoutValue * 1000);
  timeoutWhatToDo = 1;
  return 200;
}
int handleCloseChimney(RestRequest& request) {
  closeChimney();
  return 200;
}
int handleCloseChimneyTimeout(RestRequest& request) {
  long timeoutValue = request["timeout"];
  timeout.start(timeoutValue * 1000);
  timeoutWhatToDo = 2;
  return 200;
}


void loop() {
  server.handleClient();

  if (timeout.time_over()) {
    timeout.reset();
    if (timeoutWhatToDo == 1) {
      openChimney();
    }
    else if (timeoutWhatToDo == 2) {
      closeChimney();
    }
    timeoutWhatToDo = 0;
    
  }
}
