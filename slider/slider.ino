#include <AccelStepper.h>
#include <FiniteStateMachine.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
//#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <LinkedList.h>

AccelStepper stepper(1, 5, 4);

ESP8266WebServer server(80);


const int LED_PIN = LED_BUILTIN;
const int ENDSTOP_PIN = 12;
const long STEP_COUNT = 105000L;
int motorStatus = 0;
int motorSpeed = 4000;
int loopEnabled = 0;
int endstopStatus;
long currentPhotoTime;
int photoDelay = 0;
int photoCount = 0;

LinkedList<long> photoStepList = LinkedList<long>();


void idleUpdate();
void errorEnter();
void initUpdate();
void stopUpdate();
void leftEnter();
void leftUpdate();
void findHomeEnter();
void findHomeUpdate();
void homeEnter();
void homeUpdate();
void loopEnter();
void loopUpdate();
void calculatePhotoLoopUpdate();
void photoLoopEnter();
void photoLoopUpdate();
void photoEnter();
void photoUpdate();


State idleState = State(idleUpdate);  //no operation
State errorState = State(errorEnter, NULL, NULL);
State initState = State(initUpdate);
State stopState = State(stopUpdate);
State leftState = State(leftEnter, leftUpdate, NULL);
State findHomeState = State(findHomeEnter, findHomeUpdate, NULL);
State homeState = State(homeEnter, homeUpdate, NULL);
State loopState = State(loopEnter, loopUpdate, NULL);
State calculatePhotoLoopState = State(calculatePhotoLoopUpdate);
State photoLoopState = State(photoLoopEnter, photoLoopUpdate, NULL);
State photoState = State(photoEnter, photoUpdate, NULL);
FSM stateMachine = FSM(initState);


void setup() {
  pinMode(LED_PIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  pinMode(ENDSTOP_PIN, INPUT);

  Serial.begin(115200);
  delay(100);

  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println();

  Serial.println("Start");


}

// the loop function runs over and over again forever
void loop() {
  stateMachine.update();
  stepper.run();
  server.handleClient();

  endstopStatus = analogRead(ENDSTOP_PIN);

}




///////////////////////////
// callback functions
///////////////////////////
void rootCallback() {
  // Just serve the index page from SPIFFS when asked for
  File index = SPIFFS.open("/index.html", "r");
  server.streamFile(index, "text/html");
  index.close();
}


// A function which sends the LED status back to the client
void sendStatusCallback() {
  if (motorStatus == 1) {
    server.send(200, "text/plain", "RUN");
  }
  else {
    server.send(200, "text/plain", "STOP");
  }
}


void stopCallback() {
  stateMachine.transitionTo(stopState);
  motorStatus = 0;
  loopEnabled = 0;
  sendStatusCallback();
}


void leftCallback() {
  stateMachine.transitionTo(leftState);
  motorStatus = 1;
  loopEnabled = 0;
  sendStatusCallback();
}

void rightCallback() {
  stateMachine.transitionTo(homeState);
  motorStatus = 1;
  loopEnabled = 0;
  sendStatusCallback();
}

void homeCallback() {
  stateMachine.transitionTo(homeState);
  motorStatus = 1;
  loopEnabled = 0;
  sendStatusCallback();
}

void loopCallback() {
  stateMachine.transitionTo(homeState);
  motorStatus = 1;
  loopEnabled = 1;
  sendStatusCallback();
}

void settingsCallback() {
  //stateMachine.transitionTo(homeState);
  //motorStatus = 1;
  Serial.println("Settings");
  if (server.hasArg("speed")) {
    motorSpeed = server.arg("speed").toInt();
  }
  if (server.hasArg("photoDelay")) {
    photoDelay = server.arg("photoDelay").toInt();
  }
  if (server.hasArg("photoCount")) {
    photoCount = server.arg("photoCount").toInt();
  }

  Serial.print("Motor speed:");
  Serial.println(motorSpeed);
  Serial.print("Photo delay:");
  Serial.println(photoDelay);
  Serial.print("Photo count:");
  Serial.println(photoCount);
  stepper.setMaxSpeed(motorSpeed);
  sendStatusCallback();
}











//////////////////////////////////
//state functions
//////////////////////////////////

void initUpdate() {

  Serial.println("Init.");
  Serial.println("Init stepper.");
  //Stepper
  stepper.setMaxSpeed(motorSpeed);
  stepper.setAcceleration(4000);
  //stepper.setSpeed(motorSpeed);
  Serial.print("Speed:");
  Serial.println(motorSpeed);

  Serial.println("Init FS.");
  //FS
  SPIFFS.begin();

  Serial.println("Init Wifi.");
  //WIFI
  WiFi.mode(WIFI_AP);

  // Do a little work to get a unique-ish name. Append the
  // last two bytes of the MAC (HEX'd) to "Thing-":
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);
  String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
                 String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
  macID.toUpperCase();
  String AP_NameString = "Slider " + macID;

  char AP_NameChar[AP_NameString.length() + 1];
  memset(AP_NameChar, 0, AP_NameString.length() + 1);

  for (int i = 0; i < AP_NameString.length(); i++)
    AP_NameChar[i] = AP_NameString.charAt(i);

  WiFi.softAP(AP_NameChar, NULL);

  Serial.println("Start WIFI AP.");

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());


  if (!MDNS.begin("s")) {
    Serial.println("Error setting up MDNS responder!");
    stateMachine.transitionTo(errorState);

    return;

  }
  Serial.println("mDNS responder started");

  // Start TCP (HTTP) server

  server.on("/", rootCallback);
  // Handlers for various user-triggered events
  server.on("/stop", stopCallback);
  server.on("/left", leftCallback);
  server.on("/right", rightCallback);
  server.on("/home", homeCallback);
  server.on("/loop", loopCallback);
  server.on("/settings", settingsCallback);
  server.on("/status", sendStatusCallback);

  server.begin();
  Serial.println("TCP server started");

  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);

  stateMachine.transitionTo(findHomeState);
  //stateMachine.transitionTo(idleState);

}


void stopUpdate() {

  Serial.println("Motor stop");
  motorStatus = 0;
  digitalWrite(LED_PIN, LOW);
  stepper.stop();
  stateMachine.transitionTo(idleState);

}


void leftEnter() {
  Serial.println("Motor left start");
  motorStatus = 1;
  digitalWrite(LED_PIN, HIGH);

  stepper.moveTo(STEP_COUNT);

}

void leftUpdate() {
  if (stepper.targetPosition() == stepper.currentPosition()) {
    stateMachine.transitionTo(stopState);
  }
}





void findHomeEnter() {
  Serial.println("Find home");
  motorStatus = 1;
  digitalWrite(LED_PIN, HIGH);

  stepper.moveTo(-2 * STEP_COUNT);
}

void findHomeUpdate() {

  Serial.println(endstopStatus);

  if (endstopStatus == 1023) {
    stepper.stop();
    stepper.setCurrentPosition(0);
    stateMachine.transitionTo(stopState);

  }

}



void homeEnter() {
  Serial.println("Go home");
  motorStatus = 1;
  digitalWrite(LED_PIN, HIGH);

  stepper.moveTo(0);
}

void homeUpdate() {

  if (endstopStatus == 1023) {
    stepper.setCurrentPosition(0);
  }

  if (stepper.targetPosition() == stepper.currentPosition()) {
    if (loopEnabled == 1) {

      if (photoDelay != 0 && photoCount != 0) {
        stateMachine.transitionTo(calculatePhotoLoopState);
      } else {
        stateMachine.transitionTo(loopState);
      }
    } else {
      stateMachine.transitionTo(stopState);
    }
  }

}





void loopEnter() {
  Serial.println("Loop");
  motorStatus = 1;
  digitalWrite(LED_PIN, HIGH);

  stepper.moveTo(STEP_COUNT);
}

void loopUpdate() {

  if (stepper.targetPosition() == stepper.currentPosition()) {
    //stepper.stop();
    loopEnabled = 0;
    stateMachine.transitionTo(homeState);
  }

}



void calculatePhotoLoopUpdate() {
  Serial.println("Calculate steps for photo loop.");
  photoStepList.clear();
  for (int i = 0; i < photoCount; i++) {
    photoStepList.add(STEP_COUNT / photoCount * (i));
  }
  stateMachine.transitionTo(photoLoopState);
}



void photoLoopEnter() {
  Serial.println("Photo Loop");

  stepper.moveTo(photoStepList.get(0));
}

void photoLoopUpdate() {

  if (stepper.targetPosition() == stepper.currentPosition()) {
    photoStepList.shift();
    stateMachine.transitionTo(photoState);

  }
}

void photoEnter() {
  Serial.println("Photo");

  currentPhotoTime = millis();
}

void photoUpdate() {

  if (millis() - currentPhotoTime >= photoDelay * 1000) {

    if (photoStepList.size() == 0) {
      stateMachine.transitionTo(homeState);
      loopEnabled = 0;
    } else {
      stateMachine.transitionTo(photoLoopState);
    }


  }
}



void idleUpdate() {

  //Serial.println(digitalRead(12));

  //delay(300);

}


void errorEnter() {

  Serial.println("Error.");

}







