// ピン定義
const uint8_t PIN_KEYPAD_ROW[4] = {2, 3, 7, 8};
const uint8_t PIN_KEYPAD_COL[4] = {A0, A1, A2, A3};
const uint8_t PIN_LED_BLUE = 9;
const uint8_t PIN_LED_RED = 10;
const uint8_t PIN_BUZZER = 11;

// キーパッドマッピング
const char keypadMap[4][4] = {
  {'1','2','3','A'}, // A:スタート
  {'4','5','6','B'}, // B:停止
  {'7','8','9','C'}, // C:リセット
  {'*','0','#','D'}
};

// 状態管理
uint8_t currentState = 0; // 0:待機中 1:計測中 2:終了通知

// タイマー・入力値
unsigned long lastKeyScanMillis = 0;
unsigned long lastTimerMillis = 0;
unsigned long notifyStartMillis = 0;
unsigned long lastDebounceTime = 0;
char keyInput = '\0';
uint16_t setSeconds = 0;
uint16_t remainSeconds = 0;
bool isNotifying = false;
const int DEBOUNCE_DELAY = 50;
const unsigned long KEY_SCAN_INTERVAL = 15;

// キーパッドスキャン
char readKeypad() {
  unsigned long now = millis();
  if (now - lastKeyScanMillis < KEY_SCAN_INTERVAL) return '\0';
  lastKeyScanMillis = now;
  if (now - lastDebounceTime < DEBOUNCE_DELAY) return '\0';
  for (int col = 0; col < 4; col++) {
    pinMode(PIN_KEYPAD_COL[col], OUTPUT);
    digitalWrite(PIN_KEYPAD_COL[col], LOW);
    for (int row = 0; row < 4; row++) {
      pinMode(PIN_KEYPAD_ROW[row], INPUT_PULLUP);
      if (digitalRead(PIN_KEYPAD_ROW[row]) == LOW) {
        Serial.print("readKeypad: detected key ");
        Serial.println(keypadMap[row][col]);
        lastDebounceTime = now;
        pinMode(PIN_KEYPAD_COL[col], INPUT);
        return keypadMap[row][col];
      }
    }
    pinMode(PIN_KEYPAD_COL[col], INPUT);
  }
  Serial.println("readKeypad: no key");
  return '\0';
}

// 時間設定
bool setTimeFromKeypad(char key) {
  static uint16_t buffer = 0;
  if (key >= '0' && key <= '9') {
    uint16_t next = static_cast<uint16_t>(buffer * 10 + (key - '0'));
    if (next <= 5999) {
      buffer = next;
      Serial.print("setTimeFromKeypad: buffer=");
      Serial.println(buffer);
    }
  } else if (key == 'A') { // スタート
    if (buffer >= 1 && buffer <= 5999) {
      setSeconds = buffer;
      buffer = 0;
      Serial.print("setTimeFromKeypad: start valid, setSeconds=");
      Serial.println(setSeconds);
      return true;
    } else {
      buffer = 0;
      Serial.println("setTimeFromKeypad: start invalid");
      return false;
    }
  } else if (key == 'C') { // リセット
    buffer = 0;
    Serial.println("setTimeFromKeypad: reset buffer");
  }
  Serial.println("setTimeFromKeypad: no action");
  return false;
}

// シリアル表示
void showTimeSerial(uint16_t sec) {
  uint8_t mm = sec / 60;
  uint8_t ss = sec % 60;
  Serial.print("Time: ");
  if (mm < 10) Serial.print('0');
  Serial.print(mm);
  Serial.print(":");
  if (ss < 10) Serial.print('0');
  Serial.print(ss);
  Serial.print(" State: ");
  Serial.println(currentState);
}

// LED制御
void updateLedByState(uint8_t state) {
  if (state == 0) { // 待機中
    Serial.println("updateLedByState: WAITING");
    digitalWrite(PIN_LED_BLUE, LOW);
    digitalWrite(PIN_LED_RED, LOW);
  } else if (state == 1) { // 計測中
    Serial.println("updateLedByState: RUNNING");
    digitalWrite(PIN_LED_BLUE, HIGH);
    digitalWrite(PIN_LED_RED, LOW);
  } else if (state == 2) { // 終了通知
    Serial.println("updateLedByState: NOTIFY");
    digitalWrite(PIN_LED_BLUE, LOW);
    // 赤LED点滅はupdateNotifyで制御
  } else {
    Serial.println("updateLedByState: UNKNOWN");
    digitalWrite(PIN_LED_BLUE, LOW);
    digitalWrite(PIN_LED_RED, LOW);
  }
}

// ブザー制御
void notifyByBuzzer(bool on) {
  Serial.print("notifyByBuzzer: ");
  Serial.println(on ? "ON" : "OFF");
  digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
}

// 終了通知中の制御
void updateNotify(char key) {
  // 赤LED点滅（1秒に5回=100ms周期）
  unsigned long now = millis();
  if (((now / 100) % 2) == 0) {
    Serial.println("updateNotify: RED ON");
    digitalWrite(PIN_LED_RED, HIGH);
    notifyByBuzzer(true);
  } else {
    Serial.println("updateNotify: RED OFF");
    digitalWrite(PIN_LED_RED, LOW);
    notifyByBuzzer(true);
  }
  // 停止/リセットキーでのみ待機中へ
  if (key == 'B' || key == 'C') {
    Serial.print("updateNotify: exit notify by key ");
    Serial.println(key);
    digitalWrite(PIN_LED_RED, LOW);
    notifyByBuzzer(false);
    currentState = 0;
    isNotifying = false;
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("setup: start");
  for (int i = 0; i < 4; i++) {
    pinMode(PIN_KEYPAD_ROW[i], INPUT_PULLUP);
    pinMode(PIN_KEYPAD_COL[i], INPUT);
  }
  pinMode(PIN_LED_BLUE, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_LED_BLUE, HIGH);
  delay(1000);
  digitalWrite(PIN_LED_BLUE, LOW);
  Serial.println("setup: end");
}

void loop() {
  unsigned long now = millis();
  switch (currentState) {
    case 0: // 待機中
      {
      char key = readKeypad();
      Serial.println("loop: state WAITING");
      updateLedByState(0);
      if (key != '\0') {
        Serial.print("loop: WAITING key input ");
        Serial.println(key);
        if (setTimeFromKeypad(key)) {
          remainSeconds = setSeconds;
          lastTimerMillis = now;
          currentState = 1;
          Serial.println("loop: transition WAITING->RUNNING");
        }
      }
      break;
      }
    case 1: // 計測中
      {
      char key = readKeypad();
      Serial.println("loop: state RUNNING");
      updateLedByState(1);
      if (now - lastTimerMillis >= 1000 && remainSeconds > 0) {
        lastTimerMillis = now;
        remainSeconds--;
        showTimeSerial(remainSeconds);
        if (remainSeconds == 0) {
          currentState = 2;
          notifyStartMillis = now;
          isNotifying = true;
          Serial.println("loop: transition RUNNING->NOTIFY");
        }
      }
      if (key == 'B' || key == 'C') { // 停止/リセット
        Serial.print("loop: RUNNING exit by key ");
        Serial.println(key);
        remainSeconds = 0;
        currentState = 0;
      }
      break;
      }
    case 2: // 終了通知
      Serial.println("loop: state NOTIFY");
      updateNotify(readKeypad());
      break;
    default:
      Serial.println("loop: state DEFAULT");
      updateLedByState(0);
      notifyByBuzzer(false);
      currentState = 0;
      break;
  }
}