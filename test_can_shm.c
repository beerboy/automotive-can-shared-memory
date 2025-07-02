#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#include "can_shm_api.h"

// テスト結果統計
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// テストマクロ
#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (condition) { \
        tests_passed++; \
        printf("PASS: %s\n", message); \
    } else { \
        tests_failed++; \
        printf("FAIL: %s\n", message); \
    } \
} while(0)

// Subscribe用のテストデータ
typedef struct {
    uint32_t can_id;
    uint32_t subscribe_count;
    int32_t timeout_ms;
    int received_count;
    CANData received_data[10];
    pthread_mutex_t mutex;
} SubscribeTestData;

// Subscribe用コールバック関数
void subscribe_callback(uint32_t can_id, const CANData* data, void* user_data) {
    SubscribeTestData* test_data = (SubscribeTestData*)user_data;
    pthread_mutex_lock(&test_data->mutex);
    
    if (test_data->received_count < 10) {
        test_data->received_data[test_data->received_count] = *data;
        test_data->received_count++;
    }
    
    pthread_mutex_unlock(&test_data->mutex);
}

// Set用のスレッド関数
void* set_thread_func(void* arg) {
    SubscribeTestData* test_data = (SubscribeTestData*)arg;
    
    // 少し待ってからデータを送信
    usleep(100000); // 100ms
    
    uint8_t data[] = {0x11, 0x22, 0x33, 0x44};
    can_shm_set(test_data->can_id, 4, data);
    
    return NULL;
}

// TC-SET-001: 正常データ設定
void test_set_normal_data() {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    CANShmResult result = can_shm_set(0x123, 8, data);
    printf("DEBUG: Set result = %d\n", result);
    TEST_ASSERT(result == CAN_SHM_SUCCESS, "TC-SET-001: Set normal data");
}

// TC-SET-002: DLC最大値
void test_set_max_dlc() {
    uint8_t data[64];
    for (int i = 0; i < 64; i++) {
        data[i] = (uint8_t)(i & 0xFF);
    }
    CANShmResult result = can_shm_set(0x456, 64, data);
    TEST_ASSERT(result == CAN_SHM_SUCCESS, "TC-SET-002: Set max DLC");
}

// TC-SET-003: DLC=0
void test_set_dlc_zero() {
    CANShmResult result = can_shm_set(0x789, 0, NULL);
    TEST_ASSERT(result == CAN_SHM_SUCCESS, "TC-SET-003: Set DLC=0");
}

// TC-SET-004: 同一CAN IDの上書き
void test_set_overwrite() {
    uint8_t data1[] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t data2[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    
    can_shm_set(0x100, 4, data1);
    CANShmResult result = can_shm_set(0x100, 8, data2);
    TEST_ASSERT(result == CAN_SHM_SUCCESS, "TC-SET-004: Overwrite same CAN ID");
    
    // 上書きされたことを確認
    CANData retrieved;
    can_shm_get(0x100, &retrieved);
    TEST_ASSERT(retrieved.dlc == 8 && memcmp(retrieved.data, data2, 8) == 0, 
                "TC-SET-004: Data was overwritten correctly");
}

// TC-GET-001: 存在するCAN IDの取得
void test_get_existing_id() {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    can_shm_set(0x200, 4, data);
    
    CANData retrieved;
    CANShmResult result = can_shm_get(0x200, &retrieved);
    
    TEST_ASSERT(result == CAN_SHM_SUCCESS, "TC-GET-001: Get existing CAN ID (result)");
    TEST_ASSERT(retrieved.can_id == 0x200, "TC-GET-001: Get existing CAN ID (CAN ID)");
    TEST_ASSERT(retrieved.dlc == 4, "TC-GET-001: Get existing CAN ID (DLC)");
    TEST_ASSERT(memcmp(retrieved.data, data, 4) == 0, "TC-GET-001: Get existing CAN ID (data)");
}

// TC-GET-002: 存在しないCAN IDの取得
void test_get_nonexistent_id() {
    CANData retrieved;
    CANShmResult result = can_shm_get(0x999, &retrieved);
    TEST_ASSERT(result == CAN_SHM_ERROR_NOT_FOUND, "TC-GET-002: Get non-existent CAN ID");
}

// TC-GET-003: DLC=0のデータ取得
void test_get_dlc_zero() {
    can_shm_set(0x300, 0, NULL);
    
    CANData retrieved;
    CANShmResult result = can_shm_get(0x300, &retrieved);
    
    TEST_ASSERT(result == CAN_SHM_SUCCESS, "TC-GET-003: Get DLC=0 data (result)");
    TEST_ASSERT(retrieved.dlc == 0, "TC-GET-003: Get DLC=0 data (DLC)");
}

// TC-SUB-001: 単発購読
void test_subscribe_once() {
    SubscribeTestData test_data = {0};
    test_data.can_id = 0x400;
    test_data.subscribe_count = 1;
    test_data.timeout_ms = 1000;
    pthread_mutex_init(&test_data.mutex, NULL);
    
    // 別スレッドでデータを送信
    pthread_t set_thread;
    pthread_create(&set_thread, NULL, set_thread_func, &test_data);
    
    CANShmResult result = can_shm_subscribe(0x400, 1, 1000, subscribe_callback, &test_data);
    
    pthread_join(set_thread, NULL);
    pthread_mutex_destroy(&test_data.mutex);
    
    TEST_ASSERT(result == CAN_SHM_SUCCESS, "TC-SUB-001: Single subscription (result)");
    TEST_ASSERT(test_data.received_count == 1, "TC-SUB-001: Single subscription (count)");
}

// TC-SUB-005: タイムアウト発生
void test_subscribe_timeout() {
    CANData data_out;
    CANShmResult result = can_shm_subscribe_once(0x800, 100, &data_out);
    TEST_ASSERT(result == CAN_SHM_ERROR_TIMEOUT, "TC-SUB-005: Subscribe timeout");
}

// 無効なCAN IDのテスト
void test_invalid_can_id() {
    uint8_t data[] = {0x01, 0x02};
    CANShmResult result = can_shm_set(0x20000000, 2, data); // 29bitを超える値
    TEST_ASSERT(result == CAN_SHM_ERROR_INVALID_ID, "Invalid CAN ID (too large)");
}

// 無効なDLCのテスト
void test_invalid_dlc() {
    uint8_t data[65];
    CANShmResult result = can_shm_set(0x100, 65, data); // DLC > 64
    TEST_ASSERT(result == CAN_SHM_ERROR_INVALID_PARAM, "Invalid DLC (too large)");
}

// 統計情報テスト
void test_statistics() {
    uint64_t sets, gets, subscribes;
    CANShmResult result = can_shm_get_stats(&sets, &gets, &subscribes);
    TEST_ASSERT(result == CAN_SHM_SUCCESS, "Get statistics");
    printf("Stats - Sets: %lu, Gets: %lu, Subscribes: %lu\n", sets, gets, subscribes);
}

int main() {
    printf("Starting CAN Shared Memory Tests...\n\n");
    
    // システム初期化
    CANShmResult init_result = can_shm_init();
    if (init_result != CAN_SHM_SUCCESS) {
        printf("Failed to initialize CAN shared memory system: %d\n", init_result);
        return 1;
    }
    
    // テスト実行
    test_set_normal_data();
    test_set_max_dlc();
    test_set_dlc_zero();
    test_set_overwrite();
    
    test_get_existing_id();
    test_get_nonexistent_id();
    test_get_dlc_zero();
    
    test_subscribe_once();
    test_subscribe_timeout();
    
    test_invalid_can_id();
    test_invalid_dlc();
    
    test_statistics();
    
    // システム終了処理
    can_shm_cleanup();
    
    // 結果表示
    printf("\n=== Test Results ===\n");
    printf("Total tests: %d\n", tests_run);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("All tests PASSED!\n");
        return 0;
    } else {
        printf("Some tests FAILED!\n");
        return 1;
    }
}