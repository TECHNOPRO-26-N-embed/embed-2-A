# 詳細設計書 — 組込み開発実習

<!-- 作成者: 山下佑樹 / 日付: 2026-05-25 / グループ: 2-A -->

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
| 作品タイトル | TinyMusician |
| 状態の種類（1-2 状態遷移から） | 待機中 / 出題 / 入力 / 判定 / 結果表示（実装時に確認状態を追加） |
| 実装する関数の数（2-2 関数一覧から） | 8個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 27B |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
【ピン定義】（basic_design.md 3-1 から転記）
  PIN_STICK_X   = A0   // ジョイスティックX
  PIN_STICK_Y   = A1   // ジョイスティックY
  PIN_STICK_SW  = 2    // ジョイスティックSW（INPUT_PULLUP）
  PIN_BUZZER    = 8    // パッシブブザー
  PIN_LED_WHITE = 9    // 白色LED

【状態管理】（basic_design.md 1-2 の状態名から転記）
  currentState      : int = 0      // 0:待機 1:出題 2:入力 3:確認 4:判定 5:結果表示
  nextState         : int = 0      // 遷移先予約（必要時のみ使用）

【音・判定仕様】
  targetNote        : int = 0      // 出題音（Hz）
  playerNote        : int = 0      // 入力音（Hz）
  playerDurationMs  : int = 100    // 入力音長（ms）
  NOTE_MIN_HZ       : const int = 262
  NOTE_MAX_HZ       : const int = 494
  NOTE_TOLERANCE_HZ : const int = 20
  DURATION_MIN_MS   : const int = 100
  DURATION_MAX_MS   : const int = 300

【入力値】
  stickValueX       : int = 0
  stickValueY       : int = 0
  prevX             : int = 0
  prevY             : int = 0
  deadband          : const int = 8   // アナログノイズ無視幅

【ボタン制御】
  swRaw             : bool = true   // INPUT_PULLUPなので未押下=true
  swPrevRaw         : bool = true
  swConfirmed       : bool = true
  lastDebounceTime  : unsigned long = 0
  pressStartTime    : unsigned long = 0
  DEBOUNCE_DELAY    : const int = 50     // ms
  REINPUT_DELAY     : const int = 200    // ms
  LONG_PRESS_MS     : const int = 1000   // ms

【タイマー（millis()用）】
  nowMs             : unsigned long = 0
  lastReadTime      : unsigned long = 0   // 50ms周期入力読取
  lastBlinkTime     : unsigned long = 0   // 500ms周期LED点滅
  lastInputTime     : unsigned long = 0   // 再入力受付間隔
  READ_INTERVAL_MS  : const int = 50
  BLINK_INTERVAL_MS : const int = 500

【出力制御】
  ledState          : bool = false
  isCorrect         : bool = false
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
   - PIN_STICK_SW  → INPUT_PULLUP
   - PIN_LED_WHITE → OUTPUT
   - PIN_BUZZER    → OUTPUT

2. 初期状態を設定する
   - currentState = 0（待機中）
   - LED消灯、ブザー停止

3. 乱数シードを設定する
   - analogRead(A5) など未使用ピンを利用して randomSeed()

4. Serial.begin(9600)（任意デバッグ）
```

---

### `loop()` — メインループ

> ※ loop() は「状態ごとに何をするか」だけ書く。細かい処理は各関数に任せる。

```
【処理の流れ】

＜毎ループ実行すること＞
  - nowMs = millis()
  - readJoystick()
  - readSwitchWithDebounce()

＜currentState が 0（待機中）のとき＞
  - LED消灯
  - 短押しを検知したら currentState = 1（出題）

＜currentState が 1（出題）のとき＞
  - playTargetTone() を1回だけ実行
  - 再生完了後 currentState = 2（入力）

＜currentState が 2（入力）のとき＞
  - updatePlayerTone() でY軸から音程、X軸から音長を更新
  - 必要に応じて入力音を鳴らす
  - 短押しなら currentState = 3（確認）
  - 長押し（1秒）なら currentState = 4（判定）

＜currentState が 3（確認）のとき＞
  - checkAnswer() で暫定結果のみ表示（確定しない）
  - 表示後 currentState = 2（入力）へ戻す

＜currentState が 4（判定）のとき＞
  - ブザーで「お題の音→プレイヤーの音」を順に再生
  - checkAnswer() で最終判定
  - isCorrect を確定
  - currentState = 5（結果表示）

＜currentState が 5（結果表示）のとき＞
  - 正解時: LED点灯 + 高音短音
  - 不正解時: blinkLED() + 低音短音
  - 一定時間後または短押しで currentState = 0（待機中）
```

---

### `playTargetTone()` — ランダムな目標音を再生する

**basic_design.md 2-2 との対応：** F01 ランダム音再生

**引数：** なし  
**戻り値：** void

```
【処理の流れ】
1. NOTE_MIN_HZ〜NOTE_MAX_HZ で targetNote をランダム生成
2. 基本音長（例: 200ms）で targetNote をブザー再生
3. 再生終了フラグを立てる

【エラー・異常ケース】
- 生成値が範囲外になった場合:
  NOTE_MIN_HZ に丸める
```

---

### `updatePlayerTone()` — ジョイスティック入力から音程と音長を更新する

**basic_design.md 2-2 との対応：** F02 音程調整

**引数：** なし  
**戻り値：** void

```
【処理の流れ】
1. stickValueY を NOTE_MIN_HZ〜NOTE_MAX_HZ に線形マップして playerNote を更新
2. stickValueX を DURATION_MIN_MS〜DURATION_MAX_MS に線形マップして playerDurationMs を更新
3. 前回値との差が deadband 未満なら値更新を抑制（ノイズ除去）
4. 音の再生は READ_INTERVAL_MS（50ms）ごとに制限する  
5. 音の再生時間は30〜50ms程度とし、過度な連続再生を防ぐ

【エラー・異常ケース】
- A/D値が0固定または1023固定で異常と判断した場合:
  直前正常値を保持し、必要なら再取得
```

---

### `selectDifficulty()` — 難易度を選択する

**basic_design.md 2-2 との対応：** A01 難易度選択

**引数：** なし  
**戻り値：** void

```
【処理の流れ】
1. 入力待機中に長押し回数または左右入力で難易度を選択
2. 難易度に応じて許容誤差・音域・制限時間を設定
3. 選択結果を短い効果音で通知

【エラー・異常ケース】
- 未選択のまま開始要求が来た場合:
  初期難易度（Normal）を適用
```

---

### `blinkLED()` — LEDを周期点滅する

**basic_design.md 2-2 との対応：** A02 LED点滅

**引数：** なし  
**戻り値：** void

```
【処理の流れ】
1. nowMs - lastBlinkTime >= BLINK_INTERVAL_MS を判定
2. 条件成立時のみ ledState を反転
3. digitalWrite(PIN_LED_WHITE, ledState) を実行
4. lastBlinkTime = nowMs を更新

【エラー・異常ケース】
- タイマーオーバーフロー時:
  差分比較を使っているため継続動作可能
```

---

### `checkAnswer()` — 目標音と入力音を比較して結果を出す

**basic_design.md 2-2 との対応：** A03 確認処理

**引数：** なし  
**戻り値：** bool（正解なら true）

```
【処理の流れ】
1. 差分 diff = abs(targetNote - playerNote) を計算
2. 判定時はブザーで targetNote を短く再生する
3. 続けてブザーで playerNote を短く再生する
4. diff <= NOTE_TOLERANCE_HZ なら正解、それ以外は不正解
5. 確認モード時は結果表示のみ（状態確定はしない）
6. 判定モード時は isCorrect を更新し、結果表示状態へ遷移

【エラー・異常ケース】
- 未出題で targetNote=0 の場合:
  判定せず false を返し、待機に戻す
```

---

### `doOptionalEvent()` — 追加イベントを実行する

**basic_design.md 2-2 との対応：** A04 イベント追加

**引数：** なし  
**戻り値：** void

```
【処理の流れ】
1. 左右入力が閾値を超えたイベントを検知
2. 追加演出（連続音、LEDパターン）を実行
3. 一定時間で入力状態に復帰

【エラー・異常ケース】
- イベント中にボタン押下が来た場合:
  中断して入力状態へ戻る
```

---

### `readJoystick()` — ジョイスティックのアナログ値を周期読取する

**basic_design.md 2-2 との対応：** 共通入力処理（updatePlayerTone から利用）

**引数：** なし  
**戻り値：** void

```
【処理の流れ】
1. nowMs - lastReadTime >= READ_INTERVAL_MS のときのみ読取
2. A0/A1を読み取り、prevX/prevYとの差分を確認
3. deadband以上の変化のみ stickValueX/Y に反映
4. lastReadTime を更新

【エラー・異常ケース】
- 異常値が継続する場合:
  直前正常値を保持し、デバッグログに記録
```

---

### `readSwitchWithDebounce()` — 押下イベントをデバウンス付きで確定する

**basic_design.md 2-2 との対応：** 共通入力処理（checkAnswer/状態遷移で利用）

**引数：** なし  
**戻り値：** void

```
【処理の流れ】
1. raw = digitalRead(PIN_STICK_SW) を取得
2. rawが前回値と異なる場合 lastDebounceTime = nowMs
3. nowMs - lastDebounceTime >= DEBOUNCE_DELAY のとき確定値を更新
4. 押下エッジ（true→false）で pressStartTime を記録
5. 解放時に押下時間を計算し、短押し/長押しフラグを更新

【エラー・異常ケース】
- 高速連打で REINPUT_DELAY 未満の場合:
  入力イベントを無視する
```

---

## 3. 重要ロジックの詳細設計

### 3-1. チャタリング防止（デバウンス処理）

> ※ ボタンを使う場合は必ず設計してください。

```
【考え方】
  ボタンが押されたとき、50ms 以内の連続入力は「同じ1回の押下」として無視する。

【処理の流れ】
  1. ボタンのデジタル値を読む（digitalRead）
  2. 前回確定した時刻（lastDebounceTime）からの経過時間を計算する
  3. 経過時間 < DEBOUNCE_DELAY（50ms）→ 無視する
  4. 経過時間 >= DEBOUNCE_DELAY → ボタンの状態として確定する
  5. 確定押下の間隔が REINPUT_DELAY（200ms）未満なら無視する

【必要な変数（Section 1 に追加済みか確認）】
  lastDebounceTime : unsigned long
  DEBOUNCE_DELAY   : const int = 50
  REINPUT_DELAY    : const int = 200
```

---

### 3-2. millis() を使ったタイマー管理

```
【考え方】
  「前回実行した時刻」を記録しておき、「今の時刻 - 前回時刻 >= 周期」なら実行する。

【自分のシステムで millis() を使う処理】
  1. LED点滅: 500ms周期（BLINK_INTERVAL_MS）
  2. ジョイスティック読取: 50ms周期（READ_INTERVAL_MS）
  3. 長押し判定: 押下継続が1000ms以上（LONG_PRESS_MS）
  4. 再入力受付: 前回入力から200ms以上（REINPUT_DELAY）

【処理の流れ（例: LED点滅）】
  1. nowMs = millis()
  2. nowMs - lastBlinkTime >= BLINK_INTERVAL_MS を判定
  3. 条件成立時のみ LED状態を反転して更新
  4. 条件不成立時は何もしない
```

---

### 3-3. その他の重要ロジック（長押しによる確認遷移）

```
【処理の流れ】
1. ボタン押下開始時刻 pressStartTime を記録
2. 押下中に nowMs - pressStartTime を監視
3. 1000ms未満で解放: 短押しとして確認状態へ
4. 1000ms以上で解放: 長押しとして提出（判定状態へ）
5. 確認状態では checkAnswer() を実行後、入力状態へ戻す

【入力値と出力値の関係】
- 短押し: 仮確認して再入力可能
- 長押し: 最終判定へ進む
```

---

## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 | 状態遷移が正しく起きているか | loop() | Serial.println(currentState); |
| 2 | target/playerの値が妥当か | checkAnswer() | Serial.println(String(targetNote) + "," + String(playerNote)); |
| 3 | 長押し判定時間が正しいか | readSwitchWithDebounce() | Serial.println(nowMs - pressStartTime); |
| 4 | デバウンスが効いているか | readSwitchWithDebounce() | Serial.println("btn confirmed"); |
| 5 | アナログノイズの発生状況 | readJoystick() | Serial.println(String(stickValueX) + "," + String(stickValueY)); |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | readSwitchWithDebounce() | SWを1回短押し | 短押しイベントが1回だけ発生 | | [ ] |
| 2 | readSwitchWithDebounce() | SWを素早く2回押す | 50ms未満の重複入力は無視される | | [ ] |
| 3 | readSwitchWithDebounce() | SWを1秒以上長押し | 長押しイベントが発生する | | [ ] |
| 4 | readJoystick() | スティックを中央付近で微動させる | deadband内は値更新しない | | [ ] |
| 5 | readJoystick() | スティックを最大まで倒す | 0〜1023範囲で安定取得 | | [ ] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | playTargetTone() | 関数呼び出し | 262〜494Hzの音が鳴る | | [ ] |
| 2 | updatePlayerTone() | Y軸上下操作 | 音程が連続的に変化する | | [ ] |
| 3 | updatePlayerTone() | X軸左右操作 | 音長が100〜300msで変化する | | [ ] |
| 4 | blinkLED() | 結果表示(不正解)状態 | 500ms周期で点滅する | | [ ] |
| 5 | checkAnswer() | target=440, player=430 | 正解(true)を返す | | [ ] |
| 6 | checkAnswer() | target=440, player=410 | 不正解(false)を返す | | [ ] |

### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | millis()並行動作確認 | LED点滅中にスティック操作 | 入力読取と点滅が同時に成立 | | [ ] |
| 2 | 長押し境界値テスト | 900ms/1000ms/1100msで解放 | 900=短押し, 1000以上=長押し | | [ ] |
| 3 | 再入力受付間隔 | 200ms未満で連打 | 2回目入力が無視される | | [ ] |
| 4 | 演出delay影響確認 | 結果音再生中に入力 | 短時間演出後に正常復帰する | | [ ] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**
1. 確認状態と判定状態の責務が混ざると遷移バグになりやすい
2. 長押し判定を押下中判定にするとチャタリングで誤判定の可能性がある
3. analogRead異常値が続くとplayerNoteが固定化する可能性がある

**対応した内容：**
1. 確認は「仮表示のみ」、判定は「結果確定のみ」に分離した
2. 押下開始時刻記録 + 解放時判定に統一した
3. 異常値時は前回正常値保持と再取得方針を追加した

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**
1. 長押し境界値（1000ms前後）のテストを追加すべき
2. 許容誤差±20Hzの境界値をテストすべき
3. 再入力200msの間隔テストを追加すべき

**対応した内容：**
1. 900/1000/1100msの境界値テストを追加
2. checkAnswer() の正解/不正解テストを追加
3. 再入力受付間隔テストを追加

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 |  |  |  |
| 2 |  |  |  |
| 3 |  |  |  |

### 7-2. レビューを受けて変更した点

-
-

---

*初版: 2026-05-25 / AIレビュー: 2026-05-25 / グループレビュー後更新: 2026-05-25*