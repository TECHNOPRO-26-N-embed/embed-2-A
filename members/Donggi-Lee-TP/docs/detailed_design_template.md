# 詳細設計書 — 組込み開発実習

<!-- 作成者: イドンギ / 日付: 2026-05-25 / グループ: 2-A -->

> **このドキュメントの目的**
> 基本設計書（basic_design.md）で「**どのような構造で作るか**」を決めました。
> この詳細設計書では「**各処理を具体的にどう実装するか**」を決めます。
> 書き終わったとき、**コードの骨格がほぼ完成している**状態を目指してください。

> [!NOTE]
> **V字モデルにおける位置づけ**
> 詳細設計書 ←→ **単体テスト**（関数・部品ごとのテスト）が対応します。
> 「この関数が正しく動くか」の確認は Section 5 の単体テスト仕様書で計画します。
> ※ 必須機能全体が動くかの「結合テスト」は基本設計書（Section 6）に記載します。

---

## 0. 基本設計書との接続確認

| 項目 | basic_design.md から転記 |
|:--|:--|
| 作品タイトル | 音で知らせる体温計 |
| 状態の種類（1-2 状態遷移から） | 待機中 / 計測中 / 判定・通知 / 測定エラー |
| 実装する関数の数（2-2 関数一覧から） | 10個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 約23B |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
【ピン定義】（basic_design.md 3-1 から転記）
  PIN_TEMP_SENSOR = A0  // LM35温度センサー（アナログ入力）
  PIN_4DIGIT_CLK  = 2   // 4digit表示器 TM1637 CLK
  PIN_4DIGIT_DIO  = 3   // 4digit表示器 TM1637 DIO
  PIN_LED_BLUE_1   = 4   // 青LED（最下段）
  PIN_LED_GREEN_1  = 5   // 緑LED（下から2段目）
  PIN_LED_GREEN_2  = 6   // 緑LED（下から3段目）
  PIN_LED_YELLOW_1 = 7   // 黄LED（下から4段目）
  PIN_LED_YELLOW_2 = 8   // 黄LED（下から5段目）
  PIN_LED_RED_1    = 9   // 赤LED（最上段）
  PIN_BUZZER      = 13  // パッシブブザー

【状態管理】（basic_design.md 1-2 の状態名から転記）
  currentState      : uint8_t = 0  // 0:待機 1:計測中 2:判定・通知 3:測定エラー

【タイマー（millis()用）】（basic_design.md 2-3 から転記）
  measurementStartMs : unsigned long = 0
  lastSampleMs       : unsigned long = 0
  lastOutputMs       : unsigned long = 0
  lastDisplayMs      : unsigned long = 0
  errorStartMs       : unsigned long = 0

【センサー・入力値】（basic_design.md 2-1 から転記）
  tempRaw          : int   = 0
  tempC            : float = 0.0
  finalTempC       : float = 0.0
  displayTemp10x   : int   = 0

【その他のフラグ・カウンター】
  stableCount      : uint8_t = 0
  sensorErrorFlag  : bool    = false
  emergencyBlinkFlag : bool  = false

【定数】
  TEMP_START_C            : const float = 35.0
  TEMP_STEP_2_C           : const float = 36.0
  TEMP_STEP_3_C           : const float = 37.0
  TEMP_STEP_4_C           : const float = 38.0
  TEMP_EMERGENCY_C        : const float = 39.0
  TEMP_VALID_MIN_C        : const float = 20.0
  TEMP_VALID_MAX_C        : const float = 45.0
  SAMPLE_INTERVAL_MS      : const unsigned long = 100
  OUTPUT_INTERVAL_MS      : const unsigned long = 200
  DISPLAY_INTERVAL_MS     : const unsigned long = 50
  MEASUREMENT_TIMEOUT_MS  : const unsigned long = 30000
  EMERGENCY_BLINK_MS      : const unsigned long = 200
  LOW_TEMP_ERROR_MS       : const unsigned long = 5000
  HYSTERESIS_C            : const float = 0.2
```

---

## 2. 各関数の詳細設計

> ※ 基本設計書（2-2 関数一覧）で定義した各関数の「中身」を設計します。
> **疑似コード**（日本語＋処理の流れ）で書いてください。実際のC++コードは書かなくてOKです。

---

### `setup()` — 初期化処理

```
【処理の流れ】
1. ピンモードを設定する
   - PIN_BUTTON  → INPUT_PULLUP
   - PIN_LED_*   → OUTPUT
   - PIN_BUZZER  → OUTPUT

2. ライブラリの初期化（使うものだけ）
   - 例: lcd.begin(16, 2)
   - 例: servo.attach(PIN_SERVO)

3. Serial.begin(9600)（デバッグ用）

4. 起動確認（任意）: 緑LEDを1秒点灯して消灯
```

**↓ 自分の setup() を設計してください**
```
【処理の流れ】
1. 各ピンモードを設定する
  - PIN_TEMP_SENSOR: 入力（analogRead使用）
  - PIN_LED_BLUE_1 / PIN_LED_GREEN_1 / PIN_LED_GREEN_2 / PIN_LED_YELLOW_1 / PIN_LED_YELLOW_2 / PIN_LED_RED_1: OUTPUT
  - PIN_BUZZER: OUTPUT
2. 4digit表示器ライブラリを初期化し、輝度を標準値に設定する
3. Serial.begin(9600) を実行する（デバッグ時のみ有効化）
4. 初期表示として4digitに `----` を表示し、LED全消灯・ブザー停止を確認する
5. 起動確認音を150msだけ鳴らし、currentStateを待機中(0)に設定する
```

---

### `loop()` — メインループ

> ※ loop() は「状態ごとに何をするか」だけ書く。細かい処理は各関数に任せる。

```
【処理の流れ】

＜毎ループ実行すること＞
  - 入力を読む（readButton(), readSensor() などを呼ぶ）
  - 現在時刻を取得: now = millis()

＜currentState が 0（待機中）のとき＞
  - センサー値を監視する
  - 検知条件を満たしたら → currentState = 1

＜currentState が 1（動作中）のとき＞
  - メイン処理を行う
  - 終了条件を満たしたら → currentState = 2

＜currentState が 2（完了）のとき＞
  - 完了表示をする
  - リセットボタンが押されたら → currentState = 0

＜currentState が 3（エラー）のとき＞
  - エラー表示をする / リセットを待つ
```

**↓ 自分の loop() を設計してください**
```
【処理の流れ】

＜毎ループ実行すること＞
- now = millis() を取得する
- 100msごとに readTemperatureSensor() を実行し tempC を更新する
- updateState(tempC) を実行して currentState を更新する
- notifyBySoundAndLed(tempC, currentState) を実行する
- showTemperatureOn4Digit(displayTemp10x) を実行する

＜currentState が 0（待機中） のとき＞
- tempC >= 35.0 を検知したら計測開始時刻 measurementStartMs = now を保存し、currentState = 1
- それ以外は `----` 表示を維持する


＜currentState が 1（計測中） のとき＞
- tempC を0.1℃単位に変換して displayTemp10x を更新する
- 30秒経過かつ温度の変動が小さい場合は currentState = 2
- tempC < 35.0 が5秒継続した場合は currentState = 3


＜currentState が 2（判定・通知） のとき＞
- 最終温度帯に対応する音・LEDパターンを出力する
- emergencyBlinkFlag=true（= finalTempC > 39.0）の場合は、青〜赤の全段を一度点灯してから200ms周期で点滅する
- 3秒通知後に currentState = 0 に戻す

＜currentState が 3（測定エラー） のとき＞
- エラー音・LED消灯・`Err` 表示を出力する
- 2秒経過または温度回復（>=35.0）で currentState = 0 に戻す

```

---

### （関数ごとに以下のブロックをコピーして追加してください）

> ※ 基本設計書 2-2 の関数一覧に記載した関数を1つずつ設計します。

---

### `readTemperatureSensor()` — 温度センサー値を取得して温度へ変換する

**basic_design.md 2-2 との対応：** 温度センサーを読み取り、温度値に変換する。

**引数：** なし

**戻り値：** float（温度℃）

```
【処理の流れ】
1. analogRead(PIN_TEMP_SENSOR) で tempRaw を取得する
2. 電圧値へ変換し、LM35の係数（10mV/℃）で tempC を算出する
3. tempC が有効範囲（20.0〜45.0）内なら sensorErrorFlag = false にする
4. 範囲外なら sensorErrorFlag = true とし、戻り値は直前の有効値を返す

【エラー・異常ケース】
- 範囲外値が連続した場合: 異常値は採用せず、前回有効値を保持する
```

### `updateState(tempC)` — 温度と時間から状態遷移を決める

**basic_design.md 2-2 との対応：** 温度・経過時間から次状態を決定する。

**引数：** `tempC`（float）: 現在温度

**戻り値：** uint8_t（次状態）

```
【処理の流れ】
1. currentState ごとに遷移条件を判定する
2. 待機中: tempC >= 35.0 で計測中へ遷移する
3. 計測中: 30秒経過かつ安定条件成立で finalTempC = tempC を保存し、判定・通知へ遷移する
4. 判定・通知へ遷移するとき、finalTempC > 39.0 なら emergencyBlinkFlag=true、そうでなければ false にする
5. 計測中: tempC < 35.0 が5秒継続で測定エラーへ遷移する
6. 判定・通知: 通知時間満了で待機中へ戻る
7. 測定エラー: 一定時間経過または温度回復で待機中へ戻る

【エラー・異常ケース】
- sensorErrorFlag=true のとき: 状態遷移を保留し、誤遷移を防止する
```

### `validateMeasurement(tempC)` — 測定成立可否を判定する

**basic_design.md 2-2 との対応：** 35℃未満継続や範囲外値を検出する。

**引数：** `tempC`（float）: 現在温度

**戻り値：** bool（true: 測定成立, false: 測定不成立）

```
【処理の流れ】
1. tempC が20.0〜45.0の範囲外なら false を返す
2. tempC < 35.0 の間は低温継続タイマーを進める
3. 低温継続が5秒を超えたら false を返す
4. それ以外は true を返す

【エラー・異常ケース】
- 計測開始前に呼ばれた場合: false を返して遷移を抑止する
```

### `updateBuzzerByTemperature(tempC, state)` — 状態と温度帯に応じて音を出す

**basic_design.md 2-2 との対応：** 測定進捗に応じた音階と判定後の通知音を出力する。

**引数：** `tempC`（float）, `state`（uint8_t）

**戻り値：** void

```
【処理の流れ】
1. state が計測中なら、経過時間（0〜30秒）を8段階に分けてドレミファソラシドを鳴らす
2. state が判定・通知なら finalTempC に基づく確定音を鳴らす
  - 35.0〜39.0℃: 完了音を1回
  - 39.0℃超: 緊急音を短周期で繰り返す
3. state が測定エラーなら力が抜ける音を短く繰り返す
4. state が待機中なら noTone() で停止する

【エラー・異常ケース】
- tempC が不正値の場合: 既定の短い警告音1回のみ出して停止する
```

### `updateLedByTemperature(tempC, state)` — 状態と温度帯に応じてLEDを制御する

**basic_design.md 2-2 との対応：** 青1+緑2+黄2+赤1を温度帯に応じて下段から段階点灯する。

**引数：** `tempC`（float）, `state`（uint8_t）

**戻り値：** void

```
【処理の流れ】
1. 待機中は全LED消灯
2. 計測中と判定・通知（39.0℃以下）では以下の段階で点灯する
  - tempC <= 35.0: 青1（ただし5秒継続して35.0未満なら測定エラー遷移）
  - 35.0 < tempC < 36.0: 青1 + 緑1
  - 36.0 <= tempC < 37.0: 青1 + 緑1 + 緑2
  - 37.0 <= tempC < 38.0: 青1 + 緑1 + 緑2 + 黄1
  - 38.0 <= tempC <= 39.0: 青1 + 緑1 + 緑2 + 黄1 + 黄2
3. 判定・通知で emergencyBlinkFlag=true の場合は、青1〜赤1を全点灯後に200ms周期で全段点滅する
4. 測定エラーは全LED消灯を維持する

【エラー・異常ケース】
- 状態値が未定義の場合: 全LED消灯にフォールバックする
```

### `update4DigitDisplay(displayTemp10x, state)` — 4digit表示器へ表示データを送る

**basic_design.md 2-2 との対応：** 現在温度またはエラーコードを4digitへ表示する。

**引数：** `displayTemp10x`（int）, `state`（uint8_t）

**戻り値：** void

```
【処理の流れ】
1. state が待機中なら `----` を表示する
2. 計測中または判定・通知なら displayTemp10x を小数1桁で表示する
3. 測定エラーなら `Err` を表示する
4. 更新周期（50ms）を守って多重更新を防ぐ

【エラー・異常ケース】
- 表示器通信に失敗した場合: 次周期で再送し、1秒超なら Err を維持する
```

### `notifyBySoundAndLed(tempC, state)` — 音通知とLED通知を統合して呼び出す

**basic_design.md 2-2 との対応：** 状態と温度帯に合わせて通知出力を統合する。

**引数：** `tempC`（float）, `state`（uint8_t）

**戻り値：** void

```
【処理の流れ】
1. updateBuzzerByTemperature(tempC, state) を呼ぶ
2. updateLedByTemperature(tempC, state) を呼ぶ
3. 両者の周期がずれないよう同じタイミング基準で実行する

【エラー・異常ケース】
- いずれかが異常を返した場合: 出力を安全側（消灯・停止）にする
```

### `showTemperatureOn4Digit(displayTemp10x)` — 温度値を4桁表示形式へ整形する

**basic_design.md 2-2 との対応：** 0.1℃単位の温度を4桁表示に整形する。

**引数：** `displayTemp10x`（int）

**戻り値：** void

```
【処理の流れ】
1. 温度値を 0000〜9999 の範囲に収める
2. 小数点位置を固定して 36.8 -> 368 形式で表示用バッファを作る
3. update4DigitDisplay() に整形済みデータを渡す

【エラー・異常ケース】
- 値が範囲外の場合: 上限下限へ丸めて表示する
```

---

## 3. 重要ロジックの詳細設計

### 3-1. チャタリング防止（デバウンス処理）

> ※ ボタンを使う場合は必ず設計してください。

```
【考え方】
  本システムはボタンを使わないため、チャタリング対策は不要。
  代わりにアナログ温度値の揺れ対策（移動平均＋ヒステリシス）を適用する。

【処理の流れ】
  1. 直近5サンプルの平均値 tempAvg を算出する
  2. 閾値判定は tempAvg を用いる
  3. 状態遷移時は ±0.2℃ のヒステリシスを設ける

【必要な変数（Section 1 に追加済みか確認）】
  stableCount   : uint8_t
  HYSTERESIS_C  : const float = 0.2
```

---

### 3-2. millis() を使ったタイマー管理

```
【考え方】
  「前回実行した時刻」を記録しておき、「今の時刻 − 前回時刻 ≥ 周期」なら実行する。

【処理の流れ（例: LED点滅）】
  1. now = millis()
  2. now - lastMillis_LED >= LED_INTERVAL かどうか確認
  3. 条件を満たした場合: LEDのON/OFFを切り替え、lastMillis_LED = now
  4. 条件を満たさない場合: 何もしない（次のループで再チェック）

【自分のシステムで millis() を使う処理】
  1. 温度センサー読み取り（100ms）
    - now - lastSampleMs >= 100 で readTemperatureSensor() を実行
  2. 音階更新（200ms）
    - now - lastOutputMs >= 200 で updateBuzzerByTemperature() を実行
  3. LED更新（100ms）
    - now - lastOutputMs >= 100 の周期で updateLedByTemperature() を実行
  4. 4digit更新（50ms）
    - now - lastDisplayMs >= 50 で update4DigitDisplay() を実行
  5. 30秒タイムアウト判定
    - now - measurementStartMs >= 30000 で判定・通知へ移行
  6. 緊急時点滅（200ms）
    - 判定・通知かつ emergencyBlinkFlag=true のとき、now - lastOutputMs >= 200 で全段ON/OFFを反転
```

---

### 3-3. その他の重要ロジック（任意）

> **【任意】** 複雑なロジックがある場合のみ記入してください。
> 例：「距離に応じたLED点灯パターン」「ゲームの衝突判定」「温度の閾値判定」

```
【処理の流れ】
1.
2.
3.

【入力値と出力値の関係】

```

---

## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 | センサー値が正しく取れているか | `readSensor()` | `Serial.println(sensorValue);` |
| 2 | 状態遷移が正しく起きているか | `loop()` | `Serial.println(currentState);` |
| 3 | チャタリング処理が効いているか | `readButton()` | `Serial.println("btn confirmed");` |
| 4 |  |  |  |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | readTemperatureSensor() | 36.5℃相当入力を与える | 36.x℃の値が返る | | [ ] |
| 2 | readTemperatureSensor() | 20.0℃未満/45.0℃超を入力 | 異常値として無効化される | | [ ] |
| 3 | validateMeasurement() | 35.0℃以上を維持 | true を返す | | [ ] |
| 4 | validateMeasurement() | 35.0℃未満を5秒継続 | false を返す | | [ ] |
| 5 | updateState() | 30秒経過+安定値 | 判定・通知(2)へ遷移する | | [ ] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | updateLedByTemperature() | state=2, tempC < 35.0 | 青1のみ点灯する | | [ ] |
| 2 | updateLedByTemperature() | state=2, tempC = 36.5 | 青1+緑1+緑2が点灯する | | [ ] |
| 3 | updateLedByTemperature() | state=2, tempC >= 39.0（30秒経過） | 青〜赤を全点灯後、200ms周期で全段点滅する | | [ ] |
| 4 | updateBuzzerByTemperature() | state=1（計測中） | 進捗に応じて音階が上がる | | [ ] |
| 5 | update4DigitDisplay() | state=1, displayTemp10x=365 | 36.5 が表示される | | [ ] |

### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | センサー100ms周期の確認 | Serial出力時刻差を確認 | 約100ms間隔で更新される | | [ ] |
| 2 | 4digit 50ms更新の確認 | 表示更新ログの間隔を確認 | 約50ms間隔で更新される | | [ ] |
| 3 | 30秒タイムアウトの確認 | 計測開始後30秒待つ | 判定・通知へ遷移する | | [ ] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**
- `float` と `int` の相互変換箇所で丸め誤差が出やすいので、表示用値は最後に1回だけ整数化するほうが安全。
- 状態2（判定・通知）で再判定を続けると通知が揺れる可能性があるため、最終温度帯を固定する設計が必要。
- センサー異常時に前回有効値を使う方針は妥当だが、連続異常回数の上限を決めるとデバッグしやすい。

**対応した内容：**
- `displayTemp10x` への変換は表示直前に限定する方針を追記。
- 3-3に「判定完了後は再判定しない」仕様を追記。
- 異常系で `sensorErrorFlag` を使った安全側制御を明記。

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**
- 入力系では 35.0℃境界値、20.0℃/45.0℃境界値のテストが不足しやすいため明示が必要。
- 出力系では 39℃以上の赤点滅周期を数値で確認する項目が必要。
- タイミング系は 100ms/50ms/30秒 の3種類を分けて検証すると原因切り分けしやすい。

**対応した内容：**
- 5-1に境界値テスト項目を追加。
- 5-2に赤LED 200ms点滅の確認項目を追加。
- 5-3を周期別テストに再構成。

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 | 体の脇の下以外の部位の測定も考慮しているのか？ | 飯田翔斗 | 回路設計に注意 |
| 2 |  |  |  |
| 3 |  |  |  |

### 7-2. レビューを受けて変更した点

-体温計がどんな形で作られるかによって変わるから、できるだけ気を使って作ってみる
-

---

*初版: 2026-05-25 / AIレビュー: 2026-05-25 / グループレビュー後更新: 2026-05-25*
