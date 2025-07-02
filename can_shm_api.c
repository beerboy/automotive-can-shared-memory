#include "can_shm_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

// グローバル変数
static SharedMemoryLayout* g_shm_ptr = NULL;
static int g_shm_fd = -1;
static int g_is_initialized = 0;

// タイムスタンプ取得（ナノ秒）
static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// 共有メモリ初期化
CANShmResult can_shm_init(void) {
    if (g_is_initialized) {
        return CAN_SHM_SUCCESS;
    }
    
    // 共有メモリセグメント作成または開く
    g_shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (g_shm_fd == -1) {
        perror("shm_open");
        return CAN_SHM_ERROR_INIT_FAILED;
    }
    
    // ファイルサイズを確認して設定
    struct stat shm_stat;
    if (fstat(g_shm_fd, &shm_stat) == 0 && shm_stat.st_size < (off_t)sizeof(SharedMemoryLayout)) {
        if (ftruncate(g_shm_fd, sizeof(SharedMemoryLayout)) == -1) {
            perror("ftruncate");
            close(g_shm_fd);
            return CAN_SHM_ERROR_INIT_FAILED;
        }
    }
    
    // メモリマップ
    g_shm_ptr = (SharedMemoryLayout*)mmap(NULL, sizeof(SharedMemoryLayout),
                                         PROT_READ | PROT_WRITE, MAP_SHARED, g_shm_fd, 0);
    if (g_shm_ptr == MAP_FAILED) {
        perror("mmap");
        close(g_shm_fd);
        return CAN_SHM_ERROR_INIT_FAILED;
    }
    
    // 初期化チェック（マジックナンバー）
    if (g_shm_ptr->magic_number != MAGIC_NUMBER) {
        // 初回初期化
        memset(g_shm_ptr, 0, sizeof(SharedMemoryLayout));
        g_shm_ptr->magic_number = MAGIC_NUMBER;
        g_shm_ptr->version = 1;
        g_shm_ptr->global_sequence = 0;
        
        // グローバルミューテックス初期化
        pthread_mutexattr_t mutex_attr;
        pthread_mutexattr_init(&mutex_attr);
        pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&g_shm_ptr->global_mutex, &mutex_attr);
        pthread_mutexattr_destroy(&mutex_attr);
        
        // 条件変数初期化
        pthread_condattr_t cond_attr;
        pthread_condattr_init(&cond_attr);
        pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
        pthread_cond_init(&g_shm_ptr->update_condition, &cond_attr);
        pthread_condattr_destroy(&cond_attr);
        
        // 各バケットのミューテックス初期化
        pthread_mutexattr_t bucket_mutex_attr;
        pthread_mutexattr_init(&bucket_mutex_attr);
        pthread_mutexattr_setpshared(&bucket_mutex_attr, PTHREAD_PROCESS_SHARED);
        
        for (int i = 0; i < MAX_CAN_ENTRIES; i++) {
            pthread_mutex_init(&g_shm_ptr->buckets[i].mutex, &bucket_mutex_attr);
            g_shm_ptr->buckets[i].is_valid = 0;
        }
        
        pthread_mutexattr_destroy(&bucket_mutex_attr);
    }
    
    g_is_initialized = 1;
    return CAN_SHM_SUCCESS;
}

// 共有メモリ終了処理
CANShmResult can_shm_cleanup(void) {
    if (!g_is_initialized) {
        return CAN_SHM_SUCCESS;
    }
    
    if (g_shm_ptr != NULL) {
        munmap(g_shm_ptr, sizeof(SharedMemoryLayout));
        g_shm_ptr = NULL;
    }
    
    if (g_shm_fd != -1) {
        close(g_shm_fd);
        g_shm_fd = -1;
    }
    
    g_is_initialized = 0;
    return CAN_SHM_SUCCESS;
}

// Set関数実装
CANShmResult can_shm_set(uint32_t can_id, uint16_t dlc, const uint8_t* data) {
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
    
    // ハッシュ計算
    uint32_t bucket_index = can_id_hash(can_id);
    CANBucket* bucket = &g_shm_ptr->buckets[bucket_index];
    
    // バケットロック
    if (pthread_mutex_lock(&bucket->mutex) != 0) {
        return CAN_SHM_ERROR_MUTEX_FAILED;
    }
    
    // seqlock書き込み開始（奇数にする）
    uint32_t seq = bucket->can_data.sequence + 1;
    __atomic_store_n(&bucket->can_data.sequence, seq, __ATOMIC_RELEASE);
    
    // データ設定
    bucket->can_data.can_id = can_id;
    bucket->can_data.dlc = dlc;
    bucket->can_data.timestamp = get_timestamp_ns();
    
    if (dlc > 0 && data != NULL) {
        memcpy(bucket->can_data.data, data, dlc);
    }
    
    // パディング部分をクリア
    if (dlc < 64) {
        memset(&bucket->can_data.data[dlc], 0, 64 - dlc);
    }
    
    bucket->is_valid = 1;
    
    // seqlock書き込み完了（偶数にする）
    __atomic_store_n(&bucket->can_data.sequence, seq + 1, __ATOMIC_RELEASE);
    
    pthread_mutex_unlock(&bucket->mutex);
    
    // グローバル更新通知
    pthread_mutex_lock(&g_shm_ptr->global_mutex);
    g_shm_ptr->global_sequence++;
    g_shm_ptr->total_sets++;
    pthread_cond_broadcast(&g_shm_ptr->update_condition);
    pthread_mutex_unlock(&g_shm_ptr->global_mutex);
    
    return CAN_SHM_SUCCESS;
}

// Get関数実装
CANShmResult can_shm_get(uint32_t can_id, CANData* data_out) {
    if (!g_is_initialized) {
        return CAN_SHM_ERROR_INIT_FAILED;
    }
    
    if (!is_valid_can_id(can_id) || data_out == NULL) {
        return CAN_SHM_ERROR_INVALID_PARAM;
    }
    
    uint32_t bucket_index = can_id_hash(can_id);
    CANBucket* bucket = &g_shm_ptr->buckets[bucket_index];
    
    // データが有効かチェック
    if (!bucket->is_valid || bucket->can_data.can_id != can_id) {
        pthread_mutex_lock(&g_shm_ptr->global_mutex);
        g_shm_ptr->total_gets++;
        pthread_mutex_unlock(&g_shm_ptr->global_mutex);
        return CAN_SHM_ERROR_NOT_FOUND;
    }
    
    // seqlock読み取り（ロックフリー）
    uint32_t seq1, seq2;
    do {
        seq1 = __atomic_load_n(&bucket->can_data.sequence, __ATOMIC_ACQUIRE);
        if (seq1 & 1) continue; // 書き込み中
        
        // データコピー
        *data_out = bucket->can_data;
        
        seq2 = __atomic_load_n(&bucket->can_data.sequence, __ATOMIC_RELAXED);
    } while (seq1 != seq2);
    
    pthread_mutex_lock(&g_shm_ptr->global_mutex);
    g_shm_ptr->total_gets++;
    pthread_mutex_unlock(&g_shm_ptr->global_mutex);
    
    return CAN_SHM_SUCCESS;
}

// Subscribe関数実装
CANShmResult can_shm_subscribe(uint32_t can_id, 
                               uint32_t subscribe_count,
                               int32_t timeout_ms,
                               CANDataCallback callback,
                               void* user_data) {
    if (!g_is_initialized) {
        return CAN_SHM_ERROR_INIT_FAILED;
    }
    
    if (!is_valid_can_id(can_id) || callback == NULL) {
        return CAN_SHM_ERROR_INVALID_PARAM;
    }
    
    uint32_t bucket_index = can_id_hash(can_id);
    CANBucket* bucket = &g_shm_ptr->buckets[bucket_index];
    
    uint32_t received_count = 0;
    uint32_t last_sequence = 0;
    
    // 現在のシーケンス番号を取得
    if (bucket->is_valid && bucket->can_data.can_id == can_id) {
        last_sequence = bucket->can_data.sequence;
    }
    
    pthread_mutex_lock(&g_shm_ptr->global_mutex);
    g_shm_ptr->total_subscribes++;
    pthread_mutex_unlock(&g_shm_ptr->global_mutex);
    
    while (subscribe_count == 0 || received_count < subscribe_count) {
        pthread_mutex_lock(&g_shm_ptr->global_mutex);
        
        struct timespec timeout_spec;
        struct timespec* timeout_ptr = NULL;
        
        if (timeout_ms >= 0) {
            clock_gettime(CLOCK_REALTIME, &timeout_spec);
            timeout_spec.tv_sec += timeout_ms / 1000;
            timeout_spec.tv_nsec += (timeout_ms % 1000) * 1000000;
            if (timeout_spec.tv_nsec >= 1000000000) {
                timeout_spec.tv_sec++;
                timeout_spec.tv_nsec -= 1000000000;
            }
            timeout_ptr = &timeout_spec;
        }
        
        // 更新待ち
        int wait_result;
        if (timeout_ptr) {
            wait_result = pthread_cond_timedwait(&g_shm_ptr->update_condition, 
                                               &g_shm_ptr->global_mutex, timeout_ptr);
        } else {
            wait_result = pthread_cond_wait(&g_shm_ptr->update_condition, 
                                          &g_shm_ptr->global_mutex);
        }
        
        pthread_mutex_unlock(&g_shm_ptr->global_mutex);
        
        if (wait_result == ETIMEDOUT) {
            return CAN_SHM_ERROR_TIMEOUT;
        }
        
        // データチェック
        if (bucket->is_valid && bucket->can_data.can_id == can_id) {
            uint32_t current_sequence = bucket->can_data.sequence;
            if (current_sequence != last_sequence) {
                // 新しいデータを受信
                CANData data_copy = bucket->can_data;
                callback(can_id, &data_copy, user_data);
                received_count++;
                last_sequence = current_sequence;
            }
        }
    }
    
    return CAN_SHM_SUCCESS;
}

// Subscribe単発版用のヘルパー構造体
typedef struct {
    CANData* output;
    int received;
} OnceData;

// Subscribe単発版用のコールバック関数
static void once_callback(uint32_t id, const CANData* data, void* user_data) {
    (void)id; // 未使用パラメータを明示的に無視
    OnceData* od = (OnceData*)user_data;
    if (!od->received) {
        *(od->output) = *data;
        od->received = 1;
    }
}

// Subscribe単発版実装
CANShmResult can_shm_subscribe_once(uint32_t can_id,
                                    int32_t timeout_ms,
                                    CANData* data_out) {
    if (data_out == NULL) {
        return CAN_SHM_ERROR_INVALID_PARAM;
    }
    
    OnceData once_data = {data_out, 0};
    
    CANShmResult result = can_shm_subscribe(can_id, 1, timeout_ms, once_callback, &once_data);
    return result;
}

// 統計情報取得
CANShmResult can_shm_get_stats(uint64_t* total_sets,
                               uint64_t* total_gets,
                               uint64_t* total_subscribes) {
    if (!g_is_initialized) {
        return CAN_SHM_ERROR_INIT_FAILED;
    }
    
    if (total_sets == NULL || total_gets == NULL || total_subscribes == NULL) {
        return CAN_SHM_ERROR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_shm_ptr->global_mutex);
    *total_sets = g_shm_ptr->total_sets;
    *total_gets = g_shm_ptr->total_gets;
    *total_subscribes = g_shm_ptr->total_subscribes;
    pthread_mutex_unlock(&g_shm_ptr->global_mutex);
    
    return CAN_SHM_SUCCESS;
}

// デバッグ出力
void can_shm_debug_print(void) {
    if (!g_is_initialized) {
        printf("CAN Shared Memory: Not initialized\n");
        return;
    }
    
    printf("=== CAN Shared Memory Debug Info ===\n");
    printf("Magic: 0x%X, Version: %u\n", g_shm_ptr->magic_number, g_shm_ptr->version);
    printf("Global Sequence: %llu\n", g_shm_ptr->global_sequence);
    printf("Stats - Sets: %llu, Gets: %llu, Subscribes: %llu\n",
           g_shm_ptr->total_sets, g_shm_ptr->total_gets, g_shm_ptr->total_subscribes);
    
    int valid_entries = 0;
    for (int i = 0; i < MAX_CAN_ENTRIES; i++) {
        if (g_shm_ptr->buckets[i].is_valid) {
            valid_entries++;
        }
    }
    printf("Valid entries: %d / %d\n", valid_entries, MAX_CAN_ENTRIES);
}