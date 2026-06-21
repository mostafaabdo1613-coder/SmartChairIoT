#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

Adafruit_MPU6050 mpu;

// عتبة ميل الرأس (تم تقليلها لـ 4 لتبدأ الحركة بميل خفيف ومريح ومباشر)
const int threshold = 4;       
char currentCommand = 'S';

// ── إعدادات اكتشاف السقوط (X) ────────────────────────────
const float FALL_IMPACT_G = 18.0;   // عتبة تسارع مفاجئ = صدمة سقوط

// ── إعدادات اكتشاف الإغماء/الثبات الطويل (W) ─────────────
const float   STILL_EPSILON   = 0.35;     // أقل تغيّر يعتبر "حركة حقيقية"
const unsigned long STILL_TIMEOUT_MS = 60000; // 60 ثانية ثبات تام = إنذار

float lastMagnitude = 9.8;
unsigned long lastMovementTime = 0;
bool faintAlertSent = false;

void setup(void) {
  Serial.begin(115200); 

  if (!mpu.begin()) {
    while (1) { delay(10); }
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  
  // 🔥 تم تعديل الفلتر إلى 21 هرتز لإزالة الـ Latency الثقيل وجعل الاستجابة فورية ولحظية جداً
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

  // ── 1) اكتشاف صدمة سقوط مفاجئة ──────────────────────────
  bool fallImpact = (magnitude > FALL_IMPACT_G);

  // ── 2) تتبع آخر حركة حقيقية لاكتشاف الثبات الطويل ──
  float delta = abs(magnitude - lastMagnitude);
  bool gyroMoving = (abs(g.gyro.x) > 0.05 || abs(g.gyro.y) > 0.05 || abs(g.gyro.z) > 0.05);
  if (delta > STILL_EPSILON || gyroMoving) {
    lastMovementTime = millis();
    faintAlertSent = false;
  }
  lastMagnitude = magnitude;

  bool prolongedStillness = (millis() - lastMovementTime > STILL_TIMEOUT_MS);

  // ── 3) تحديد الأمر الحالي بناء على الزوايا بدقة وسلاسة ──
  if (fallImpact) {
    currentCommand = 'X'; 
  }
  else if (prolongedStillness) {
    currentCommand = 'W'; 
    faintAlertSent = true;
  }
  else {
    // تحديد الاتجاه بميل سلس جداً وبدون الحاجة لإنزال الرأس بشكل قاسي
    if (y < -threshold)      { currentCommand = 'F'; }
    else if (y > threshold)  { currentCommand = 'B'; }
    else if (x > threshold)  { currentCommand = 'L'; }
    else if (x < -threshold) { currentCommand = 'R'; }
    else                     { currentCommand = 'S'; }

    // حماية: حركة رأس سريعة ومفاجئة جداً (عطس مثلاً) = توقف مؤقت للأمان
    if (abs(g.gyro.x) > 2.0 || abs(g.gyro.y) > 2.0) {
      currentCommand = 'S';
    }
  }

  // ── 4) إرسال الأمر بانتظام وثبات كل 80 ملي ثانية لضمان السلاسة ──
  static unsigned long lastSendTime = 0;
  if (millis() - lastSendTime > 80) {
    Serial.write(currentCommand);
    lastSendTime = millis();
  }
}