# CANデータ共有メモリシステム テストケース設計

## 基本データ構造
- CAN ID: 4 byte (uint32_t)
- DLC: 2 byte (uint16_t) 
- データ部: 0~64 byte

## Set関数のテストケース

### TC-SET-001: 正常データ設定
- 入力: CAN ID=0x123, DLC=8, データ=[0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08]
- 期待結果: 戻り値=成功(0)

### TC-SET-002: DLC最大値
- 入力: CAN ID=0x456, DLC=64, データ=64バイト分
- 期待結果: 戻り値=成功(0)

### TC-SET-003: DLC=0
- 入力: CAN ID=0x789, DLC=0, データ=NULL
- 期待結果: 戻り値=成功(0)

### TC-SET-004: 同一CAN IDの上書き
- 入力1: CAN ID=0x100, DLC=4, データ=[0xAA,0xBB,0xCC,0xDD]
- 入力2: CAN ID=0x100, DLC=8, データ=[0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88]
- 期待結果: 最新データで上書きされる

## Get関数のテストケース

### TC-GET-001: 存在するCAN IDの取得
- 前提: Set(CAN ID=0x200, DLC=4, データ=[0x01,0x02,0x03,0x04])済み
- 入力: CAN ID=0x200
- 期待結果: DLC=4, データ=[0x01,0x02,0x03,0x04], 戻り値=成功(0)

### TC-GET-002: 存在しないCAN IDの取得
- 入力: CAN ID=0x999 (未設定)
- 期待結果: 戻り値=エラー(-1)

### TC-GET-003: DLC=0のデータ取得
- 前提: Set(CAN ID=0x300, DLC=0)済み
- 入力: CAN ID=0x300
- 期待結果: DLC=0, 戻り値=成功(0)

## Subscribe関数のテストケース

### TC-SUB-001: 単発購読
- 入力: CAN ID=0x400, 購読回数=1, タイムアウト=1000ms
- 動作: 別スレッドでSet(CAN ID=0x400, ...)を実行
- 期待結果: データを1回受信して正常終了

### TC-SUB-002: 複数回購読
- 入力: CAN ID=0x500, 購読回数=3, タイムアウト=2000ms
- 動作: 別スレッドでSet(CAN ID=0x500, ...)を3回実行
- 期待結果: データを3回受信して正常終了

### TC-SUB-003: 無限購読
- 入力: CAN ID=0x600, 購読回数=0, タイムアウト=500ms
- 動作: 別スレッドでSet(CAN ID=0x600, ...)を実行後、タイムアウト待ち
- 期待結果: タイムアウトエラーで終了

### TC-SUB-004: タイムアウト無効
- 入力: CAN ID=0x700, 購読回数=1, タイムアウト=-1
- 動作: 1秒後に別スレッドでSet(CAN ID=0x700, ...)を実行
- 期待結果: タイムアウトせずデータを受信

### TC-SUB-005: タイムアウト発生
- 入力: CAN ID=0x800, 購読回数=1, タイムアウト=100ms
- 動作: データ更新を行わない
- 期待結果: タイムアウトエラーで終了

## マルチプロセステスト

### TC-MULTI-001: 同時Get
- 動作: 複数プロセスから同一CAN IDを同時にGet
- 期待結果: データ競合なく正常取得

### TC-MULTI-002: Set中のGet
- 動作: Set実行中に別プロセスからGet実行
- 期待結果: 一貫性のあるデータを取得

### TC-MULTI-003: 複数Subscribe
- 動作: 複数プロセスから同一CAN IDをSubscribe
- 期待結果: 全プロセスがデータ更新を受信