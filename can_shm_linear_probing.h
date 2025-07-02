#ifndef CAN_SHM_LINEAR_PROBING_H
#define CAN_SHM_LINEAR_PROBING_H

#include "can_shm_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * リニアプロービング法を使用したSet関数
 * ハッシュ衝突時に次の空きバケットを線形探索で見つけてデータを格納
 * 
 * @param can_id CAN ID (29bit有効値)
 * @param dlc データ長 (0~64)
 * @param data データ部へのポインタ (dlc=0の場合NULLも可)
 * @return CAN_SHM_SUCCESS on success, error code on failure
 */
CANShmResult can_shm_set_linear_probing(uint32_t can_id, uint16_t dlc, const uint8_t* data);

/**
 * リニアプロービング法を使用したGet関数
 * ハッシュ値から開始して線形探索でCAN IDを検索
 * 
 * @param can_id CAN ID (29bit有効値)
 * @param data_out 取得したCANデータの格納先
 * @return CAN_SHM_SUCCESS on success, error code on failure
 */
CANShmResult can_shm_get_linear_probing(uint32_t can_id, CANData* data_out);

/**
 * リニアプロービング法での削除（将来拡張用）
 * Tombstone方式で削除マークを設定
 * 
 * @param can_id CAN ID (29bit有効値)
 * @return CAN_SHM_SUCCESS on success, error code on failure
 */
CANShmResult can_shm_delete_linear_probing(uint32_t can_id);

/**
 * ハッシュテーブルの統計情報を取得
 * 負荷率、衝突回数、最大探査距離などを出力
 */
void can_shm_print_hash_stats(void);

/**
 * 指定されたCAN IDのハッシュ衝突をテスト
 * 複数のCAN IDが同じハッシュ値を持つかをチェック
 * 
 * @param can_ids テスト対象のCAN ID配列
 * @param count 配列の要素数
 */
void can_shm_test_hash_collisions(const uint32_t* can_ids, int count);

#ifdef __cplusplus
}
#endif

#endif // CAN_SHM_LINEAR_PROBING_H