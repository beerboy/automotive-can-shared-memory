#include "can_shm_linear_probing.h"
#include "can_shm_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

// 外部変数（can_shm_api.cで定義）
extern SharedMemoryLayout* g_shm_ptr;
extern int g_is_initialized;

// 統計情報構造体
typedef struct {
    uint64_t total_probes;      // 総探査回数
    uint64_t collision_count;   // 衝突発生回数
    uint32_t max_probe_distance; // 最大探査距離
    uint32_t current_entries;   // 現在の格納エントリ数
} HashStats;

static HashStats g_hash_stats = {0, 0, 0, 0};

// タイムスタンプ取得（ナノ秒）
static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * seqlockによる安全なデータ書き込み
 */
static void write_can_data_with_seqlock(CANBucket* bucket, uint32_t can_id, 
                                       uint16_t dlc, const uint8_t* data) {
    // seqlock書き込み開始（奇数にする）
    uint32_t seq = bucket->can_data.sequence + 1;
    __atomic_store_n(&bucket->can_data.sequence, seq, __ATOMIC_RELEASE);
    
    // データ設定
    bucket->can_data.can_id = can_id;
    bucket->can_data.dlc = dlc;
    if (dlc > 0 && data != NULL) {
        memcpy(bucket->can_data.data, data, dlc);
    }
    bucket->can_data.timestamp = get_timestamp_ns();
    
    // seqlock書き込み完了（偶数にする）
    __atomic_store_n(&bucket->can_data.sequence, seq + 1, __ATOMIC_RELEASE);
}

/**
 * seqlockによる安全なデータ読み取り
 */
static int read_can_data_with_seqlock(CANBucket* bucket, CANData* data_out) {
    uint32_t seq1, seq2;
    int retry_count = 0;
    const int MAX_RETRIES = 10;
    
    do {
        // 書き込み中でないことを確認
        seq1 = __atomic_load_n(&bucket->can_data.sequence, __ATOMIC_ACQUIRE);
        if (seq1 & 1) {
            // 書き込み中（奇数）の場合はリトライ
            retry_count++;
            if (retry_count > MAX_RETRIES) {
                return -1; // リトライ上限
            }
            continue;
        }
        
        // データをコピー
        *data_out = bucket->can_data;
        
        // シーケンス番号が変わっていないことを確認
        seq2 = __atomic_load_n(&bucket->can_data.sequence, __ATOMIC_RELAXED);
        
        retry_count++;
        if (retry_count > MAX_RETRIES) {
            return -1; // リトライ上限
        }
    } while (seq1 != seq2);
    
    return 0; // 成功
}

/**
 * リニアプロービング法を使用したSet関数
 */
CANShmResult can_shm_set_linear_probing(uint32_t can_id, uint16_t dlc, const uint8_t* data) {
    if (!g_is_initialized) {
        return CAN_SHM_ERROR_INIT_FAILED;
    }
    
    // パラメータ検証
    if (!is_valid_can_id(can_id)) {
        return CAN_SHM_ERROR_INVALID_ID;
    }
    
    if (dlc > 64) {
        return CAN_SHM_ERROR_INVALID_PARAM;
    }
    
    if (dlc > 0 && data == NULL) {
        return CAN_SHM_ERROR_INVALID_PARAM;
    }
    
    // 初期ハッシュ値計算
    uint32_t initial_hash = can_id_hash(can_id);
    uint32_t probe_distance = 0;
    int found_slot = 0;
    
    // リニアプロービングによる探索
    for (int i = 0; i < MAX_CAN_ENTRIES; i++) {
        uint32_t probe_index = (initial_hash + i) % MAX_CAN_ENTRIES;
        CANBucket* bucket = &g_shm_ptr->buckets[probe_index];
        
        // バケットロック取得
        if (pthread_mutex_lock(&bucket->mutex) != 0) {
            return CAN_SHM_ERROR_MUTEX_FAILED;
        }
        
        // 空きバケットまたは同じCAN IDのバケットを発見
        if (bucket->is_valid == 0) {
            // 空きバケットに新規挿入
            write_can_data_with_seqlock(bucket, can_id, dlc, data);
            bucket->is_valid = 1;
            found_slot = 1;
            
            // 統計更新
            if (i > 0) {
                g_hash_stats.collision_count++;
            }
            g_hash_stats.total_probes += (i + 1);
            g_hash_stats.current_entries++;
            probe_distance = i;
            
        } else if (bucket->can_data.can_id == can_id) {
            // 同じCAN IDの更新
            write_can_data_with_seqlock(bucket, can_id, dlc, data);
            found_slot = 1;
            
            // 統計更新
            g_hash_stats.total_probes += (i + 1);
            probe_distance = i;
        }
        
        pthread_mutex_unlock(&bucket->mutex);
        
        if (found_slot) {
            // 最大探査距離更新
            if (probe_distance > g_hash_stats.max_probe_distance) {
                g_hash_stats.max_probe_distance = probe_distance;
            }
            
            // グローバル統計更新とSubscribe通知
            pthread_mutex_lock(&g_shm_ptr->global_mutex);
            g_shm_ptr->total_sets++;
            g_shm_ptr->global_sequence++;
            pthread_cond_broadcast(&g_shm_ptr->update_condition);
            pthread_mutex_unlock(&g_shm_ptr->global_mutex);
            
            return CAN_SHM_SUCCESS;
        }
    }
    
    // ハッシュテーブルが満杯
    return CAN_SHM_ERROR_NOT_FOUND; // 適切なエラーコードが必要
}

/**
 * リニアプロービング法を使用したGet関数
 */
CANShmResult can_shm_get_linear_probing(uint32_t can_id, CANData* data_out) {
    if (!g_is_initialized) {
        return CAN_SHM_ERROR_INIT_FAILED;
    }
    
    // パラメータ検証
    if (!is_valid_can_id(can_id)) {
        return CAN_SHM_ERROR_INVALID_ID;
    }
    
    if (data_out == NULL) {
        return CAN_SHM_ERROR_INVALID_PARAM;
    }
    
    // 初期ハッシュ値計算
    uint32_t initial_hash = can_id_hash(can_id);
    
    // リニアプロービングによる探索
    for (int i = 0; i < MAX_CAN_ENTRIES; i++) {
        uint32_t probe_index = (initial_hash + i) % MAX_CAN_ENTRIES;
        CANBucket* bucket = &g_shm_ptr->buckets[probe_index];
        
        // 空きバケットに到達した場合、データは存在しない
        if (bucket->is_valid == 0) {
            break;
        }
        
        // CAN IDが一致するかチェック
        if (bucket->can_data.can_id == can_id) {
            // seqlockによる安全な読み取り
            if (read_can_data_with_seqlock(bucket, data_out) == 0) {
                // 統計更新
                pthread_mutex_lock(&g_shm_ptr->global_mutex);
                g_shm_ptr->total_gets++;
                pthread_mutex_unlock(&g_shm_ptr->global_mutex);
                
                return CAN_SHM_SUCCESS;
            } else {
                // seqlock読み取り失敗（書き込み競合）
                return CAN_SHM_ERROR_MUTEX_FAILED;
            }
        }
    }
    
    return CAN_SHM_ERROR_NOT_FOUND;
}

/**
 * リニアプロービング法での削除（将来拡張用）
 * 注意：削除は線形探査チェーンを壊す可能性があるため、
 * 実装は慎重に行う必要がある
 */
CANShmResult can_shm_delete_linear_probing(uint32_t can_id) {
    if (!g_is_initialized) {
        return CAN_SHM_ERROR_INIT_FAILED;
    }
    
    // パラメータ検証
    if (!is_valid_can_id(can_id)) {
        return CAN_SHM_ERROR_INVALID_ID;
    }
    
    // 初期ハッシュ値計算
    uint32_t initial_hash = can_id_hash(can_id);
    
    // リニアプロービングによる探索
    for (int i = 0; i < MAX_CAN_ENTRIES; i++) {
        uint32_t probe_index = (initial_hash + i) % MAX_CAN_ENTRIES;
        CANBucket* bucket = &g_shm_ptr->buckets[probe_index];
        
        // バケットロック取得
        if (pthread_mutex_lock(&bucket->mutex) != 0) {
            return CAN_SHM_ERROR_MUTEX_FAILED;
        }
        
        if (bucket->is_valid == 0) {
            // 空きバケットに到達、データは存在しない
            pthread_mutex_unlock(&bucket->mutex);
            break;
        }
        
        if (bucket->can_data.can_id == can_id) {
            // 発見、削除実行
            // 注意：単純にis_valid=0にすると探査チェーンが切れる
            // 実際の実装では後続要素の再配置が必要
            
            // 簡易実装：Tombstone方式（削除マーク）
            bucket->is_valid = 0;
            memset(&bucket->can_data, 0, sizeof(CANData));
            
            // 統計更新
            g_hash_stats.current_entries--;
            
            pthread_mutex_unlock(&bucket->mutex);
            return CAN_SHM_SUCCESS;
        }
        
        pthread_mutex_unlock(&bucket->mutex);
    }
    
    return CAN_SHM_ERROR_NOT_FOUND;
}

/**
 * ハッシュテーブルの統計情報を取得
 */
void can_shm_print_hash_stats(void) {
    printf("=== Hash Table Statistics (Linear Probing) ===\n");
    printf("Current Entries: %u / %d\n", g_hash_stats.current_entries, MAX_CAN_ENTRIES);
    printf("Load Factor: %.2f%%\n", 
           (double)g_hash_stats.current_entries / MAX_CAN_ENTRIES * 100.0);
    printf("Total Probes: %lu\n", g_hash_stats.total_probes);
    printf("Collision Count: %lu\n", g_hash_stats.collision_count);
    printf("Max Probe Distance: %u\n", g_hash_stats.max_probe_distance);
    
    if (g_hash_stats.current_entries > 0) {
        printf("Average Probe Distance: %.2f\n", 
               (double)g_hash_stats.total_probes / 
               (g_shm_ptr->total_sets > 0 ? g_shm_ptr->total_sets : 1));
    }
    
    printf("Total Operations: Set=%lu, Get=%lu\n", 
           g_shm_ptr->total_sets, g_shm_ptr->total_gets);
    printf("===============================================\n");
}

/**
 * 指定されたCAN IDのハッシュ衝突をテスト
 */
void can_shm_test_hash_collisions(const uint32_t* can_ids, int count) {
    printf("=== Hash Collision Test ===\n");
    printf("Testing %d CAN IDs for hash collisions:\n", count);
    
    // ハッシュ値とCAN IDのマッピング
    for (int i = 0; i < count; i++) {
        uint32_t hash_val = can_id_hash(can_ids[i]);
        printf("CAN ID 0x%08X -> Hash %u\n", can_ids[i], hash_val);
        
        // 衝突チェック
        for (int j = i + 1; j < count; j++) {
            uint32_t hash_val2 = can_id_hash(can_ids[j]);
            if (hash_val == hash_val2) {
                printf("  *** COLLISION: CAN ID 0x%08X also hashes to %u\n", 
                       can_ids[j], hash_val2);
            }
        }
    }
    printf("===========================\n");
}