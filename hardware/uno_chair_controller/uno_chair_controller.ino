#include <ArduinoJson.h>
#include <SoftwareSerial.h>

SoftwareSerial espSerial(A0, A1); 

int IN1 = 2; int IN2 = 4; int IN3 = 7; int IN4 = 8;
const int buzzerPin = 13;

#define TRIG_FRONT 6
#define ECHO_FRONT 5
#define TRIG_BACK  3
#define ECHO_BACK  9
#define BATTERY_PIN A2

char helmetCmd = 'S';
unsigned long lastSignalTime = 0;
unsigned long lastFirebaseUpdate = 0;
const int safetyDistance = 20;

bool helmetConnectedOnce = false;

int getBatteryPercentage() {
  int raw = analogRead(BATTERY_PIN);
  int pct = map(raw, 700, 1023, 0, 100);
  return constrain(pct, 0, 100);
}

long getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW); delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 20000);
  if (duration == 0) return 999;
  return duration * 0.034 / 2;
}


void playStartupChime() {
  int notes[]    = {1000, 1300, 1700};
  int durations[] = {90, 90, 160};
  for (int i = 0; i < 3; i++) {
    tone(buzzerPin, notes[i], durations[i]);
    delay(durations[i] + 30);
  }
  noTone(buzzerPin);
}

void setup() {
  Serial.begin(115200);  
  espSerial.begin(9600); 

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  pinMode(TRIG_FRONT, OUTPUT); pinMode(ECHO_FRONT, INPUT);
  pinMode(TRIG_BACK, OUTPUT);  pinMode(ECHO_BACK, INPUT);
  pinMode(BATTERY_PIN, INPUT);

  digitalWrite(buzzerPin, LOW);
  lastSignalTime = millis();

  playStartupChime();  

void loop() {
  
  if (Serial.available() > 0) {
    char rec = Serial.read();
    if (rec == 'F' || rec == 'B' || rec == 'L' || rec == 'R' || rec == 'S' || rec == 'X' || rec == 'W') {
      helmetCmd = rec;
      lastSignalTime = millis();
      helmetConnectedOnce = true;
    }
  }

 
  if (espSerial.available() > 0) {
    char espCmd = espSerial.read();
    if (espCmd == 'M') {
      for (int i = 0; i < 3; i++) {
        digitalWrite(buzzerPin, HIGH); delay(150);
        digitalWrite(buzzerPin, LOW);  delay(100);
      }
    }
  }

  long distFront = getDistance(TRIG_FRONT, ECHO_FRONT);
  long distBack  = getDistance(TRIG_BACK, ECHO_BACK);
  int  currentBattery = getBatteryPercentage();

  bool signalLost = helmetConnectedOnce && (millis() - lastSignalTime > 1500);

 
  if (helmetCmd == 'X' || helmetCmd == 'W' || signalLost) {
    stopMotors();
    digitalWrite(buzzerPin, HIGH);
  }
  else if (helmetCmd == 'F' && distFront < safetyDistance) {
    stopMotors();
    digitalWrite(buzzerPin, HIGH); delay(80); digitalWrite(buzzerPin, LOW);
  }
  else if (helmetCmd == 'B' && distBack < safetyDistance) {
    stopMotors();
    digitalWrite(buzzerPin, HIGH); delay(80); digitalWrite(buzzerPin, LOW);
  }
  else {
    executeCommand(helmetCmd);
    digitalWrite(buzzerPin, LOW);
  }

  
  if (millis() - lastFirebaseUpdate > 400) {
    StaticJsonDocument<250> doc;
    doc["distance"] = distFront;
    doc["battery"]  = currentBattery;

    if      (helmetCmd == 'F' && distFront >= safetyDistance) doc["direction"] = "forward";
    else if (helmetCmd == 'B' && distBack  >= safetyDistance) doc["direction"] = "backward";
    else if (helmetCmd == 'L') doc["direction"] = "left";
    else if (helmetCmd == 'R') doc["direction"] = "right";
    else doc["direction"] = "stop";

    doc["fall"]        = (helmetCmd == 'X' || helmetCmd == 'W');
    doc["anomaly"]      = (helmetCmd == 'W');
    doc["signal_lost"]  = signalLost;

    serializeJson(doc, espSerial);
    espSerial.println();
    lastFirebaseUpdate = millis();
  }
}

void executeCommand(char cmd) {
  if      (cmd == 'F') { digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); }
  else if (cmd == 'B') { digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); }
  else if (cmd == 'L') { digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); }
  else if (cmd == 'R') { digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); }
  else stopMotors();
}

void stopMotors() { digitalWrite(IN1, LOW); digitalWrite(IN2, LOW); digitalWrite(IN3, LOW); digitalWrite(IN4, LOW); }
