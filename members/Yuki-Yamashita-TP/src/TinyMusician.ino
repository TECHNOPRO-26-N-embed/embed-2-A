// TinyMusician - Arduino UNO R3

// Pin definitions
const int PIN_STICK_X = A0;
const int PIN_STICK_Y = A1;
const int PIN_STICK_SW = 2;
const int PIN_BUZZER = 8;
const int PIN_LED_WHITE = 9;

// State definitions
const int STATE_IDLE = 0;
const int STATE_QUESTION = 1;
const int STATE_INPUT = 2;
const int STATE_CONFIRM = 3;
const int STATE_JUDGE = 4;
const int STATE_RESULT = 5;

// State management
int currentState = STATE_IDLE;
int nextState = STATE_IDLE;
bool targetTonePlayed = false;

// Tone and answer settings
int targetNote = 0;
int playerNote = 262;
int playerDurationMs = 100;
const int NOTE_MIN_HZ = 262;
const int NOTE_MAX_HZ = 494;
const int NOTE_TOLERANCE_HZ = 20;
const int DURATION_MIN_MS = 100;
const int DURATION_MAX_MS = 300;

// Difficulty and time limit
int difficulty = 1; // 0:easy, 1:normal, 2:hard
const int TIME_LIMIT_EASY = 0;
const int TIME_LIMIT_NORMAL = 7000;
const int TIME_LIMIT_HARD = 4000;
int timeLimitMs = TIME_LIMIT_NORMAL;
unsigned long inputStartTime = 0;
const int WARNING_START_MS = 3000;
const int WARNING_FINAL_MS = 1000;

// Input values
int stickValueX = 512;
int stickValueY = 512;
int prevX = 512;
int prevY = 512;
const int deadband = 8;

// Switch control (INPUT_PULLUP)
bool swRaw = true;
bool swPrevRaw = true;
bool swConfirmed = true;
bool swPrevConfirmed = true;
unsigned long lastDebounceTime = 0;
unsigned long pressStartTime = 0;
const int DEBOUNCE_DELAY = 50;
const int REINPUT_DELAY = 200;
const int LONG_PRESS_MS = 1000;
bool shortPressEvent = false;
bool longPressEvent = false;

// Timers
unsigned long nowMs = 0;
unsigned long lastReadTime = 0;
unsigned long lastPreviewToneTime = 0;
unsigned long lastBlinkTime = 0;
unsigned long lastInputTime = 0;
const int READ_INTERVAL_MS = 50;
const int BLINK_INTERVAL_MS = 500;

// Output and result
bool ledState = false;
bool isCorrect = false;

// Forward declarations
void playTargetTone();
void updatePlayerTone();
void selectDifficulty();
void blinkLED();
void checkAnswer(bool finalizeResult);
void readJoystick();
void readSwitchWithDebounce();

void setup() {
  pinMode(PIN_STICK_SW, INPUT_PULLUP);
  pinMode(PIN_LED_WHITE, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  currentState = STATE_IDLE;
  nextState = STATE_IDLE;
  digitalWrite(PIN_LED_WHITE, LOW);
  noTone(PIN_BUZZER);

  randomSeed(analogRead(A5));
  Serial.begin(9600);
}

void loop() {
  nowMs = millis();
  readJoystick();
  readSwitchWithDebounce();

  switch (currentState) {
    case STATE_IDLE:
      digitalWrite(PIN_LED_WHITE, LOW);
      noTone(PIN_BUZZER);
      targetTonePlayed = false;
      selectDifficulty();

      if (shortPressEvent) {
        shortPressEvent = false;
        currentState = STATE_QUESTION;
      }
      break;

    case STATE_QUESTION:
      if (!targetTonePlayed) {
        playTargetTone();
      }
      if (targetTonePlayed) {
        inputStartTime = nowMs;
        currentState = STATE_INPUT;
      }
      break;

    case STATE_INPUT: {
      updatePlayerTone();

      if (timeLimitMs > 0) {
        unsigned long elapsed = nowMs - inputStartTime;
        int remaining = timeLimitMs - (int)elapsed;
        if (remaining < 0) {
          remaining = 0;
        }

        if (remaining <= WARNING_FINAL_MS) {
          if (nowMs - lastBlinkTime >= 100) {
            ledState = !ledState;
            digitalWrite(PIN_LED_WHITE, ledState ? HIGH : LOW);
            lastBlinkTime = nowMs;
          }
        } else if (remaining <= WARNING_START_MS) {
          if (nowMs - lastBlinkTime >= 250) {
            ledState = !ledState;
            digitalWrite(PIN_LED_WHITE, ledState ? HIGH : LOW);
            lastBlinkTime = nowMs;
          }
        } else {
          digitalWrite(PIN_LED_WHITE, LOW);
          ledState = false;
        }

        if (elapsed >= (unsigned long)timeLimitMs) {
          currentState = STATE_JUDGE;
          break;
        }
      } else {
        digitalWrite(PIN_LED_WHITE, LOW);
        ledState = false;
      }

      if (shortPressEvent) {
        shortPressEvent = false;
        currentState = STATE_CONFIRM;
      } else if (longPressEvent) {
        longPressEvent = false;
        currentState = STATE_JUDGE;
      }
      break;
    }

    case STATE_CONFIRM:
      checkAnswer(false);
      currentState = STATE_INPUT;
      break;

    case STATE_JUDGE:
      digitalWrite(PIN_LED_WHITE, LOW);
      ledState = false;
      checkAnswer(true);
      if (currentState == STATE_IDLE) {
        break;
      }
      currentState = STATE_RESULT;
      break;

    case STATE_RESULT:
      if (isCorrect) {
        digitalWrite(PIN_LED_WHITE, HIGH);
        tone(PIN_BUZZER, 880, 120);
      } else {
        blinkLED();
        tone(PIN_BUZZER, 220, 120);
      }

      if (shortPressEvent) {
        shortPressEvent = false;
        longPressEvent = false;
        noTone(PIN_BUZZER);
        digitalWrite(PIN_LED_WHITE, LOW);
        ledState = false;
        currentState = STATE_IDLE;
      }
      break;

    default:
      currentState = STATE_IDLE;
      break;
  }
}

void playTargetTone() {
  targetNote = random(NOTE_MIN_HZ, NOTE_MAX_HZ + 1);
  if (targetNote < NOTE_MIN_HZ || targetNote > NOTE_MAX_HZ) {
    targetNote = NOTE_MIN_HZ;
  }

  tone(PIN_BUZZER, targetNote, 200);
  delay(220);
  noTone(PIN_BUZZER);
  targetTonePlayed = true;
}

void updatePlayerTone() {
  int mappedNote = map(stickValueY, 0, 1023, NOTE_MIN_HZ, NOTE_MAX_HZ);
  int mappedDuration = map(stickValueX, 0, 1023, DURATION_MIN_MS, DURATION_MAX_MS);

  mappedNote = constrain(mappedNote, NOTE_MIN_HZ, NOTE_MAX_HZ);
  mappedDuration = constrain(mappedDuration, DURATION_MIN_MS, DURATION_MAX_MS);

  playerNote = mappedNote;
  playerDurationMs = mappedDuration;


if (nowMs - lastPreviewToneTime >= READ_INTERVAL_MS &&
    abs(playerNote - mappedNote) > 2) {

  int previewMs = constrain(playerDurationMs, 30, 50);
  tone(PIN_BUZZER, mappedNote, previewMs);
  lastPreviewToneTime = nowMs;
}

}

void selectDifficulty() {
  static int prevZone = 1;
  static bool wasCentered = true;

  int rawX = analogRead(PIN_STICK_X);
  int threshold = 120;

  int zone;

  if (rawX < 512 - threshold) {
    zone = 0;
  } else if (rawX > 512 + threshold) {
    zone = 2;
  } else {
    zone = 1;
  }

  // 中央に戻ったらフラグON
  if (zone == 1) {
    wasCentered = true;
  }

  // 中央から外に出た瞬間だけ反応
  if (wasCentered && zone != 1) {
    difficulty = zone;

    if (difficulty == 0) timeLimitMs = TIME_LIMIT_EASY;
    else if (difficulty == 2) timeLimitMs = TIME_LIMIT_HARD;
    else timeLimitMs = TIME_LIMIT_NORMAL;

    int notifyTone = 440;
    if (difficulty == 0) notifyTone = 330;
    if (difficulty == 2) notifyTone = 660;

    tone(PIN_BUZZER, notifyTone, 70);
    Serial.print("zone changed → ");
    Serial.println(zone);
    prevZone = zone;


    wasCentered = false; // 連続入力防止
  }
}

void blinkLED() {
  if (nowMs - lastBlinkTime >= BLINK_INTERVAL_MS) {
    ledState = !ledState;
    digitalWrite(PIN_LED_WHITE, ledState ? HIGH : LOW);
    lastBlinkTime = nowMs;
  }
}

void checkAnswer(bool finalizeResult) {
  if (targetNote == 0) {
    isCorrect = false;
    currentState = STATE_IDLE;
    return;
  }

  int diff = abs(targetNote - playerNote);

  tone(PIN_BUZZER, targetNote, 120);
  delay(140);
  tone(PIN_BUZZER, playerNote, 120);
  delay(140);
  noTone(PIN_BUZZER);

  if (finalizeResult) {
    isCorrect = (diff <= NOTE_TOLERANCE_HZ);
  }
}

void readJoystick() {
  if (nowMs - lastReadTime >= READ_INTERVAL_MS) {
    int rawX = analogRead(PIN_STICK_X);
    int rawY = analogRead(PIN_STICK_Y);

    // Keep previous valid values if readings are likely broken.
    bool xLooksBroken = (rawX == 0 || rawX == 1023);
    bool yLooksBroken = (rawY == 0 || rawY == 1023);

    if (!xLooksBroken && abs(rawX - prevX) >= deadband) {
      stickValueX = rawX;
      prevX = rawX;
    }
    if (!yLooksBroken && abs(rawY - prevY) >= deadband) {
      stickValueY = rawY;
      prevY = rawY;
    }

    lastReadTime = nowMs;
  }
}

void readSwitchWithDebounce() {
  swRaw = (digitalRead(PIN_STICK_SW) == HIGH);

  if (swRaw != swPrevRaw) {
    lastDebounceTime = nowMs;
  }

  if (nowMs - lastDebounceTime >= (unsigned long)DEBOUNCE_DELAY) {
    swConfirmed = swRaw;
  }

  // Edge detect on confirmed signal (INPUT_PULLUP: true=release, false=pressed)
  if (swConfirmed != swPrevConfirmed) {
    if (!swConfirmed) {
      pressStartTime = nowMs;
    } else {
      unsigned long pressDuration = nowMs - pressStartTime;
      if (nowMs - lastInputTime >= (unsigned long)REINPUT_DELAY) {
        if (pressDuration >= (unsigned long)LONG_PRESS_MS) {
          longPressEvent = true;
          shortPressEvent = false;
        } else {
          shortPressEvent = true;
          longPressEvent = false;
        }
        lastInputTime = nowMs;
      }
    }
    swPrevConfirmed = swConfirmed;
  }

  swPrevRaw = swRaw;
}
