#include <WiFi.h>
#include <FirebaseESP32.h>
#include <ArduinoJson.h>

#define WIFI_SSID     "mostafa IT"
#define WIFI_PASSWORD "16131613"
#define FIREBASE_HOST "smartchair-b40bb-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "AIzaSyDp40s_gR8xX5rNEMqYsxtqm1io8TQuEhE"

#define PULSE_PIN        34   
#define TO_ARDUINO_PIN   25   
#define MED_REMINDER_PIN 26   

FirebaseData fbData;
FirebaseConfig fbConfig;
FirebaseAuth fbAuth;

int bpm = 0;
float distanceCm = 0;
int batteryPct = 100;
bool isFall = false;
bool isAnomaly = false;
bool signalLost = false;
String chairDirection = "stop"; 

bool pulseCheckEnable = true; 
bool wifiConnected = false;
unsigned long lastFirebaseUpdate = 0;
unsigned long lastFingerTime = 0; 

unsigned long medTriggerTime = 0;
bool medActive = false;

void setup() {
  Serial.begin(115200); 
  
  pinMode(PULSE_PIN, INPUT);
  pinMode(TO_ARDUINO_PIN, OUTPUT);
  pinMode(MED_REMINDER_PIN, OUTPUT);
  
  digitalWrite(TO_ARDUINO_PIN, LOW);   
  digitalWrite(MED_REMINDER_PIN, LOW);  

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 15) { 
    delay(500); 
    timeout++; 
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    fbConfig.host = FIREBASE_HOST;
    fbConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
    Firebase.begin(&fbConfig, &fbAuth);
    Firebase.reconnectWiFi(true);
  }
}

void loop() {
  int rawPulse = analogRead(PULSE_PIN);
  if (rawPulse > 1300) { 
    bpm = map(rawPulse, 1300, 4095, 62, 122);
    lastFingerTime = millis(); 
  } else {
    if (millis() - lastFingerTime > 3000) {
      bpm = 0;
    }
  }

  if (!pulseCheckEnable) {
    bpm = 0; 
    digitalWrite(TO_ARDUINO_PIN, LOW); 
    isAnomaly = false;
  } else {
    if (bpm == 0 || bpm < 50 || bpm > 130) {
      digitalWrite(TO_ARDUINO_PIN, HIGH);  
      isAnomaly = true;                  
    } else {
      digitalWrite(TO_ARDUINO_PIN, LOW);   
      isAnomaly = false;
    }
  }

  if (medActive && (millis() - medTriggerTime >= 5000)) {
    digitalWrite(MED_REMINDER_PIN, LOW);
    medActive = false;
    if (wifiConnected) {
      Firebase.setBool(fbData, "config/trigger_buzzer_med", false);
    }
  }

  if (wifiConnected && (millis() - lastFirebaseUpdate > 1000)) {
    Firebase.setTimestamp(fbData, "system/last_seen");

    if (Firebase.getBool(fbData, "config/pulse_check_enable")) {
      pulseCheckEnable = fbData.boolData();
    }

    if (Firebase.getBool(fbData, "config/trigger_buzzer_med")) {
      if (fbData.boolData() == true && !medActive) {
        digitalWrite(MED_REMINDER_PIN, HIGH); 
        medTriggerTime = millis();            
        medActive = true;
      } else if (fbData.boolData() == false) {
        digitalWrite(MED_REMINDER_PIN, LOW);  
        medActive = false;
      }
    }

    Firebase.setInt(fbData, "patient/heartRate", bpm);
    Firebase.setBool(fbData, "chair/isAnomaly", isAnomaly);
116117118119120121122123124125126127128129
      else if (bpm < 50 || bpm > 130) statusMsg = " تحذير: اضطراب وخروج معدل النبض عن النطاق الآمن";
    } else {

      statusMsg = " تنبيه: نظام الأمان موقوف يدوياً من الموقع الإلكتروني";
    }
    Firebase.setString(fbData, "patient/fallStatus", statusMsg);

    lastFirebaseUpdate = millis();
  }
}
Writing at 0x0013a83c [============================> ]  99.4% 786432/791148 bytes... 

Writing at 0x0013d000 [==============================] 100.0% 791148/791148 bytes... 
Wrote 1232896 bytes (791148 compressed) at 0x00010000 in 12.6 seconds (782.4 kbit/s).
Verifying written data...
Hash of data verified.

Hard resetting via RTS pin...


    
    String statusMsg = "المريض مستقر تماماً وعلاماته الحيوية سليمة";
    if (pulseCheckEnable) {
      if (bpm == 0) statusMsg = " طوارئ: خطر فقدان وعي أو نزع حساس النبض";
      else if (bpm < 50 || bpm > 130) statusMsg = " تحذير: اضطراب وخروج معدل النبض عن النطاق الآمن";
    } else {
      statusMsg = " تنبيه: نظام الأمان موقوف يدوياً من الموقع الإلكتروني";
    }
    Firebase.setString(fbData, "patient/fallStatus", statusMsg);

    lastFirebaseUpdate = millis();
  }
}