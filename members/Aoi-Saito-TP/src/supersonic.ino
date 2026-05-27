#include "SR04.h"

#define TRIG_PIN 12
#define ECHO_PIN 11
#define BUZZER_PIN 8

SR04 sr04 = SR04(ECHO_PIN, TRIG_PIN);
long distance;

void setup() {
  Serial.begin(9600);
  pinMode(BUZZER_PIN, OUTPUT);
  delay(1000);
}

void loop() {
  distance = sr04.Distance();

  Serial.print(distance);
  Serial.println(" cm");

  if (distance > 0 && distance <= 30) {
    int freq = map(distance, 30, 1, 200, 2000);
    tone(BUZZER_PIN, freq);
  } else {
    noTone(BUZZER_PIN);
  }

  delay(100);
}