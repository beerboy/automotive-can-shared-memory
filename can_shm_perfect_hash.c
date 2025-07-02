#include "can_shm_perfect_hash.h"
#include "can_shm_api.h"
#include "can_shm_linear_probing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

// 外部変数（can_shm_api.cで定義）
extern SharedMemoryLayout* g_shm_ptr;
extern int g_is_initialized;

// 完全ハッシュ専用テーブル（デモ用）
static CANData g_perfect_hash_table[PERFECT_HASH_TABLE_SIZE];
static uint8_t g_perfect_hash_valid[PERFECT_HASH_TABLE_SIZE];
static pthread_mutex_t g_perfect_hash_mutexes[PERFECT_HASH_TABLE_SIZE];
static int g_perfect_hash_initialized = 0;

// 統計情報
typedef struct {
    uint64_t total_sets;
    uint64_t total_gets;
    uint64_t total_deletes;
    uint64_t total_access_time_ns;
    uint32_t current_entries;
} PerfectHashTableStats;

static PerfectHashTableStats g_perfect_stats = {0};

// タイムスタンプ取得（ナノ秒）
static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * 完全ハッシュテーブルの初期化
 */
static int init_perfect_hash_table(void) {
    if (g_perfect_hash_initialized) {
        return 1;
    }
    
    // テーブルクリア
    memset(g_perfect_hash_table, 0, sizeof(g_perfect_hash_table));
    memset(g_perfect_hash_valid, 0, sizeof(g_perfect_hash_valid));
    memset(&g_perfect_stats, 0, sizeof(g_perfect_stats));
    
    // ミューテックス初期化
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    
    for (int i = 0; i < PERFECT_HASH_TABLE_SIZE; i++) {
        if (pthread_mutex_init(&g_perfect_hash_mutexes[i], &mutex_attr) != 0) {
            // 初期化済みのミューテックスを破棄
            for (int j = 0; j < i; j++) {
                pthread_mutex_destroy(&g_perfect_hash_mutexes[j]);
            }
            pthread_mutexattr_destroy(&mutex_attr);
            return 0;
        }
    }
    
    pthread_mutexattr_destroy(&mutex_attr);
    g_perfect_hash_initialized = 1;
    
    printf("Perfect hash table initialized (size: %d)\n", PERFECT_HASH_TABLE_SIZE);
    return 1;
}

/**
 * 完全ハッシュ関数を使用したSet関数
 */
CANShmResult can_shm_set_perfect_hash(uint32_t can_id, uint16_t dlc, const uint8_t* data) {
    if (!g_is_initialized) {
        return CAN_SHM_ERROR_INIT_FAILED;
    }
    
    if (!init_perfect_hash_table()) {
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
    
    // 完全ハッシュ関数でインデックス計算
    uint32_t index = can_id_perfect_hash_demo(can_id);
    if (index == 0xFFFFFFFF || index >= PERFECT_HASH_TABLE_SIZE) {
        return CAN_SHM_ERROR_INVALID_ID;  // 事前定義されていないCAN ID
    }
    
    // 事前検証: 指定されたCAN IDが正しくマッピングされるかチェック
    if (!is_valid_can_id_for_perfect_hash_demo(can_id)) {
        return CAN_SHM_ERROR_INVALID_ID;
    }
    
    uint64_t start_time = get_timestamp_ns();
    
    // インデックス用ミューテックス取得
    if (pthread_mutex_lock(&g_perfect_hash_mutexes[index]) != 0) {
        return CAN_SHM_ERROR_MUTEX_FAILED;
    }
    
    // データ設定（seqlockは簡略化、必要に応じて追加可能）
    CANData* entry = &g_perfect_hash_table[index];
    
    entry->can_id = can_id;
    entry->dlc = dlc;
    entry->sequence++;  // 簡易的なバージョン管理
    
    if (dlc > 0 && data != NULL) {
        memcpy(entry->data, data, dlc);
    }
    entry->timestamp = get_timestamp_ns();
    
    // 新規エントリの場合
    if (!g_perfect_hash_valid[index]) {
        g_perfect_hash_valid[index] = 1;
        g_perfect_stats.current_entries++;
    }
    
    pthread_mutex_unlock(&g_perfect_hash_mutexes[index]);
    
    // 統計更新
    uint64_t end_time = get_timestamp_ns();
    g_perfect_stats.total_sets++;
    g_perfect_stats.total_access_time_ns += (end_time - start_time);
    
    return CAN_SHM_SUCCESS;
}

/**
 * 完全ハッシュ関数を使用したGet関数
 */
CANShmResult can_shm_get_perfect_hash(uint32_t can_id, CANData* data_out) {
    if (!g_is_initialized) {
        return CAN_SHM_ERROR_INIT_FAILED;
    }
    
    if (!init_perfect_hash_table()) {
        return CAN_SHM_ERROR_INIT_FAILED;
    }
    
    // パラメータ検証
    if (!is_valid_can_id(can_id)) {
        return CAN_SHM_ERROR_INVALID_ID;
    }
    
    if (data_out == NULL) {
        return CAN_SHM_ERROR_INVALID_PARAM;
    }
    
    // 完全ハッシュ関数でインデックス計算
    uint32_t index = can_id_perfect_hash_demo(can_id);
    if (index == 0xFFFFFFFF || index >= PERFECT_HASH_TABLE_SIZE) {
        return CAN_SHM_ERROR_INVALID_ID;
    }
    
    // 事前検証
    if (!is_valid_can_id_for_perfect_hash_demo(can_id)) {
        return CAN_SHM_ERROR_INVALID_ID;
    }
    
    uint64_t start_time = get_timestamp_ns();
    
    // データ存在チェック
    if (!g_perfect_hash_valid[index]) {
        return CAN_SHM_ERROR_NOT_FOUND;
    }
    
    // データ取得（ロックフリー読み取り、必要に応じてseqlock追加可能）
    *data_out = g_perfect_hash_table[index];
    
    // 整合性チェック
    if (data_out->can_id != can_id) {
        return CAN_SHM_ERROR_NOT_FOUND;  // 理論上発生しないはず
    }
    
    // 統計更新
    uint64_t end_time = get_timestamp_ns();
    g_perfect_stats.total_gets++;
    g_perfect_stats.total_access_time_ns += (end_time - start_time);
    
    return CAN_SHM_SUCCESS;
}

/**
 * 完全ハッシュ関数での削除
 */
CANShmResult can_shm_delete_perfect_hash(uint32_t can_id) {
    if (!g_is_initialized) {
        return CAN_SHM_ERROR_INIT_FAILED;
    }
    
    if (!init_perfect_hash_table()) {
        return CAN_SHM_ERROR_INIT_FAILED;
    }
    
    // パラメータ検証
    if (!is_valid_can_id(can_id)) {
        return CAN_SHM_ERROR_INVALID_ID;
    }
    
    // 完全ハッシュ関数でインデックス計算
    uint32_t index = can_id_perfect_hash_demo(can_id);
    if (index == 0xFFFFFFFF || index >= PERFECT_HASH_TABLE_SIZE) {
        return CAN_SHM_ERROR_INVALID_ID;
    }
    
    if (!is_valid_can_id_for_perfect_hash_demo(can_id)) {
        return CAN_SHM_ERROR_INVALID_ID;
    }
    
    uint64_t start_time = get_timestamp_ns();
    
    // ミューテックス取得
    if (pthread_mutex_lock(&g_perfect_hash_mutexes[index]) != 0) {
        return CAN_SHM_ERROR_MUTEX_FAILED;
    }
    
    // データ存在チェック
    if (!g_perfect_hash_valid[index]) {
        pthread_mutex_unlock(&g_perfect_hash_mutexes[index]);
        return CAN_SHM_ERROR_NOT_FOUND;
    }
    
    // 削除実行
    g_perfect_hash_valid[index] = 0;
    memset(&g_perfect_hash_table[index], 0, sizeof(CANData));
    g_perfect_stats.current_entries--;
    
    pthread_mutex_unlock(&g_perfect_hash_mutexes[index]);
    
    // 統計更新
    uint64_t end_time = get_timestamp_ns();
    g_perfect_stats.total_deletes++;
    g_perfect_stats.total_access_time_ns += (end_time - start_time);
    
    return CAN_SHM_SUCCESS;
}

/**
 * 完全ハッシュテーブルの統計情報を取得
 */
void can_shm_print_perfect_hash_stats(void) {
    printf("=== Perfect Hash Table Statistics ===\n");
    printf("Table Size: %d\n", PERFECT_HASH_TABLE_SIZE);
    printf("Current Entries: %u / %d\n", 
           g_perfect_stats.current_entries, PERFECT_HASH_TABLE_SIZE);
    printf("Load Factor: %.2f%%\n", 
           (double)g_perfect_stats.current_entries / PERFECT_HASH_TABLE_SIZE * 100.0);
    
    printf("Total Operations:\n");
    printf("  Set: %lu\n", g_perfect_stats.total_sets);
    printf("  Get: %lu\n", g_perfect_stats.total_gets);
    printf("  Delete: %lu\n", g_perfect_stats.total_deletes);
    
    uint64_t total_ops = g_perfect_stats.total_sets + 
                        g_perfect_stats.total_gets + 
                        g_perfect_stats.total_deletes;
    
    if (total_ops > 0) {
        printf("Average Access Time: %.2f ns\n", 
               (double)g_perfect_stats.total_access_time_ns / total_ops);
    }
    
    printf("Hash Collisions: 0 (Perfect Hash)\n");
    printf("Max Probe Distance: 1 (Always)\n");
    printf("====================================\n");
}

/**
 * 完全ハッシュ関数のテスト（全CAN ID検証）
 */
int can_shm_test_perfect_hash_function(void) {
    printf("\n=== Perfect Hash Function Test ===\n");
    
    // 内蔵テスト実行
    int result = test_perfect_hash_demo();
    
    if (result) {
        printf("✅ Perfect hash function test PASSED\n");
        printf("✅ All CAN IDs map to unique indices\n");
        printf("✅ No collisions detected\n");
    } else {
        printf("❌ Perfect hash function test FAILED\n");
        printf("❌ Collisions or invalid mappings detected\n");
    }
    
    printf("==================================\n");
    return result;
}

/**
 * 性能ベンチマーク実行
 */
void can_shm_benchmark_perfect_vs_linear(void) {
    const int NUM_OPERATIONS = 10000;
    const int NUM_WARM_UP = 1000;
    
    printf("\n=== Performance Benchmark: Perfect Hash vs Linear Probing ===\n");
    printf("Operations per test: %d (+ %d warm-up)\n", NUM_OPERATIONS, NUM_WARM_UP);
    
    // 初期化確認
    if (!init_perfect_hash_table()) {
        printf("Error: Failed to initialize perfect hash table\n");
        return;
    }
    
    struct timespec start, end;
    double perfect_set_time, perfect_get_time;
    double linear_set_time, linear_get_time;
    
    // ウォームアップ用のテストデータ準備
    uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};
    
    printf("\n--- Perfect Hash Performance ---\n");
    
    // Perfect Hash Set ベンチマーク
    printf("Benchmarking Perfect Hash Set operations...\n");
    
    // ウォームアップ
    for (int i = 0; i < NUM_WARM_UP; i++) {
        uint32_t can_id = DEMO_CAN_IDS[i % PERFECT_HASH_NUM_CAN_IDS];
        can_shm_set_perfect_hash(can_id, 4, test_data);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        uint32_t can_id = DEMO_CAN_IDS[i % PERFECT_HASH_NUM_CAN_IDS];
        can_shm_set_perfect_hash(can_id, 4, test_data);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    perfect_set_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    
    // Perfect Hash Get ベンチマーク
    printf("Benchmarking Perfect Hash Get operations...\n");
    
    // ウォームアップ
    CANData retrieved_data;
    for (int i = 0; i < NUM_WARM_UP; i++) {
        uint32_t can_id = DEMO_CAN_IDS[i % PERFECT_HASH_NUM_CAN_IDS];
        can_shm_get_perfect_hash(can_id, &retrieved_data);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        uint32_t can_id = DEMO_CAN_IDS[i % PERFECT_HASH_NUM_CAN_IDS];
        can_shm_get_perfect_hash(can_id, &retrieved_data);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    perfect_get_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("\n--- Linear Probing Performance ---\n");
    
    // Linear Probing Set ベンチマーク
    printf("Benchmarking Linear Probing Set operations...\n");
    
    // ウォームアップ
    for (int i = 0; i < NUM_WARM_UP; i++) {
        uint32_t can_id = DEMO_CAN_IDS[i % PERFECT_HASH_NUM_CAN_IDS];
        can_shm_set_linear_probing(can_id, 4, test_data);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        uint32_t can_id = DEMO_CAN_IDS[i % PERFECT_HASH_NUM_CAN_IDS];
        can_shm_set_linear_probing(can_id, 4, test_data);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    linear_set_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    
    // Linear Probing Get ベンチマーク
    printf("Benchmarking Linear Probing Get operations...\n");
    
    // ウォームアップ
    for (int i = 0; i < NUM_WARM_UP; i++) {
        uint32_t can_id = DEMO_CAN_IDS[i % PERFECT_HASH_NUM_CAN_IDS];
        can_shm_get_linear_probing(can_id, &retrieved_data);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        uint32_t can_id = DEMO_CAN_IDS[i % PERFECT_HASH_NUM_CAN_IDS];
        can_shm_get_linear_probing(can_id, &retrieved_data);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    linear_get_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    
    // 結果出力
    printf("\n=== Benchmark Results ===\n");
    printf("| Operation | Perfect Hash | Linear Probing | Ratio |\n");
    printf("|-----------|--------------|----------------|-------|\n");
    
    printf("| Set       | %8.2f μs  | %10.2f μs  | %.2fx |\n",
           (perfect_set_time / NUM_OPERATIONS) * 1e6,
           (linear_set_time / NUM_OPERATIONS) * 1e6,
           linear_set_time / perfect_set_time);
    
    printf("| Get       | %8.2f μs  | %10.2f μs  | %.2fx |\n",
           (perfect_get_time / NUM_OPERATIONS) * 1e6,
           (linear_get_time / NUM_OPERATIONS) * 1e6,
           linear_get_time / perfect_get_time);
    
    printf("\n=== Summary ===\n");
    if (perfect_set_time < linear_set_time) {
        printf("✅ Perfect Hash is %.1fx faster for Set operations\n", 
               linear_set_time / perfect_set_time);
    } else {
        printf("⚠️  Linear Probing is %.1fx faster for Set operations\n", 
               perfect_set_time / linear_set_time);
    }
    
    if (perfect_get_time < linear_get_time) {
        printf("✅ Perfect Hash is %.1fx faster for Get operations\n", 
               linear_get_time / perfect_get_time);
    } else {
        printf("⚠️  Linear Probing is %.1fx faster for Get operations\n", 
               perfect_get_time / linear_get_time);
    }
    
    printf("\nNote: Perfect Hash guarantees O(1) with zero collisions\n");
    printf("      Linear Probing performance depends on load factor\n");
    printf("============================================================\n");
}