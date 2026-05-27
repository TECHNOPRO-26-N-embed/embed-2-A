// ===== ピン定義 =====
const int PIN_TRIG   = 12;
const int PIN_ECHO   = 11;
const int PIN_BUZZER = 8;

// ===== 設定値 =====
const int THRESHOLD_CM = 30;
const int MIN_FREQ     = 200;
const int MAX_FREQ     = 2000;

// ===== タイマー =====
const unsigned long SENSOR_INTERVAL = 100;
unsigned long lastMillis_Sensor = 0;

// ===== 状態 =====
int currentState = 0;   // 0:待機, 1:警告

// ===== センサー値 =====
long distance = 0;
int freq = 0;
bool sensorOK = false;

void setup() {
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  Serial.begin(9600);

  digitalWrite(PIN_TRIG, LOW);
  noTone(PIN_BUZZER);
}

void loop() {
  unsigned long now = millis();

  if (now - lastMillis_Sensor >= SENSOR_INTERVAL) {
    lastMillis_Sensor = now;

    distance = readSensor();
    showDistance(distance);

    if (distance > 0 && distance <= THRESHOLD_CM) {
      currentState = 1;
    } else {
      currentState = 0;
      stopBuzzer();
    }
  }

  switch (currentState) {
    case 0:
      // 待機中
      stopBuzzer();
      break;

    case 1:
      // 警告中
      playToneByDistance(distance);
      break;

    default:
      stopBuzzer();
      currentState = 0;
      break;
  }
}

long readSensor() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  unsigned long duration = pulseIn(PIN_ECHO, HIGH, 30000);

  if (duration == 0) {
    sensorOK = false;
    return 0;
  }

  long cm = duration / 58;
  sensorOK = true;
  return cm;
}

void showDistance(long cm) {
  Serial.print("Distance = ");
  Serial.print(cm);
  Serial.println(" cm");
}

int calcFrequency(long cm) {
  if (cm <= 0 || cm > THRESHOLD_CM) {
    return 0;
  }

  // 近いほど高音になるように変換
  int f = map(cm, THRESHOLD_CM, 1, MIN_FREQ, MAX_FREQ);
  f = constrain(f, MIN_FREQ, MAX_FREQ);
  return f;
}

void playToneByDistance(long cm) {
  if (!sensorOK) {
    stopBuzzer();
    return;
  }

  int f = calcFrequency(cm);
  if (f == 0) {
    stopBuzzer();
    return;
  }

  tone(PIN_BUZZER, f);
  freq = f;
}

void stopBuzzer() {
  noTone(PIN_BUZZER);
  freq = 0;
}