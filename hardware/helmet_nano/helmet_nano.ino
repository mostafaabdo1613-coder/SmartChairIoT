#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

Adafruit_MPU6050 mpu;


const int threshold = 4;       
char currentCommand = 'S';


const float FALL_IMPACT_G = 18.0;  


const float   STILL_EPSILON   = 0.35;    
const unsigned long STILL_TIMEOUT_MS = 60000; 

float lastMagnitude = 9.8;
unsigned long lastMovementTime = 0;
bool faintAlertSent = false;

void setup(void) {
  Serial.begin(115200); 

  if (!mpu.begin()) {
    while (1) { delay(10); }
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  
  
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  lastMovementTime = millis();
}

void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float x = a.acceleration.x;
  float y = a.acceleration.y;
  float z = a.acceleration.z;
  float magnitude = sqrt(x * x + y * y + z * z);

  
  bool fallImpact = (magnitude > FALL_IMPACT_G);

  
  float delta = abs(magnitude - lastMagnitude);
  bool gyroMoving = (abs(g.gyro.x) > 0.05 || abs(g.gyro.y) > 0.05 || abs(g.gyro.z) > 0.05);
  if (delta > STILL_EPSILON || gyroMoving) {
    lastMovementTime = millis();
    faintAlertSent = false;
  }
  lastMagnitude = magnitude;

  bool prolongedStillness = (millis() - lastMovementTime > STILL_TIMEOUT_MS);

 
  if (fallImpact) {
    currentCommand = 'X'; 
  }
  else if (prolongedStillness) {
    currentCommand = 'W'; 
    faintAlertSent = true;
  }
  else {
  
    if (y < -threshold)      { currentCommand = 'F'; }
    else if (y > threshold)  { currentCommand = 'B'; }
    else if (x > threshold)  { currentCommand = 'L'; }
    else if (x < -threshold) { currentCommand = 'R'; }
    else                     { currentCommand = 'S'; }

   
    if (abs(g.gyro.x) > 2.0 || abs(g.gyro.y) > 2.0) {
      currentCommand = 'S';
    }
  }

  
  static unsigned long lastSendTime = 0;
  if (millis() - lastSendTime > 80) {
    Serial.write(currentCommand);
    lastSendTime = millis();
  }
}