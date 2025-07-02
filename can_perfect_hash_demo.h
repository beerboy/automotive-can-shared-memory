#ifndef CAN_PERFECT_HASH_DEMO_H
#define CAN_PERFECT_HASH_DEMO_H

/*
 * Demo Perfect Hash Function for CAN IDs
 * ======================================
 * 
 * 手動で作成した完全ハッシュ関数のデモ実装
 * 小さなCAN IDセットを使用してコンセプトを実証
 * 
 * CAN ID セット (16個):
 * 0x100, 0x101, 0x102, 0x103, 0x200, 0x201, 0x202, 0x203,
 * 0x300, 0x301, 0x302, 0x303, 0x400, 0x401, 0x402, 0x403
 */

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Perfect hash parameters
#define PERFECT_HASH_SALT 0x12345678U
#define PERFECT_HASH_TABLE_SIZE 32
#define PERFECT_HASH_NUM_CAN_IDS 16
#define PERFECT_HASH_ALGORITHM "demo"

// デモ用の固定CAN IDセット
static const uint32_t DEMO_CAN_IDS[PERFECT_HASH_NUM_CAN_IDS] = {
    0x100, 0x101, 0x102, 0x103,  // Engine ECU
    0x200, 0x201, 0x202, 0x203,  // Transmission ECU
    0x300, 0x301, 0x302, 0x303,  // Body ECU
    0x400, 0x401, 0x402, 0x403   // ABS ECU
};

// 手動で設計した完全ハッシュ関数
static inline uint32_t can_id_perfect_hash_demo(uint32_t can_id) {
    // シンプルなマッピング設計
    uint32_t group = (can_id >> 8) & 0xF;  // 上位4bit（ECUグループ）
    uint32_t offset = can_id & 0xF;        // 下位4bit（ECU内オフセット）
    
    // グループごとのベースインデックス
    uint32_t base_index;
    switch (group) {
        case 1: base_index = 0;  break;   // 0x1xx -> 0-7
        case 2: base_index = 8;  break;   // 0x2xx -> 8-15
        case 3: base_index = 16; break;   // 0x3xx -> 16-23
        case 4: base_index = 24; break;   // 0x4xx -> 24-31
        default: return 0xFFFFFFFF;       // 無効
    }
    
    // 最終インデックス計算
    uint32_t index = base_index + (offset & 0x7);
    return (index < PERFECT_HASH_TABLE_SIZE) ? index : 0xFFFFFFFF;
}

// バックアップ用のハッシュ関数（数学的なアプローチ）
static inline uint32_t can_id_perfect_hash_math(uint32_t can_id) {
    // 乗法ハッシュ法（手動調整済み）
    return ((can_id * 0x9E3779B9U) ^ 0x87654321U) % PERFECT_HASH_TABLE_SIZE;
}

// 逆マッピング: index -> CAN ID（デモ用手動設定）
static const uint32_t INDEX_TO_CAN_ID_MAP_DEMO[PERFECT_HASH_TABLE_SIZE] = {
    // Engine ECU (0x100-0x103)
    0x100, 0x101, 0x102, 0x103, 0, 0, 0, 0,
    // Transmission ECU (0x200-0x203)  
    0x200, 0x201, 0x202, 0x203, 0, 0, 0, 0,
    // Body ECU (0x300-0x303)
    0x300, 0x301, 0x302, 0x303, 0, 0, 0, 0,
    // ABS ECU (0x400-0x403)
    0x400, 0x401, 0x402, 0x403, 0, 0, 0, 0
};

// 検証関数
static inline int is_valid_can_id_for_perfect_hash_demo(uint32_t can_id) {
    uint32_t index = can_id_perfect_hash_demo(can_id);
    return (index != 0xFFFFFFFF) && 
           (index < PERFECT_HASH_TABLE_SIZE) && 
           (INDEX_TO_CAN_ID_MAP_DEMO[index] == can_id);
}

// 全CAN IDをテストする関数
static inline int test_perfect_hash_demo(void) {
    int success_count = 0;
    int collision_count = 0;
    int used_indices[PERFECT_HASH_TABLE_SIZE] = {0};
    
    printf("Testing Perfect Hash Demo Function:\n");
    printf("===================================\n");
    
    for (int i = 0; i < PERFECT_HASH_NUM_CAN_IDS; i++) {
        uint32_t can_id = DEMO_CAN_IDS[i];
        uint32_t index = can_id_perfect_hash_demo(can_id);
        
        printf("CAN ID 0x%03X -> Index %2u", can_id, index);
        
        if (index == 0xFFFFFFFF) {
            printf(" [INVALID]\n");
            continue;
        }
        
        if (index >= PERFECT_HASH_TABLE_SIZE) {
            printf(" [OUT_OF_RANGE]\n");
            continue;
        }
        
        if (used_indices[index]) {
            printf(" [COLLISION]\n");
            collision_count++;
            continue;
        }
        
        used_indices[index] = 1;
        printf(" [OK]\n");
        success_count++;
    }
    
    printf("\nResults:\n");
    printf("Success: %d/%d (%.1f%%)\n", 
           success_count, PERFECT_HASH_NUM_CAN_IDS, 
           (float)success_count / PERFECT_HASH_NUM_CAN_IDS * 100.0f);
    printf("Collisions: %d\n", collision_count);
    printf("Load Factor: %.1f%%\n", 
           (float)success_count / PERFECT_HASH_TABLE_SIZE * 100.0f);
    
    return (success_count == PERFECT_HASH_NUM_CAN_IDS) && (collision_count == 0);
}

// 統計情報
typedef struct {
    uint32_t total_can_ids;
    uint32_t table_size;
    uint32_t salt;
    float load_factor;
    const char* algorithm;
} PerfectHashStats;

static inline PerfectHashStats get_perfect_hash_stats_demo(void) {
    PerfectHashStats stats = {
        .total_can_ids = PERFECT_HASH_NUM_CAN_IDS,
        .table_size = PERFECT_HASH_TABLE_SIZE,
        .salt = PERFECT_HASH_SALT,
        .load_factor = (float)PERFECT_HASH_NUM_CAN_IDS / PERFECT_HASH_TABLE_SIZE,
        .algorithm = PERFECT_HASH_ALGORITHM
    };
    return stats;
}

#ifdef __cplusplus
}
#endif

#endif // CAN_PERFECT_HASH_DEMO_H