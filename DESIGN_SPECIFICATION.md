# 車両CANデータ共有メモリシステム - 詳細設計仕様書

## 1. システム概要

### 1.1 目的
車両のCANデータを共有メモリに格納し、複数のアプリケーションプロセスが効率的にCANデータをGet/Subscribeできるシステムを提供する。

### 1.2 主要機能
- **Set機能**: CANデータを共有メモリに高速格納
- **Get機能**: CAN IDによる現在値の即座取得
- **Subscribe機能**: データ更新のリアルタイム通知・受信

### 1.3 設計目標
- **高性能**: マイクロ秒レベルの低遅延
- **高信頼性**: プロセス間データ競合の完全回避
- **スケーラビリティ**: 多数のCAN ID・プロセスに対応
- **保守性**: 明確なAPI・エラーハンドリング

## 2. アーキテクチャ設計

### 2.1 全体構成

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Process A     │    │   Process B     │    │   Process C     │
│   (CAN Driver)  │    │   (App 1)       │    │   (App 2)       │
├─────────────────┤    ├─────────────────┤    ├─────────────────┤
│   Set() API     │    │ Get/Subscribe() │    │ Get/Subscribe() │
└─────────┬───────┘    └─────────┬───────┘    └─────────┬───────┘
          │                      │                      │
          └──────────────────────┼──────────────────────┘
                                 │
                    ┌────────────▼──────────────┐
                    │    Shared Memory Layout   │
                    │  ┌─────────────────────┐  │
                    │  │ Global Header       │  │
                    │  ├─────────────────────┤  │
                    │  │ Hash Table (4096)   │  │
                    │  │ ┌─────────────────┐ │  │
                    │  │ │ Bucket[0]       │ │  │
                    │  │ │ ┌─────────────┐ │ │  │
                    │  │ │ │ Mutex       │ │ │  │
                    │  │ │ │ CANData     │ │ │  │
                    │  │ │ │ ValidFlag   │ │ │  │
                    │  │ │ └─────────────┘ │ │  │
                    │  │ └─────────────────┘ │  │
                    │  │        ...          │  │
                    │  │ Bucket[4095]        │  │
                    │  └─────────────────────┘  │
                    └───────────────────────────┘
```

### 2.2 メモリレイアウト設計

#### 2.2.1 SharedMemoryLayout構造体
```c
typedef struct {
    // === 管理情報セクション ===
    uint32_t magic_number;       // 0xCADDA7A (初期化確認)
    uint32_t version;            // バージョン番号 (1)
    uint64_t global_sequence;    // グローバル更新シーケンス
    
    // === 同期プリミティブ ===
    pthread_mutex_t global_mutex;      // グローバルミューテックス
    pthread_cond_t  update_condition;  // 更新通知用条件変数
    
    // === 統計情報 ===
    uint64_t total_sets;         // Set操作回数
    uint64_t total_gets;         // Get操作回数  
    uint64_t total_subscribes;   // Subscribe操作回数
    
    uint8_t padding[64];         // キャッシュライン境界調整
    
    // === データセクション ===
    CANBucket buckets[4096];     // ハッシュテーブル本体
} SharedMemoryLayout;
```

#### 2.2.2 CANBucket構造体 (ハッシュテーブルエントリ)
```c
typedef struct {
    pthread_mutex_t mutex;        // バケット専用ミューテックス
    CANData can_data;            // CANデータ本体
    uint8_t is_valid;            // データ有効フラグ
    uint8_t padding[7];          // 8バイトアライメント
} CANBucket;
```

#### 2.2.3 CANData構造体 (PDU形式)
```c
typedef struct {
    uint32_t sequence;    // seqlock用シーケンス番号
    uint32_t can_id;      // CAN ID (29bit有効)
    uint16_t dlc;         // データ長 (0~64)
    uint8_t  data[64];    // データ部
    uint64_t timestamp;   // 更新タイムスタンプ(ns)
} CANData;
```

### 2.3 ハッシュテーブル設計

#### 2.3.1 ハッシュ関数
```c
static inline uint32_t can_id_hash(uint32_t can_id) {
    can_id &= 0x1FFFFFFF;  // 29bit制限
    return (can_id ^ (can_id >> 16) ^ (can_id >> 8)) % 4096;
}
```

**数学的特性:**
- **入力範囲**: 29bit = 536,870,912 個の可能なCAN ID (0x0 〜 0x1FFFFFFF)
- **出力範囲**: 4096バケット (0 〜 4095)
- **圧縮比**: 約131,072:1 (536M ÷ 4096)
- **衝突確率**: 理論上、ランダムな50個のCAN IDで約50%の衝突発生

**特徴:**
- XOR演算による分散（完全ランダムではない）
- 各バケットに1エントリのみ格納（上書き方式）
- **重要**: 4096個を超えるCAN IDを使用すると確実に衝突が発生

#### 2.3.2 衝突解決と計算量

**現在の実装**:
- **バケット特定**: O(1) - ハッシュ関数による直接計算
- **データ照合**: O(1) - 1バケット内に1エントリのみ（上書き方式）
- **全体**: O(1) - ハッシュ衝突が発生しない限り

**制限事項と注意点**:
- **ハッシュ衝突**: 異なるCAN IDが同じバケットにマップされる可能性
- **データ上書き**: 衝突時は後から来たデータが前のデータを上書き
- **実際の容量**: 最大4096個の異なるCAN IDを**衝突なしで**格納可能
- **29bit空間**: 5億3千万個の可能なCAN ID → 4096バケットにハッシュ

**具体例**:
```
CAN ID 0x123     → hash() → バケット[0x456]
CAN ID 0x789ABC  → hash() → バケット[0x456]  ← 衝突！
結果: 0x123のデータが失われ、0x789ABCのデータのみ残る
```

## 2.4 現在の実装 vs std::unordered_map比較

### 2.4.1 std::unordered_mapの動的拡張機能

`std::unordered_map`の優秀な点：
```cpp
std::unordered_map<uint32_t, CANData> can_map;
// 自動的にバケット数を動的拡張
// load_factor > max_load_factor (通常1.0) でrehash
// バケット数を約2倍に拡張してデータを再配置
```

**動的拡張の仕組み**:
1. **負荷率監視**: `要素数/バケット数 > 1.0` で拡張トリガー
2. **リハッシュ**: 新しいサイズでハッシュテーブル再構築
3. **データ移行**: 全要素を新テーブルに再配置
4. **メモリ確保**: 動的にメモリ割り当て

### 2.4.2 現在の実装が固定サイズを選んだ理由

**共有メモリ制約**:
```c
// 共有メモリは起動時にサイズ固定
SharedMemoryLayout* shm = mmap(..., sizeof(SharedMemoryLayout), ...);
// 後からサイズ変更は非常に困難
```

**技術的制約**:
- **プロセス間共有**: 全プロセスが同じメモリマップを持つ必要
- **ポインタ無効**: 共有メモリ内でのポインタ使用は危険
- **アライメント**: 厳密なメモリレイアウト制御が必要
- **原子性**: リハッシュ中の他プロセスアクセス問題

### 2.4.3 動的拡張可能な代替設計案

**案1: セグメント分割**
```c
// 複数の共有メモリセグメントを使用
typedef struct {
    int segment_count;
    SharedMemorySegment segments[MAX_SEGMENTS];
} DynamicCANStorage;
```

**案2: 間接参照テーブル**
```c
// 固定テーブル + 動的データ領域
typedef struct {
    uint32_t data_offset;  // 共有メモリ内オフセット
    uint32_t can_id;
    uint16_t dlc;
} CANEntry;
```

**案3: C++実装への移行**
```cpp
// Boost.Interprocess使用
namespace bip = boost::interprocess;
typedef bip::allocator<std::pair<const uint32_t, CANData>, 
                       bip::managed_shared_memory::segment_manager> ShmemAllocator;
typedef bip::unordered_map<uint32_t, CANData, 
                           std::hash<uint32_t>, 
                           std::equal_to<uint32_t>, 
                           ShmemAllocator> ShmCANMap;
```

## 3. 同期・排他制御設計

### 3.1 階層化ロック戦略

```
レベル1: グローバルロック
├─ global_mutex (統計更新・条件変数)
└─ update_condition (Subscribe通知)

レベル2: バケットロック  
├─ buckets[0].mutex
├─ buckets[1].mutex
└─ ... (個別バケット保護)
```

### 3.2 Seqlock実装 (ロックフリー読み取り)

#### 3.2.1 書き込み処理 (Set関数)
```c
// バケットロック取得
pthread_mutex_lock(&bucket->mutex);

// seqlock書き込み開始 (奇数化)
uint32_t seq = bucket->can_data.sequence + 1;
__atomic_store_n(&bucket->can_data.sequence, seq, __ATOMIC_RELEASE);

// データ更新
bucket->can_data.can_id = can_id;
bucket->can_data.dlc = dlc;
memcpy(bucket->can_data.data, data, dlc);
bucket->can_data.timestamp = get_timestamp_ns();

// seqlock書き込み完了 (偶数化)
__atomic_store_n(&bucket->can_data.sequence, seq + 1, __ATOMIC_RELEASE);

pthread_mutex_unlock(&bucket->mutex);
```

#### 3.2.2 読み取り処理 (Get関数)
```c
uint32_t seq1, seq2;
do {
    // シーケンス番号読み取り
    seq1 = __atomic_load_n(&bucket->can_data.sequence, __ATOMIC_ACQUIRE);
    if (seq1 & 1) continue;  // 書き込み中はリトライ
    
    // データコピー
    *data_out = bucket->can_data;
    
    // シーケンス番号再確認
    seq2 = __atomic_load_n(&bucket->can_data.sequence, __ATOMIC_RELAXED);
} while (seq1 != seq2);  // 不整合時はリトライ
```

### 3.3 Subscribe通知メカニズム

#### 3.3.1 Publisher (Set関数)
```c
// データ更新後
pthread_mutex_lock(&g_shm_ptr->global_mutex);
g_shm_ptr->global_sequence++;
pthread_cond_broadcast(&g_shm_ptr->update_condition);  // 全Subscriber通知
pthread_mutex_unlock(&g_shm_ptr->global_mutex);
```

#### 3.3.2 Subscriber (Subscribe関数)
```c
while (received_count < subscribe_count) {
    pthread_mutex_lock(&g_shm_ptr->global_mutex);
    
    // タイムアウト付き待機
    int result = pthread_cond_timedwait(&g_shm_ptr->update_condition, 
                                       &g_shm_ptr->global_mutex, &timeout);
    
    pthread_mutex_unlock(&g_shm_ptr->global_mutex);
    
    if (result == ETIMEDOUT) return CAN_SHM_ERROR_TIMEOUT;
    
    // データ変更チェック・コールバック実行
    check_and_callback();
}
```

## 4. API仕様

### 4.1 初期化・終了API

#### 4.1.1 can_shm_init()
```c
CANShmResult can_shm_init(void);
```
**機能**: 共有メモリシステム初期化
**戻り値**: 
- `CAN_SHM_SUCCESS` (0): 成功
- `CAN_SHM_ERROR_INIT_FAILED` (-5): 初期化失敗

**内部処理**:
1. POSIX共有メモリセグメント作成 (`/can_data_shm`)
2. メモリマッピング (`mmap`)
3. 初回時: ミューテックス・条件変数初期化
4. マジックナンバー確認・設定

#### 4.1.2 can_shm_cleanup()
```c
CANShmResult can_shm_cleanup(void);
```
**機能**: 共有メモリシステム終了処理
**戻り値**: `CAN_SHM_SUCCESS`

### 4.2 データ操作API

#### 4.2.1 can_shm_set()
```c
CANShmResult can_shm_set(uint32_t can_id, uint16_t dlc, const uint8_t* data);
```
**機能**: CANデータを共有メモリに格納

**パラメータ**:
- `can_id`: CAN ID (29bit有効値, 0x0~0x1FFFFFFF)
- `dlc`: データ長 (0~64)
- `data`: データポインタ (dlc=0時はNULL可)

**戻り値**:
- `CAN_SHM_SUCCESS` (0): 成功
- `CAN_SHM_ERROR_INVALID_ID` (-1): 無効CAN ID
- `CAN_SHM_ERROR_INVALID_PARAM` (-4): 無効パラメータ
- `CAN_SHM_ERROR_MUTEX_FAILED` (-6): ロック失敗

**性能**: ~1μs (典型値)

#### 4.2.2 can_shm_get()
```c
CANShmResult can_shm_get(uint32_t can_id, CANData* data_out);
```
**機能**: CAN IDによるデータ取得

**パラメータ**:
- `can_id`: 取得対象CAN ID
- `data_out`: 取得データ格納先

**戻り値**:
- `CAN_SHM_SUCCESS` (0): 成功
- `CAN_SHM_ERROR_NOT_FOUND` (-2): データ未存在
- `CAN_SHM_ERROR_INVALID_PARAM` (-4): 無効パラメータ

**性能**: ~500ns (ロックフリー読み取り)

#### 4.2.3 can_shm_subscribe()
```c
CANShmResult can_shm_subscribe(uint32_t can_id, 
                               uint32_t subscribe_count,
                               int32_t timeout_ms,
                               CANDataCallback callback,
                               void* user_data);
```
**機能**: CAN IDの更新購読

**パラメータ**:
- `can_id`: 購読対象CAN ID
- `subscribe_count`: 購読回数 (0=無限)
- `timeout_ms`: タイムアウト時間[ms] (-1=無制限)
- `callback`: データ受信コールバック
- `user_data`: ユーザーデータ

**コールバック型**:
```c
typedef void (*CANDataCallback)(uint32_t can_id, const CANData* data, void* user_data);
```

**戻り値**:
- `CAN_SHM_SUCCESS` (0): 成功
- `CAN_SHM_ERROR_TIMEOUT` (-3): タイムアウト
- `CAN_SHM_ERROR_INVALID_PARAM` (-4): 無効パラメータ

### 4.3 補助API

#### 4.3.1 can_shm_subscribe_once()
```c
CANShmResult can_shm_subscribe_once(uint32_t can_id, int32_t timeout_ms, CANData* data_out);
```
**機能**: 単発データ更新待ち (Subscribe簡易版)

#### 4.3.2 can_shm_get_stats()
```c
CANShmResult can_shm_get_stats(uint64_t* total_sets, uint64_t* total_gets, uint64_t* total_subscribes);
```
**機能**: システム統計情報取得

#### 4.3.3 can_shm_debug_print()
```c
void can_shm_debug_print(void);
```
**機能**: デバッグ情報出力

## 5. エラーハンドリング

### 5.1 エラーコード体系
```c
typedef enum {
    CAN_SHM_SUCCESS = 0,              // 成功
    CAN_SHM_ERROR_INVALID_ID = -1,    // 無効CAN ID (>29bit)
    CAN_SHM_ERROR_NOT_FOUND = -2,     // データ未存在
    CAN_SHM_ERROR_TIMEOUT = -3,       // タイムアウト
    CAN_SHM_ERROR_INVALID_PARAM = -4, // 無効パラメータ
    CAN_SHM_ERROR_INIT_FAILED = -5,   // 初期化失敗
    CAN_SHM_ERROR_MUTEX_FAILED = -6   // ミューテックス失敗
} CANShmResult;
```

### 5.2 エラー処理方針
- **入力検証**: 全APIでパラメータ事前チェック
- **初期化確認**: 各API呼び出し時に初期化状態確認
- **ミューテックス失敗**: 致命的エラーとして即座に復帰
- **タイムアウト**: Subscribe系のみ、アプリ判断に委ねる

## 6. 性能特性 (推定値)

### 6.1 レイテンシ特性 (理論推定値)

| 操作 | 推定典型値 | 推定最大値 | 備考 |
|------|----------|----------|------|
| Set | 1μs | 10μs | ミューテックス競合時 |
| Get | 500ns | 2μs | seqlockリトライ時 |
| Subscribe通知 | 5μs | 50μs | プロセス数依存 |

**注**: これらは実装設計に基づく理論推定値です。実測には専用のベンチマークツールが必要です。

### 6.2 スループット特性

| 項目 | 性能値 | 条件 |
|------|-------|-----|
| Set ops/sec | 100万 | 単一プロセス |
| Get ops/sec | 500万 | 並行読み取り |
| CAN ID数 | 4096 | ハッシュ衝突なし |
| 同時プロセス数 | 64 | Subscribe推奨上限 |

### 6.3 メモリ使用量

| 項目 | サイズ | 備考 |
|------|-------|-----|
| 共有メモリ総容量 | ~1.2MB | 4096バケット |
| CANData単体 | 88 byte | タイムスタンプ含む |
| CANBucket単体 | ~128 byte | ミューテックス含む |
| 管理情報 | ~200 byte | ヘッダー・統計 |

## 7. 制限事項・注意点

### 7.1 システム制限
- **CAN ID範囲**: 0x0~0x1FFFFFFF (29bit)
- **DLC上限**: 64 byte
- **最大CAN ID数**: 4096 (ハッシュテーブルサイズ)
- **プロセス数**: 理論上無制限、実用的には64プロセス推奨

### 7.2 プラットフォーム依存
- **Linux専用**: POSIX共有メモリ・pthread使用
- **アーキテクチャ**: little-endian想定
- **アライメント**: 8バイト境界必須

### 7.3 運用上の注意
- **共有メモリ永続化**: プロセス異常終了時の手動削除必要
- **初期化順序**: 最初のプロセスが初期化を完了してから他プロセス起動
- **メモリリーク**: 必ず`can_shm_cleanup()`呼び出し

## 8. 拡張可能性

### 8.1 将来拡張候補

**高優先度**:
- **動的リハッシュ**: `std::unordered_map`風の自動拡張機能
- **チェイン法**: 衝突時のデータロス解決
- **Boost.Interprocess移行**: C++による高機能化

**中優先度**:  
- **複数セグメント**: CAN ID範囲分割による性能向上
- **設定ファイル**: テーブルサイズの実行時調整
- **圧縮格納**: データ部圧縮による容量削減

**低優先度**:
- **永続化**: 起動時データ復元機能
- **ネットワーク対応**: 分散システム化

### 8.2 移植性考慮
- **Windows対応**: Named Pipe・CRITICAL_SECTION使用
- **組込み対応**: RTOSマルチタスク環境
- **ネットワーク対応**: TCP/UDP経由での分散システム

## 9. テスト仕様

### 9.1 単体テスト (18項目)
- **Set機能**: 正常系4、異常系2
- **Get機能**: 正常系3、異常系1  
- **Subscribe機能**: 正常系2、異常系1
- **統計・その他**: 5項目

### 9.2 性能テスト
- **負荷テスト**: 100万ops/sec継続動作
- **同時実行**: 32プロセス並行アクセス
- **メモリリーク**: 24時間連続稼働

### 9.3 信頼性テスト
- **プロセス異常**: 強制終了時の他プロセス影響
- **メモリ破損**: valgrind検証
- **競合状態**: Thread Sanitizer検証

---

**文書バージョン**: 1.0  
**作成日**: 2025-07-02  
**対象実装**: can_shm_api v1.0