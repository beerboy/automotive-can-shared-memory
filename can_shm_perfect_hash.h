#ifndef CAN_SHM_PERFECT_HASH_H
#define CAN_SHM_PERFECT_HASH_H

#include "can_shm_types.h"
#include "can_perfect_hash_demo.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 完全ハッシュ関数を使用したSet関数
 * ハッシュ衝突が発生しないため、常にO(1)での格納が保証される
 * 
 * @param can_id CAN ID (事前定義されたIDセットのみ有効)
 * @param dlc データ長 (0~64)
 * @param data データ部へのポインタ (dlc=0の場合NULLも可)
 * @return CAN_SHM_SUCCESS on success, error code on failure
 */
CANShmResult can_shm_set_perfect_hash(uint32_t can_id, uint16_t dlc, const uint8_t* data);

/**
 * 完全ハッシュ関数を使用したGet関数
 * ハッシュ衝突が発生しないため、常にO(1)での取得が保証される
 * 
 * @param can_id CAN ID (事前定義されたIDセットのみ有効)
 * @param data_out 取得したCANデータの格納先
 * @return CAN_SHM_SUCCESS on success, error code on failure
 */
CANShmResult can_shm_get_perfect_hash(uint32_t can_id, CANData* data_out);

/**
 * 完全ハッシュ関数での削除
 * 
 * @param can_id CAN ID (事前定義されたIDセットのみ有効)
 * @return CAN_SHM_SUCCESS on success, error code on failure
 */
CANShmResult can_shm_delete_perfect_hash(uint32_t can_id);

/**
 * 完全ハッシュテーブルの統計情報を取得
 */
void can_shm_print_perfect_hash_stats(void);

/**
 * 完全ハッシュ関数のテスト（全CAN ID検証）
 * 
 * @return 1 if all tests pass, 0 if any test fails
 */
int can_shm_test_perfect_hash_function(void);

/**
 * 性能ベンチマーク実行
 * 完全ハッシュ vs リニアプロービングの性能比較
 */
void can_shm_benchmark_perfect_vs_linear(void);

#ifdef __cplusplus
}
#endif

#endif // CAN_SHM_PERFECT_HASH_H