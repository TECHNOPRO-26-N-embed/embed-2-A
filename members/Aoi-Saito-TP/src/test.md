// ピン定義
const int PIN_BUTTON    = 2;
const int PIN_LED_RED   = 9;
const int PIN_LED_GREEN = 10;
const int PIN_LED_BLUE  = 11;
const int PIN_BUZZER    = 12;

// 状態
const int STATE_WAIT   = 0;
const int STATE_START  = 1;
const int STATE_RUN    = 2;
const int STATE_END    = 3;

int currentState = STATE_WAIT;

// タイマー
unsigned long stateStartTime = 0;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50;

// ボタン
bool lastButtonState = HIGH;  // INPUT_PULLUPなので未押下はHIGH
bool buttonPressed = false;

void setColor(bool r, bool g, bool b) {
  digitalWrite(PIN_LED_RED, r);
  digitalWrite(PIN_LED_GREEN, g);
  digitalWrite(PIN_LED_BLUE, b);
}

void setup() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  Serial.begin(9600);

  setColor(LOW, HIGH, LOW);  // 起動時は緑っぽく表示
  digitalWrite(PIN_BUZZER, LOW);
}

bool readButton() {
  bool reading = digitalRead(PIN_BUTTON);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading == LOW && lastButtonState == HIGH) {
      lastButtonState = reading;
      return true;   // 押した瞬間だけtrue
    }
  }

  lastButtonState = reading;
  return false;
}

void startFocusMode() {
  currentState = STATE_START;
  stateStartTime = millis();
  tone(PIN_BUZZER, 1000, 100);
}

void runFocusMode() {
  unsigned long now = millis();

  if (now - stateStartTime < 2000) {
    setColor(LOW, HIGH, LOW);   // 緑
  } else {
    currentState = STATE_RUN;
    setColor(LOW, LOW, HIGH);   // 青
  }
}

void endFocusMode() {
  currentState = STATE_END;
  setColor(LOW, LOW, LOW);
  tone(PIN_BUZZER, 1500, 200);
  delay(200);
  currentState = STATE_WAIT;
  setColor(HIGH, LOW, LOW);    // 待機時の色は必要に応じて変更
}

void loop() {
  buttonPressed = readButton();

  Serial.println(currentState);

  switch (currentState) {
    case STATE_WAIT:
      setColor(HIGH, HIGH, LOW);  // 黄
      if (buttonPressed) {
        startFocusMode();
      }
      break;

    case STATE_START:
      runFocusMode();
      if (buttonPressed) {
        endFocusMode();
      }
      break;

    case STATE_RUN:
      setColor(LOW, LOW, HIGH);   // 青
      if (buttonPressed) {
        endFocusMode();
      }
      break;

    case STATE_END:
      currentState = STATE_WAIT;
      break;
  }
}