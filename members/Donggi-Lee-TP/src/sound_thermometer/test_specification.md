# sound_thermometer 単体テスト仕様書

## 1. 目的
- `sound_thermometer.ino` の関数単位および状態遷移が、要求仕様どおりに動作することを確認する。
- 正常系、境界値、異常系、タイミング要件を明確化し、再現可能なテスト手順を定義する。

## 2. テスト対象
- 対象ファイル: `sound_thermometer.ino`
- 対象関数:
  - `setup()`
  - `loop()`
  - `readTemperatureSensor()`
  - `updateState(float)`
  - `validateMeasurement(float)`
  - `notifyBySoundAndLed(float, uint8_t)`
  - `updateBuzzerByTemperature(float, uint8_t)`
  - `updateLedByTemperature(float, uint8_t)`
  - `showTemperatureOnLcd(float, uint8_t)`
  - `setAllLeds(bool)`
  - `applyLedPattern(uint8_t)`
  - `isTemperatureStable()`

## 3. テスト環境
- Arduino Uno 互換機
- 温度センサ (サーミスタ回路)
- LCD1602 (4bit並列接続)
- LED 6個 (Blue 1, Green 2, Yellow 2, Red 1)
- パッシブブザー
- シリアルモニタ (9600bps)
- 室温・加温環境を再現できる治具 (手動でも可)

## 4. テストケース

### 4.1 初期化 (`setup`)
| ID | 観点 | 手順 | 期待結果 | 実際結果 | 判定 |
|----|------|------|----------|----------|------|
| S-01 | ピン初期化 | 電源投入/リセット | LEDが全消灯初期化される |  |  |
| S-02 | LCD初期表示 | 起動直後LCD確認 | 1行目 `Ready`、2行目 `----` が表示される |  |  |
| S-03 | 起動音 | 起動直後ブザー確認 | 523Hz相当の短音が1回鳴る |  |  |
| S-04 | 初期状態 | シリアル出力確認 | `currentState` が `STATE_WAITING` で開始する |  |  |

### 4.2 温度入力・変換 (`readTemperatureSensor`)
| ID | 観点 | 入力・操作 | 期待結果 | 実際結果 | 判定 |
|----|------|------------|----------|----------|------|
| R-01 | 正常ADC | 常温で測定 | 20.0-45.0℃ 範囲の温度が返る |  |  |
| R-02 | ADC下限異常 | ADC=0相当を入力 | `sensorErrorFlag=true`、`lastValidTempC` を返す |  |  |
| R-03 | ADC上限異常 | ADC=1023相当を入力 | `sensorErrorFlag=true`、`lastValidTempC` を返す |  |  |
| R-04 | 範囲外温度低 | 20.0℃未満相当を入力 | `sensorErrorFlag=true`、`lastValidTempC` を返す |  |  |
| R-05 | 範囲外温度高 | 45.0℃超相当を入力 | `sensorErrorFlag=true`、`lastValidTempC` を返す |  |  |
| R-06 | 正常復帰 | 異常後に正常値入力 | `sensorErrorFlag=false` に戻り、`lastValidTempC` が更新される |  |  |

### 4.3 状態遷移 (`updateState`)
| ID | 初期状態 | 入力・条件 | 期待状態遷移 | 実際結果 | 判定 |
|----|----------|------------|--------------|----------|------|
| ST-01 | WAITING | 温度 >= 30.0℃ | MEASURINGへ遷移し計測開始時刻が更新される |  |  |
| ST-02 | WAITING | 温度 < 30.0℃ | WAITING維持 |  |  |
| ST-03 | MEASURING | `validateMeasurement=false` | ERRORへ遷移 |  |  |
| ST-04 | MEASURING | 30秒経過かつ安定判定true | NOTIFYへ遷移し`finalTempC`確定 |  |  |
| ST-05 | MEASURING | 30秒未満 or 安定判定false | MEASURING維持 |  |  |
| ST-06 | NOTIFY | 3秒経過 | WAITINGへ遷移し通知停止 |  |  |
| ST-07 | ERROR | 2秒経過 | WAITINGへ遷移 |  |  |
| ST-08 | ERROR | 温度 >= 30.0℃ | 2秒未満でもWAITINGへ遷移 |  |  |

### 4.4 測定妥当性 (`validateMeasurement`)
| ID | 入力・条件 | 手順 | 期待結果 | 実際結果 | 判定 |
|----|------------|------|----------|----------|------|
| V-01 | 20.0-45.0℃ | 単発評価 | `true` を返す |  |  |
| V-02 | 20.0℃未満 | 単発評価 | `false` を返す |  |  |
| V-03 | 45.0℃超 | 単発評価 | `false` を返す |  |  |
| V-04 | 30.0℃未満継続 | 5秒以上継続 | `false` を返す（低温エラー） |  |  |
| V-05 | 30.0℃未満一時的 | 5秒未満で30.0℃以上へ復帰 | `true` 継続、低温タイマがリセットされる |  |  |

### 4.5 安定判定 (`isTemperatureStable`)
| ID | 観点 | 入力・操作 | 期待結果 | 実際結果 | 判定 |
|----|------|------------|----------|----------|------|
| T-01 | サンプル不足 | 5点未満のまま呼び出し | `false` を返す |  |  |
| T-02 | 安定データ | 5点の差分 <= 0.2℃ | `true` を返す |  |  |
| T-03 | 非安定データ | 5点の差分 > 0.2℃ | `false` を返す |  |  |

### 4.6 ブザー出力 (`updateBuzzerByTemperature`)
| ID | 状態 | 手順 | 期待結果 | 実際結果 | 判定 |
|----|------|------|----------|----------|------|
| B-01 | WAITING | 関数呼び出し | ブザー停止 (`noTone`) |  |  |
| B-02 | MEASURING | 計測開始直後 | 進行度に応じた低い音階が鳴る |  |  |
| B-03 | MEASURING | 計測終盤 | 進行度に応じた高い音階が鳴る |  |  |
| B-04 | NOTIFY(通常) | NOTIFY遷移直後 | 完了音(659Hz)が1回のみ鳴る |  |  |
| B-05 | NOTIFY(緊急) | `finalTempC > 39.0℃` | 2音が交互に鳴る |  |  |
| B-06 | ERROR | ERROR状態維持 | エラー音2パターンが交互に鳴る |  |  |

### 4.7 LED表示 (`updateLedByTemperature` / `applyLedPattern`)
| ID | 状態・温度 | 手順 | 期待結果 | 実際結果 | 判定 |
|----|------------|------|----------|----------|------|
| L-01 | WAITING | 関数呼び出し | 青LEDのみON |  |  |
| L-02 | ERROR | 関数呼び出し | 全LED OFF |  |  |
| L-03 | MEASURING, <=30.0℃ | 温度入力 | レベル1点灯 |  |  |
| L-04 | MEASURING, 30.0-36.0℃未満 | 温度入力 | レベル2点灯 |  |  |
| L-05 | MEASURING, 36.0-37.0℃未満 | 温度入力 | レベル3点灯 |  |  |
| L-06 | MEASURING, 37.0-38.0℃未満 | 温度入力 | レベル4点灯 |  |  |
| L-07 | MEASURING, 38.0-39.0℃以下 | 温度入力 | レベル5点灯 |  |  |
| L-08 | MEASURING, >39.0℃ | 温度入力 | レベル6点灯 |  |  |
| L-09 | NOTIFY(緊急) | 通知開始直後 | 全LED点灯後、200ms周期で全体点滅 |  |  |

### 4.8 LCD表示 (`showTemperatureOnLcd`)
| ID | 状態 | 手順 | 期待結果 | 実際結果 | 判定 |
|----|------|------|----------|----------|------|
| D-01 | WAITING | 関数呼び出し | 1行目`Ready`、2行目`----` |  |  |
| D-02 | ERROR | 関数呼び出し | 1行目`Error`、2行目`Sensor Error` |  |  |
| D-03 | MEASURING | 温度入力 | `Temp: xx.xC` と `Measuring...` を表示 |  |  |
| D-04 | NOTIFY(通常) | `finalTempC <= 39.0` | `Measurement OK` を表示 |  |  |
| D-05 | NOTIFY(緊急) | `finalTempC > 39.0` | `Emergency Alert` を表示 |  |  |

### 4.9 周期処理・連携 (`loop`)
| ID | 観点 | 手順 | 期待結果 | 実際結果 | 判定 |
|----|------|------|----------|----------|------|
| P-01 | サンプル周期 | 連続監視 | 約100msごとに温度更新される |  |  |
| P-02 | 出力周期 | 連続監視 | 約200msごとにLED/ブザー更新される |  |  |
| P-03 | 表示周期 | 連続監視 | 約200msごとにLCD/Serial更新される |  |  |
| P-04 | 総合遷移 | 30℃以上維持し30秒待機 | WAITING->MEASURING->NOTIFY->WAITING が成立 |  |  |

## 5. 実施ログ
| 日付 | 担当 | テストID | 結果 | 備考 |
|------|------|----------|------|------|
|      |      |          |      |      |

## 6. 備考
- 温度境界テストは実機のばらつきがあるため、必要に応じて模擬入力(可変抵抗やADC固定値)で再確認する。
- 緊急通知の点滅周期は `EMERGENCY_BLINK_MS` を基準に、目視だけでなく動画記録で確認すると再現性が高い。
