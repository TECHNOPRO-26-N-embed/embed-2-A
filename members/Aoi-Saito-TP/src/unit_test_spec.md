5-1. 入力系テスト


No	テスト対象関数	入力・操作	期待する結果	実際の結果	合否

1	readSensor()	センサー前方に物体を置く（正常距離）	距離値が取得できる		
2	readSensor()	物体を近づける（閾値内）	近い距離の値が返る		
3	readSensor()	物体を遠ざける（閾値外）	0 または閾値外の値が返る		
4	readSensor()	センサーを遮蔽する	測定失敗時の処理になる		
5	calcFrequency()	cm = 1 を入力	最大音に近い周波数が返る		
6	calcFrequency()	cm = THRESHOLD_CM を入力	最小周波数が返る		
7	calcFrequency()	cm = THRESHOLD_CM + 1 を入力	0 が返る		
8	calcFrequency()	cm = 0 を入力	0 が返る		


5-2. 出力系テスト


No	テスト対象関数	入力・操作	期待する結果	実際の結果	合否

1	showDistance()	距離値を渡す	Serial Monitor に距離が表示される		
2	playToneByDistance()	閾値内の距離を渡す	ブザーが鳴る		
3	playToneByDistance()	閾値外の距離を渡す	ブザーが鳴らない		
4	stopBuzzer()	ブザー鳴動中に呼ぶ	音が停止する		
5	loop()	距離が近い状態にする	警告状態になりブザーが鳴る		
6	loop()	距離が遠い状態にする	待機状態になりブザーが止まる		


5-3. タイミング・連携テスト


No	テスト内容	テスト手順	期待する結果	実際の結果	合否

1	millis()による周期実行	連続で距離を変える	約100msごとに更新される		
2	delay()による停止がないか	動作中に物体を動かす	反応が止まらない		
3	センサー更新とブザー出力の同期	距離を近づけたり離したりする	距離変化に応じて音が変わる		
4	測定失敗時の動作	センサーを遮蔽する	ブザーが停止し、異常動作しない		


5-4. 境界値テスト


No	テスト対象関数	入力・操作	期待する結果	実際の結果	合否

1	calcFrequency()	cm = THRESHOLD_CM	最小周波数になる		
2	calcFrequency()	cm = THRESHOLD_CM - 1	最小周波数より少し高い値になる		
3	calcFrequency()	cm = 1	最大周波数になる		
4	calcFrequency()	cm = 0	0 になる		
5	readSensor()	最大測定距離付近	誤動作せず値が返る		

