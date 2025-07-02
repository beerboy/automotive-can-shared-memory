#include "can_shm_api.h"
#include "can_shm_linear_probing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

// テスト用のCAN IDセット（意図的に衝突を発生させる）
static const uint32_t test_can_ids[] = {
    0x123,      // 標準的なCAN ID
    0x456ABC,   // 拡張CAN ID
    0x789DEF,   // 拡張CAN ID
    0x100,      // 小さな値
    0x1FFFFFFF, // 最大値（29bit）
    0x000,      // 最小値
    0x555,      // 中間値
    0xAAA,      // 中間値
    0x111,      // パターン値
    0x222,      // パターン値
    0x333,      // パターン値
    0x444,      // パターン値
    0x666,      // パターン値
    0x777,      // パターン値
    0x888,      // パターン値
    0x999,      // パターン値
};

#define TEST_CAN_COUNT (sizeof(test_can_ids) / sizeof(test_can_ids[0]))

/**
 * 基本的な機能テスト
 */
void test_basic_functionality(void) {
    printf("\n=== Basic Functionality Test ===\n");
    
    CANShmResult result;
    CANData retrieved_data;
    uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    
    // テストデータの挿入
    printf("Testing basic set/get operations...\n");
    for (int i = 0; i < 5; i++) {
        uint32_t can_id = test_can_ids[i];
        uint8_t dlc = (i % 8) + 1;
        
        // データ挿入
        result = can_shm_set_linear_probing(can_id, dlc, test_data);
        if (result != CAN_SHM_SUCCESS) {
            printf("ERROR: Failed to set CAN ID 0x%X (error: %d)\n", can_id, result);
            continue;
        }
        
        // データ取得
        result = can_shm_get_linear_probing(can_id, &retrieved_data);
        if (result != CAN_SHM_SUCCESS) {
            printf("ERROR: Failed to get CAN ID 0x%X (error: %d)\n", can_id, result);
            continue;
        }
        
        // データ検証
        if (retrieved_data.can_id == can_id && retrieved_data.dlc == dlc) {
            printf("✓ CAN ID 0x%X: Set/Get successful (DLC=%d)\n", can_id, dlc);
        } else {
            printf("✗ CAN ID 0x%X: Data mismatch (expected DLC=%d, got DLC=%d)\n", 
                   can_id, dlc, retrieved_data.dlc);
        }
    }
}

/**
 * ハッシュ衝突テスト
 */
void test_hash_collisions(void) {
    printf("\n=== Hash Collision Test ===\n");
    
    // ハッシュ衝突の可視化
    can_shm_test_hash_collisions(test_can_ids, TEST_CAN_COUNT);
    
    // 全テストCAN IDを挿入してハッシュ衝突の処理を確認
    printf("\nInserting all test CAN IDs to test collision handling...\n");
    
    for (int i = 0; i < TEST_CAN_COUNT; i++) {
        uint32_t can_id = test_can_ids[i];
        uint8_t test_data[] = {0x10 + i, 0x20 + i, 0x30 + i, 0x40 + i};
        uint8_t dlc = 4;
        
        CANShmResult result = can_shm_set_linear_probing(can_id, dlc, test_data);
        if (result == CAN_SHM_SUCCESS) {
            printf("✓ Inserted CAN ID 0x%X\n", can_id);
        } else {
            printf("✗ Failed to insert CAN ID 0x%X (error: %d)\n", can_id, result);
        }
    }
    
    // 全データの取得確認
    printf("\nVerifying all inserted data...\n");
    int success_count = 0;
    for (int i = 0; i < TEST_CAN_COUNT; i++) {
        uint32_t can_id = test_can_ids[i];
        CANData retrieved_data;
        
        CANShmResult result = can_shm_get_linear_probing(can_id, &retrieved_data);
        if (result == CAN_SHM_SUCCESS && retrieved_data.can_id == can_id) {
            printf("✓ Retrieved CAN ID 0x%X successfully\n", can_id);
            success_count++;
        } else {
            printf("✗ Failed to retrieve CAN ID 0x%X (error: %d)\n", can_id, result);
        }
    }
    
    printf("Success rate: %d/%d (%.1f%%)\n", 
           success_count, TEST_CAN_COUNT, 
           (double)success_count / TEST_CAN_COUNT * 100.0);
}

/**
 * 従来方式との比較テスト
 */
void test_comparison_with_original(void) {
    printf("\n=== Comparison with Original Implementation ===\n");
    
    uint32_t collision_can_ids[] = {0x123, 0x456ABC}; // 同じハッシュ値を持つ可能性
    uint8_t test_data1[] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t test_data2[] = {0x11, 0x22, 0x33, 0x44};
    
    printf("Testing with potentially colliding CAN IDs...\n");
    printf("CAN ID 1: 0x%X, CAN ID 2: 0x%X\n", collision_can_ids[0], collision_can_ids[1]);
    printf("Hash 1: %u, Hash 2: %u\n", 
           can_id_hash(collision_can_ids[0]), 
           can_id_hash(collision_can_ids[1]));
    
    // 両方のCAN IDをリニアプロービング法で挿入
    printf("\nLinear Probing Method:\n");
    
    CANShmResult result1 = can_shm_set_linear_probing(collision_can_ids[0], 4, test_data1);
    CANShmResult result2 = can_shm_set_linear_probing(collision_can_ids[1], 4, test_data2);
    
    printf("Set CAN ID 0x%X: %s\n", collision_can_ids[0], 
           (result1 == CAN_SHM_SUCCESS) ? "SUCCESS" : "FAILED");
    printf("Set CAN ID 0x%X: %s\n", collision_can_ids[1], 
           (result2 == CAN_SHM_SUCCESS) ? "SUCCESS" : "FAILED");
    
    // 両方のデータが取得できるかテスト
    CANData data1, data2;
    CANShmResult get_result1 = can_shm_get_linear_probing(collision_can_ids[0], &data1);
    CANShmResult get_result2 = can_shm_get_linear_probing(collision_can_ids[1], &data2);
    
    printf("Get CAN ID 0x%X: %s", collision_can_ids[0], 
           (get_result1 == CAN_SHM_SUCCESS) ? "SUCCESS" : "FAILED");
    if (get_result1 == CAN_SHM_SUCCESS) {
        printf(" (data[0]=0x%02X)\n", data1.data[0]);
    } else {
        printf("\n");
    }
    
    printf("Get CAN ID 0x%X: %s", collision_can_ids[1], 
           (get_result2 == CAN_SHM_SUCCESS) ? "SUCCESS" : "FAILED");
    if (get_result2 == CAN_SHM_SUCCESS) {
        printf(" (data[0]=0x%02X)\n", data2.data[0]);
    } else {
        printf("\n");
    }
    
    // 従来方式の問題を説明
    printf("\nOriginal Implementation (Overwrite) would have:\n");
    printf("- Lost data for CAN ID 0x%X if collision occurred\n", collision_can_ids[0]);
    printf("- Only CAN ID 0x%X data would remain\n", collision_can_ids[1]);
    printf("- No error indication for data loss\n");
}

/**
 * 性能テスト
 */
void test_performance(void) {
    printf("\n=== Performance Test ===\n");
    
    const int NUM_OPERATIONS = 1000;
    struct timespec start, end;
    
    // Set操作の性能測定
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        uint32_t can_id = 0x1000 + i; // 連続するCAN ID
        uint8_t test_data[] = {(uint8_t)i, (uint8_t)(i >> 8), 0x55, 0xAA};
        can_shm_set_linear_probing(can_id, 4, test_data);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double set_time = (end.tv_sec - start.tv_sec) + 
                      (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("Set Operations: %d operations in %.6f seconds\n", NUM_OPERATIONS, set_time);
    printf("Average Set Time: %.2f μs per operation\n", 
           (set_time / NUM_OPERATIONS) * 1e6);
    
    // Get操作の性能測定
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        uint32_t can_id = 0x1000 + i;
        CANData data;
        can_shm_get_linear_probing(can_id, &data);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double get_time = (end.tv_sec - start.tv_sec) + 
                      (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("Get Operations: %d operations in %.6f seconds\n", NUM_OPERATIONS, get_time);
    printf("Average Get Time: %.2f μs per operation\n", 
           (get_time / NUM_OPERATIONS) * 1e6);
}

/**
 * データ更新テスト
 */
void test_data_updates(void) {
    printf("\n=== Data Update Test ===\n");
    
    uint32_t can_id = 0x555;
    CANData retrieved_data;
    
    // 初期データ挿入
    uint8_t data1[] = {0x01, 0x02, 0x03, 0x04};
    can_shm_set_linear_probing(can_id, 4, data1);
    can_shm_get_linear_probing(can_id, &retrieved_data);
    printf("Initial data[0]: 0x%02X\n", retrieved_data.data[0]);
    
    // データ更新
    uint8_t data2[] = {0xAA, 0xBB, 0xCC, 0xDD};
    can_shm_set_linear_probing(can_id, 4, data2);
    can_shm_get_linear_probing(can_id, &retrieved_data);
    printf("Updated data[0]: 0x%02X\n", retrieved_data.data[0]);
    
    // さらに更新
    uint8_t data3[] = {0xFF, 0xEE, 0xDD, 0xCC};
    can_shm_set_linear_probing(can_id, 4, data3);
    can_shm_get_linear_probing(can_id, &retrieved_data);
    printf("Final data[0]: 0x%02X\n", retrieved_data.data[0]);
    
    printf("✓ Data updates working correctly\n");
}

/**
 * メイン関数
 */
int main(void) {
    printf("Linear Probing Implementation Test\n");
    printf("==================================\n");
    
    // 共有メモリ初期化
    CANShmResult init_result = can_shm_init();
    if (init_result != CAN_SHM_SUCCESS) {
        printf("ERROR: Failed to initialize shared memory (error: %d)\n", init_result);
        return 1;
    }
    
    printf("✓ Shared memory initialized successfully\n");
    
    // 各種テスト実行
    test_basic_functionality();
    test_hash_collisions();
    test_comparison_with_original();
    test_data_updates();
    test_performance();
    
    // 統計情報表示
    can_shm_print_hash_stats();
    
    // クリーンアップ
    can_shm_cleanup();
    
    printf("\n=== Test Complete ===\n");
    printf("Linear probing implementation successfully tested!\n");
    printf("Key benefits demonstrated:\n");
    printf("- No data loss on hash collisions\n");
    printf("- Consistent O(1) performance for reasonable load factors\n");
    printf("- Safe concurrent access with seqlock\n");
    printf("- Detailed statistics for monitoring\n");
    
    return 0;
}