// ピン定義
const uint8_t PIN_KEYPAD_ROW[4] = {2, 3, 4, 5};
const uint8_t PIN_KEYPAD_COL[4] = {6, 7, 8, 9};
const uint8_t PIN_LED_BLUE = A0;
const uint8_t PIN_LED_RED = A1;
const uint8_t PIN_BUZZER = A2;

// キーパッドマッピング
const char keypadMap[4][4] = {
  {'1','2','3','A'}, // A:スタート
  {'4','5','6','B'}, // B:停止
  {'7','8','9','C'}, // C:リセット
  {'*','0','#','D'}  // D: 分/秒入力切替
};

// 状態管理
uint8_t currentState = 0; // 0:待機中 1:計測中 2:終了通知 3:一時停止

// タイマー・入力値
unsigned long lastKeyScanMillis = 0;
unsigned long lastTimerMillis = 0;
unsigned long notifyStartMillis = 0;
unsigned long lastDebounceTime = 0;
char keyInput = '\0';
uint16_t setSeconds = 0;
uint16_t remainSeconds = 0;
bool isNotifying = false;
bool isMinuteInputMode = false; // false: 秒入力, true: 分入力
const int DEBOUNCE_DELAY = 50;
const unsigned long KEY_SCAN_INTERVAL = 15;
const unsigned long NOTIFY_BLINK_INTERVAL = 500;
// きらきら星 1番（A+B+A+B+A）C C G G A A G | F F E E D D C | G G F F E E D | G G F F E E D | C C G G A A G | F F E E D D C
// 1音ごとに50ms休符を挿入し、ループ再生
const uint16_t NOTIFY_MELODY_FREQ[] = {
  523,0, 523,0, 784,0, 784,0, 880,0, 880,0, 784,0, // A: C C G G A A G
  698,0, 698,0, 659,0, 659,0, 587,0, 587,0, 523,0, // B: F F E E D D C
  784,0, 784,0, 698,0, 698,0, 659,0, 659,0, 587,0, // A: G G F F E E D
  784,0, 784,0, 698,0, 698,0, 659,0, 659,0, 587,0, // B: G G F F E E D
  523,0, 523,0, 784,0, 784,0, 880,0, 880,0, 784,0, // A: C C G G A A G
  698,0, 698,0, 659,0, 659,0, 587,0, 587,0, 523,0  // B: F F E E D D C
};
const uint16_t NOTIFY_MELODY_DURATION[] = {
  200,50, 200,50, 200,50, 200,50, 200,50, 200,50, 400,50,
  200,50, 200,50, 200,50, 200,50, 200,50, 200,50, 400,50,
  200,50, 200,50, 200,50, 200,50, 200,50, 200,50, 400,50,
  200,50, 200,50, 200,50, 200,50, 200,50, 200,50, 400,50,
  200,50, 200,50, 200,50, 200,50, 200,50, 200,50, 400,50,
  200,50, 200,50, 200,50, 200,50, 200,50, 200,50, 400,50
};
const uint8_t NOTIFY_MELODY_LENGTH = sizeof(NOTIFY_MELODY_FREQ) / sizeof(NOTIFY_MELODY_FREQ[0]);

uint8_t notifyMelodyIndex = 0;
unsigned long notifyMelodyNoteStartMillis = 0;
bool notifyMelodyPlaying = false;

uint8_t lastLoggedState = 255;

// キーパッドスキャン
char readKeypad() {
  static bool waitRelease = false;
  unsigned long now = millis();
  if (now - lastKeyScanMillis < KEY_SCAN_INTERVAL) return '\0';
  lastKeyScanMillis = now;
  if (now - lastDebounceTime < DEBOUNCE_DELAY) return '\0';
  char detectedKey = '\0';

  for (int col = 0; col < 4; col++) {
    pinMode(PIN_KEYPAD_COL[col], OUTPUT);
    digitalWrite(PIN_KEYPAD_COL[col], LOW);
    for (int row = 0; row < 4; row++) {
      pinMode(PIN_KEYPAD_ROW[row], INPUT_PULLUP);
      if (digitalRead(PIN_KEYPAD_ROW[row]) == LOW) {
        detectedKey = keypadMap[row][col];
        break;
      }
    }
    pinMode(PIN_KEYPAD_COL[col], INPUT);
    if (detectedKey != '\0') break;
  }

  if (detectedKey == '\0') {
    waitRelease = false;
    return '\0';
  }

  if (waitRelease) {
    return '\0';
  }

  waitRelease = true;
  lastDebounceTime = now;
  Serial.print("readKeypad: detected key ");
  Serial.println(detectedKey);
  return detectedKey;
}

// 時間設定
bool setTimeFromKeypad(char key) {
  static uint8_t inputMinutes = 0;
  static uint8_t inputSeconds = 0;
  if (key == 'D') { // 入力単位切り替え
    isMinuteInputMode = !isMinuteInputMode;
    if (!isMinuteInputMode) {
      // 秒モードに入るときは秒入力を新規開始できるようにする
      inputSeconds = 0;
    }
    Serial.print("入力モードを");
    Serial.print(isMinuteInputMode ? "分" : "秒");
    Serial.println("に切り替えました");
    return false;
  } else if (key >= '0' && key <= '9') {
    if (isMinuteInputMode) {
      uint16_t nextMinutes = static_cast<uint16_t>(inputMinutes * 10 + (key - '0'));
      if (nextMinutes <= 99) {
        inputMinutes = static_cast<uint8_t>(nextMinutes);
      }
    } else {
      uint16_t nextSeconds = static_cast<uint16_t>(inputSeconds * 10 + (key - '0'));
      if (nextSeconds <= 59) {
        inputSeconds = static_cast<uint8_t>(nextSeconds);
      }
    }
    Serial.print("setTimeFromKeypad: ");
    Serial.print(inputMinutes);
    Serial.print("分");
    Serial.print(inputSeconds);
    Serial.println("秒");
    return false;
  } else if (key == 'A') { // スタート
    uint16_t candidateSeconds = static_cast<uint16_t>(inputMinutes) * 60 + inputSeconds;
    if (candidateSeconds >= 1 && candidateSeconds <= 5999) {
      setSeconds = candidateSeconds;
      inputMinutes = 0;
      inputSeconds = 0;
      Serial.println("スタート");
      return true;
    } else {
      inputMinutes = 0;
      inputSeconds = 0;
      Serial.println("setTimeFromKeypad: start invalid");
      return false;
    }
  } else if (key == 'C') { // リセット
    inputMinutes = 0;
    inputSeconds = 0;
    Serial.println("setTimeFromKeypad: reset buffer");
    return false;
  }
  return false;
}

void logStateIfChanged() {
  if (currentState == lastLoggedState) return;
  lastLoggedState = currentState;
  Serial.print("状態変更: ");
  if (currentState == 0) {
    Serial.println("待機中");
  } else if (currentState == 1) {
    Serial.println("計測中");
  } else if (currentState == 2) {
    Serial.println("終了通知");
  } else if (currentState == 3) {
    Serial.println("一時停止");
  } else {
    Serial.println("不明");
  }
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
  Serial.println(ss);
}

// LED制御
void setRedLed(bool on) {
  digitalWrite(PIN_LED_RED, on ? HIGH : LOW);
}

void updateLedByState(uint8_t state) {
  if (state == 0) { // 待機中
    digitalWrite(PIN_LED_BLUE, LOW);
    setRedLed(false);
  } else if (state == 1) { // 計測中
    digitalWrite(PIN_LED_BLUE, HIGH);
    setRedLed(false);
  } else if (state == 2) { // 終了通知
    digitalWrite(PIN_LED_BLUE, LOW);
    // 赤LED点滅はupdateNotifyで制御
  } else if (state == 3) { // 一時停止
    digitalWrite(PIN_LED_BLUE, LOW);
    setRedLed(false);
  } else {
    digitalWrite(PIN_LED_BLUE, LOW);
    setRedLed(false);
  }
}

// ブザー制御
void notifyByBuzzer(bool on) {
  if (on) {
    tone(PIN_BUZZER, 2000);
  } else {
    noTone(PIN_BUZZER);
  }
}

void resetNotifyMelody() {
  notifyMelodyIndex = 0;
  notifyMelodyNoteStartMillis = 0;
  notifyMelodyPlaying = false;
}

void updateNotifyMelody(unsigned long now) {
  if (!notifyMelodyPlaying) {
    notifyMelodyPlaying = true;
    notifyMelodyNoteStartMillis = now;
    uint16_t freq = NOTIFY_MELODY_FREQ[notifyMelodyIndex];
    if (freq == 0) {
      noTone(PIN_BUZZER);
    } else {
      tone(PIN_BUZZER, freq);
    }
    return;
  }

  if (now - notifyMelodyNoteStartMillis < NOTIFY_MELODY_DURATION[notifyMelodyIndex]) {
    return;
  }

  notifyMelodyIndex = (notifyMelodyIndex + 1) % NOTIFY_MELODY_LENGTH;
  notifyMelodyNoteStartMillis = now;
  uint16_t freq = NOTIFY_MELODY_FREQ[notifyMelodyIndex];
  if (freq == 0) {
    noTone(PIN_BUZZER);
  } else {
    tone(PIN_BUZZER, freq);
  }
}

// 終了通知中の制御
void updateNotify(char key) {
  // 赤LED点滅（500ms周期）
  unsigned long now = millis();
  bool ledOn = ((now / NOTIFY_BLINK_INTERVAL) % 2) == 0;

  setRedLed(ledOn);
  // 終了通知中は簡易メロディーをループ再生
  updateNotifyMelody(now);

  // 停止/リセットキーでのみ待機中へ
  if (key == 'B' || key == 'C') {
    setRedLed(false);
    notifyByBuzzer(false);
    resetNotifyMelody();
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
  logStateIfChanged();
  switch (currentState) {
    case 0: // 待機中
      {
      char key = readKeypad();
      updateLedByState(0);
      if (key != '\0') {
        if (setTimeFromKeypad(key)) {
          remainSeconds = setSeconds;
          lastTimerMillis = now;
          currentState = 1;
          logStateIfChanged();
          Serial.println("状態遷移: 待機中 -> 計測中");
          showTimeSerial(remainSeconds);
        }
      }
      break;
      }
    case 1: // 計測中
      {
      char key = readKeypad();
      updateLedByState(1);
      if (now - lastTimerMillis >= 1000 && remainSeconds > 0) {
        lastTimerMillis = now;
        remainSeconds--;
        showTimeSerial(remainSeconds);
        if (remainSeconds == 0) {
          currentState = 2;
          notifyStartMillis = now;
          isNotifying = true;
          resetNotifyMelody();
          setRedLed(true);
          Serial.println("状態遷移: 計測中 -> 終了通知");
        }
      }
      if (key == 'B') { // 一時停止
        Serial.print("計測中を停止しました\n");
        currentState = 3;
      }
      if (key == 'C') { // リセット
        Serial.print("計測中をリセットしました\n");
        remainSeconds = 0;
        currentState = 0;
      }
      break;
      }
    case 2: // 終了通知
      updateLedByState(2);
      updateNotify(readKeypad());
      break;
    case 3: // 一時停止
      {
      char key = readKeypad();
      updateLedByState(3);
      if (key == 'A' && remainSeconds > 0) {
        lastTimerMillis = now;
        currentState = 1;
        logStateIfChanged();
        Serial.println("状態遷移: 一時停止 -> 計測中");
        showTimeSerial(remainSeconds);
      }
      if (key == 'C') {
        remainSeconds = 0;
        currentState = 0;
        Serial.println("状態遷移: 一時停止 -> 待機中 (リセット)");
      }
      break;
      }
    default:
      updateLedByState(0);
      notifyByBuzzer(false);
      currentState = 0;
      break;
  }
}