#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>

// Pin definitions (detailed design aligned)
const uint8_t PIN_TEMP_SENSOR = A0;
const uint8_t PIN_LED_BLUE_1 = 4;
const uint8_t PIN_LED_GREEN_1 = 5;
const uint8_t PIN_LED_GREEN_2 = 6;
const uint8_t PIN_LED_YELLOW_1 = 7;
const uint8_t PIN_LED_YELLOW_2 = 8;
const uint8_t PIN_LED_RED_1 = 9;
const uint8_t PIN_BUZZER = 13;

// LCD1602 I2C address (typical: 0x27 or 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// State definitions
const uint8_t STATE_WAITING = 0;
const uint8_t STATE_MEASURING = 1;
const uint8_t STATE_NOTIFY = 2;
const uint8_t STATE_ERROR = 3;

// Constants
const float TEMP_START_C = 35.0;
const float TEMP_STEP_2_C = 36.0;
const float TEMP_STEP_3_C = 37.0;
const float TEMP_STEP_4_C = 38.0;
const float TEMP_EMERGENCY_C = 39.0;
const float TEMP_VALID_MIN_C = 20.0;
const float TEMP_VALID_MAX_C = 45.0;
const float HYSTERESIS_C = 0.2;

// Thermistor model constants
const float THERMISTOR_BETA = 3950.0;
const float THERMISTOR_NOMINAL_RESISTANCE = 10000.0;
const float THERMISTOR_NOMINAL_TEMP_C = 25.0;
const float SERIES_RESISTOR = 10000.0;

const unsigned long SAMPLE_INTERVAL_MS = 100;
const unsigned long OUTPUT_INTERVAL_MS = 200;
const unsigned long DISPLAY_INTERVAL_MS = 200;
const unsigned long MEASUREMENT_TIMEOUT_MS = 30000;
const unsigned long EMERGENCY_BLINK_MS = 200;
const unsigned long LOW_TEMP_ERROR_MS = 5000;
const unsigned long ERROR_HOLD_MS = 2000;
const unsigned long NOTIFY_HOLD_MS = 3000;

// Globals
uint8_t currentState = STATE_WAITING;
float tempC = 0.0;
float finalTempC = 0.0;
float lastValidTempC = 36.5;
bool sensorErrorFlag = false;
bool emergencyBlinkFlag = false;

unsigned long measurementStartMs = 0;
unsigned long lastSampleMs = 0;
unsigned long lastOutputMs = 0;
unsigned long lastDisplayMs = 0;
unsigned long lowTempStartMs = 0;
unsigned long notifyStartMs = 0;
unsigned long errorStartMs = 0;

bool emergencyBlinkOn = true;
bool notifyTonePlayed = false;

const uint16_t PROGRESS_NOTES[8] = {262, 294, 330, 349, 392, 440, 494, 523};
const uint16_t DONE_TONE = 659;
const uint16_t ERROR_TONES[2] = {392, 294};
const uint16_t EMERGENCY_TONES[2] = {988, 784};

float sampleWindow[5] = {0, 0, 0, 0, 0};
uint8_t sampleIndex = 0;
bool sampleFilled = false;

// Forward declarations
float readTemperatureSensor();
uint8_t updateState(float measuredTempC);
bool validateMeasurement(float measuredTempC);
void updateBuzzerByTemperature(float measuredTempC, uint8_t state);
void updateLedByTemperature(float measuredTempC, uint8_t state);
void notifyBySoundAndLed(float measuredTempC, uint8_t state);
void showTemperatureOnLcd(float measuredTempC, uint8_t state);
void setAllLeds(bool on);
void applyLedPattern(uint8_t level);
bool isTemperatureStable();

void setup() {
  pinMode(PIN_TEMP_SENSOR, INPUT);
  pinMode(PIN_LED_BLUE_1, OUTPUT);
  pinMode(PIN_LED_GREEN_1, OUTPUT);
  pinMode(PIN_LED_GREEN_2, OUTPUT);
  pinMode(PIN_LED_YELLOW_1, OUTPUT);
  pinMode(PIN_LED_YELLOW_2, OUTPUT);
  pinMode(PIN_LED_RED_1, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  lcd.init();
  lcd.backlight();

  Serial.begin(9600);

  setAllLeds(false);
  noTone(PIN_BUZZER);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ready");
  lcd.setCursor(0, 1);
  lcd.print("----");

  tone(PIN_BUZZER, 523, 150);
  delay(150);
  noTone(PIN_BUZZER);

  currentState = STATE_WAITING;
}

void loop() {
  unsigned long now = millis();

  if (now - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    lastSampleMs = now;
    tempC = readTemperatureSensor();

    sampleWindow[sampleIndex] = tempC;
    sampleIndex = (sampleIndex + 1) % 5;
    if (sampleIndex == 0) {
      sampleFilled = true;
    }

    currentState = updateState(tempC);
  }

  if (now - lastOutputMs >= OUTPUT_INTERVAL_MS) {
    lastOutputMs = now;
    notifyBySoundAndLed(tempC, currentState);
  }

  if (now - lastDisplayMs >= DISPLAY_INTERVAL_MS) {
    lastDisplayMs = now;
    showTemperatureOnLcd(tempC, currentState);
  }
}

float readTemperatureSensor() {
  int adc = analogRead(PIN_TEMP_SENSOR);

  if (adc <= 0 || adc >= 1023) {
    sensorErrorFlag = true;
    return lastValidTempC;
  }

  float thermistorResistance = SERIES_RESISTOR * ((1023.0 / (float)adc) - 1.0);

  float steinhart = thermistorResistance / THERMISTOR_NOMINAL_RESISTANCE;
  steinhart = log(steinhart);
  steinhart /= THERMISTOR_BETA;
  steinhart += 1.0 / (THERMISTOR_NOMINAL_TEMP_C + 273.15);
  steinhart = 1.0 / steinhart;
  float measuredTemp = steinhart - 273.15;

  if (measuredTemp >= TEMP_VALID_MIN_C && measuredTemp <= TEMP_VALID_MAX_C) {
    sensorErrorFlag = false;
    lastValidTempC = measuredTemp;
    return measuredTemp;
  }

  sensorErrorFlag = true;
  return lastValidTempC;
}

uint8_t updateState(float measuredTempC) {
  unsigned long now = millis();

  if (sensorErrorFlag) {
    if (currentState != STATE_ERROR) {
      errorStartMs = now;
    }
    return STATE_ERROR;
  }

  if (currentState == STATE_WAITING) {
    if (measuredTempC >= TEMP_START_C) {
      measurementStartMs = now;
      lowTempStartMs = 0;
      notifyTonePlayed = false;
      emergencyBlinkFlag = false;
      emergencyBlinkOn = true;
      return STATE_MEASURING;
    }
    return STATE_WAITING;
  }

  if (currentState == STATE_MEASURING) {
    if (!validateMeasurement(measuredTempC)) {
      errorStartMs = now;
      return STATE_ERROR;
    }

    if ((now - measurementStartMs) >= MEASUREMENT_TIMEOUT_MS && isTemperatureStable()) {
      finalTempC = measuredTempC;
      emergencyBlinkFlag = (finalTempC > TEMP_EMERGENCY_C);
      notifyStartMs = now;
      notifyTonePlayed = false;
      emergencyBlinkOn = true;
      return STATE_NOTIFY;
    }

    return STATE_MEASURING;
  }

  if (currentState == STATE_NOTIFY) {
    if ((now - notifyStartMs) >= NOTIFY_HOLD_MS) {
      emergencyBlinkFlag = false;
      noTone(PIN_BUZZER);
      return STATE_WAITING;
    }
    return STATE_NOTIFY;
  }

  if (currentState == STATE_ERROR) {
    if ((now - errorStartMs) >= ERROR_HOLD_MS || measuredTempC >= TEMP_START_C) {
      noTone(PIN_BUZZER);
      return STATE_WAITING;
    }
    return STATE_ERROR;
  }

  return STATE_WAITING;
}

bool validateMeasurement(float measuredTempC) {
  unsigned long now = millis();

  if (measuredTempC < TEMP_VALID_MIN_C || measuredTempC > TEMP_VALID_MAX_C) {
    return false;
  }

  if (measuredTempC < TEMP_START_C) {
    if (lowTempStartMs == 0) {
      lowTempStartMs = now;
    }
    if ((now - lowTempStartMs) >= LOW_TEMP_ERROR_MS) {
      return false;
    }
  } else {
    lowTempStartMs = 0;
  }

  return true;
}

void notifyBySoundAndLed(float measuredTempC, uint8_t state) {
  updateBuzzerByTemperature(measuredTempC, state);
  updateLedByTemperature(measuredTempC, state);
}

void updateBuzzerByTemperature(float measuredTempC, uint8_t state) {
  (void)measuredTempC;
  unsigned long now = millis();

  if (state == STATE_WAITING) {
    noTone(PIN_BUZZER);
    return;
  }

  if (state == STATE_MEASURING) {
    unsigned long elapsed = now - measurementStartMs;
    uint8_t noteIndex = (uint8_t)((elapsed * 8UL) / MEASUREMENT_TIMEOUT_MS);
    if (noteIndex > 7) {
      noteIndex = 7;
    }
    tone(PIN_BUZZER, PROGRESS_NOTES[noteIndex], 120);
    return;
  }

  if (state == STATE_NOTIFY) {
    if (emergencyBlinkFlag) {
      uint8_t idx = (uint8_t)((now / OUTPUT_INTERVAL_MS) % 2);
      tone(PIN_BUZZER, EMERGENCY_TONES[idx], 150);
    } else if (!notifyTonePlayed) {
      tone(PIN_BUZZER, DONE_TONE, 250);
      notifyTonePlayed = true;
    } else {
      noTone(PIN_BUZZER);
    }
    return;
  }

  if (state == STATE_ERROR) {
    uint8_t idx = (uint8_t)((now / 400UL) % 2);
    tone(PIN_BUZZER, ERROR_TONES[idx], 150);
    return;
  }

  noTone(PIN_BUZZER);
}

void updateLedByTemperature(float measuredTempC, uint8_t state) {
  unsigned long now = millis();

  if (state == STATE_WAITING || state == STATE_ERROR) {
    setAllLeds(false);
    return;
  }

  if (state == STATE_NOTIFY && emergencyBlinkFlag) {
    static unsigned long lastBlinkToggleMs = 0;
    if ((now - lastBlinkToggleMs) >= EMERGENCY_BLINK_MS) {
      lastBlinkToggleMs = now;
      emergencyBlinkOn = !emergencyBlinkOn;
    }
    setAllLeds(emergencyBlinkOn);
    return;
  }

  float displayTemp = (state == STATE_NOTIFY) ? finalTempC : measuredTempC;

  if (displayTemp <= TEMP_START_C) {
    applyLedPattern(1);
  } else if (displayTemp < TEMP_STEP_2_C) {
    applyLedPattern(2);
  } else if (displayTemp < TEMP_STEP_3_C) {
    applyLedPattern(3);
  } else if (displayTemp < TEMP_STEP_4_C) {
    applyLedPattern(4);
  } else {
    applyLedPattern(5);
  }
}

void showTemperatureOnLcd(float measuredTempC, uint8_t state) {
  lcd.clear();

  if (state == STATE_WAITING) {
    lcd.setCursor(0, 0);
    lcd.print("Ready");
    lcd.setCursor(0, 1);
    lcd.print("----");
    return;
  }

  if (state == STATE_ERROR) {
    lcd.setCursor(0, 0);
    lcd.print("Error");
    lcd.setCursor(0, 1);
    lcd.print("Sensor Error");
    return;
  }

  float shown = (state == STATE_NOTIFY) ? finalTempC : measuredTempC;

  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(shown, 1);
  lcd.print((char)223);
  lcd.print("C");

  lcd.setCursor(0, 1);
  if (state == STATE_MEASURING) {
    lcd.print("Measuring...");
  } else if (emergencyBlinkFlag) {
    lcd.print("Emergency Alert");
  } else {
    lcd.print("Measurement OK");
  }
}

void setAllLeds(bool on) {
  digitalWrite(PIN_LED_BLUE_1, on ? HIGH : LOW);
  digitalWrite(PIN_LED_GREEN_1, on ? HIGH : LOW);
  digitalWrite(PIN_LED_GREEN_2, on ? HIGH : LOW);
  digitalWrite(PIN_LED_YELLOW_1, on ? HIGH : LOW);
  digitalWrite(PIN_LED_YELLOW_2, on ? HIGH : LOW);
  digitalWrite(PIN_LED_RED_1, on ? HIGH : LOW);
}

void applyLedPattern(uint8_t level) {
  digitalWrite(PIN_LED_BLUE_1, level >= 1 ? HIGH : LOW);
  digitalWrite(PIN_LED_GREEN_1, level >= 2 ? HIGH : LOW);
  digitalWrite(PIN_LED_GREEN_2, level >= 3 ? HIGH : LOW);
  digitalWrite(PIN_LED_YELLOW_1, level >= 4 ? HIGH : LOW);
  digitalWrite(PIN_LED_YELLOW_2, level >= 5 ? HIGH : LOW);
  digitalWrite(PIN_LED_RED_1, level >= 6 ? HIGH : LOW);
}

bool isTemperatureStable() {
  if (!sampleFilled) {
    return false;
  }

  float minTemp = sampleWindow[0];
  float maxTemp = sampleWindow[0];
  for (uint8_t i = 1; i < 5; i++) {
    if (sampleWindow[i] < minTemp) {
      minTemp = sampleWindow[i];
    }
    if (sampleWindow[i] > maxTemp) {
      maxTemp = sampleWindow[i];
    }
  }

  return (maxTemp - minTemp) <= HYSTERESIS_C;
}
