#ifndef CAN_SHM_TYPES_H
#define CAN_SHM_TYPES_H

#include <stdint.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// CAN ID最大値（29bit）
#define CAN_ID_MAX 0x1FFFFFFF

// CANデータのPDU構造体
typedef struct {
    uint32_t sequence;    // 更新シーケンス番号（整合性チェック用）
    uint32_t can_id;      // CAN ID (4 byte, 29bit有効値)
    uint16_t dlc;         // データ長 (2 byte)
    uint8_t  data[64];    // データ部 (0~64 byte)
    uint64_t timestamp;   // 更新タイムスタンプ（nanoseconds）
} __attribute__((packed)) CANData;

// ハッシュテーブルのバケット
typedef struct {
    pthread_mutex_t mutex;        // プロセス間共有ミューテックス
    CANData can_data;            // CANデータ本体
    uint8_t is_valid;            // データ有効フラグ
    uint8_t padding[7];          // アライメント調整
} __attribute__((aligned(8))) CANBucket;

// 共有メモリ全体のレイアウト
#define MAX_CAN_ENTRIES 4096     // ハッシュテーブルサイズ
#define SHM_NAME "/can_data_shm"
#define MAGIC_NUMBER 0xCADDA7A  // マジックナンバー

typedef struct {
    // 管理情報
    uint32_t magic_number;       // マジックナンバー（初期化確認用）
    uint32_t version;            // バージョン番号
    uint64_t global_sequence;    // グローバル更新シーケンス
    
    // 通知用
    pthread_mutex_t global_mutex;      // グローバルミューテックス
    pthread_cond_t  update_condition;  // 更新通知用条件変数
    
    // 統計情報
    uint64_t total_sets;         // Set操作回数
    uint64_t total_gets;         // Get操作回数
    uint64_t total_subscribes;   // Subscribe操作回数
    
    uint8_t padding[64];         // キャッシュライン境界調整
    
    // ハッシュテーブル
    CANBucket buckets[MAX_CAN_ENTRIES];
} __attribute__((aligned(64))) SharedMemoryLayout;

// エラーコード
typedef enum {
    CAN_SHM_SUCCESS = 0,
    CAN_SHM_ERROR_INVALID_ID = -1,
    CAN_SHM_ERROR_NOT_FOUND = -2,
    CAN_SHM_ERROR_TIMEOUT = -3,
    CAN_SHM_ERROR_INVALID_PARAM = -4,
    CAN_SHM_ERROR_INIT_FAILED = -5,
    CAN_SHM_ERROR_MUTEX_FAILED = -6
} CANShmResult;

// Subscribe用のコールバック関数型
typedef void (*CANDataCallback)(uint32_t can_id, const CANData* data, void* user_data);

// ハッシュ関数（CAN IDからバケットインデックスを計算）
static inline uint32_t can_id_hash(uint32_t can_id) {
    // CAN IDの29bit制約チェック
    can_id &= CAN_ID_MAX;
    // Simple hash function - can be improved for better distribution
    return (can_id ^ (can_id >> 16) ^ (can_id >> 8)) % MAX_CAN_ENTRIES;
}

// CAN ID有効性チェック
static inline int is_valid_can_id(uint32_t can_id) {
    return can_id <= CAN_ID_MAX;
}

#ifdef __cplusplus
}
#endif

#endif // CAN_SHM_TYPES_H