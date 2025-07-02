#ifndef CAN_SHM_API_H
#define CAN_SHM_API_H

#include "can_shm_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 共有メモリシステム初期化
 * @return CAN_SHM_SUCCESS on success, error code on failure
 */
CANShmResult can_shm_init(void);

/**
 * 共有メモリシステム終了処理
 * @return CAN_SHM_SUCCESS on success, error code on failure
 */
CANShmResult can_shm_cleanup(void);

/**
 * Set関数 - CANデータを共有メモリに格納
 * @param can_id CAN ID (29bit有効値)
 * @param dlc データ長 (0~64)
 * @param data データ部へのポインタ (dlc=0の場合NULLも可)
 * @return CAN_SHM_SUCCESS on success, error code on failure
 */
CANShmResult can_shm_set(uint32_t can_id, uint16_t dlc, const uint8_t* data);

/**
 * Get関数 - CAN IDを元に共有メモリからCANデータを取得
 * @param can_id CAN ID (29bit有効値)
 * @param data_out 取得したCANデータの格納先
 * @return CAN_SHM_SUCCESS on success, error code on failure
 */
CANShmResult can_shm_get(uint32_t can_id, CANData* data_out);

/**
 * Subscribe関数 - CAN IDの更新を購読
 * @param can_id CAN ID (29bit有効値)
 * @param subscribe_count 購読回数 (0=無限回)
 * @param timeout_ms タイムアウト時間[ミリ秒] (<0=タイムアウト無効)
 * @param callback データ受信時のコールバック関数
 * @param user_data コールバック関数に渡すユーザーデータ
 * @return CAN_SHM_SUCCESS on success, error code on failure
 */
CANShmResult can_shm_subscribe(uint32_t can_id, 
                               uint32_t subscribe_count,
                               int32_t timeout_ms,
                               CANDataCallback callback,
                               void* user_data);

/**
 * Subscribe関数（簡易版） - 単発でデータを取得
 * @param can_id CAN ID (29bit有効値) 
 * @param timeout_ms タイムアウト時間[ミリ秒] (<0=タイムアウト無効)
 * @param data_out 取得したCANデータの格納先
 * @return CAN_SHM_SUCCESS on success, error code on failure
 */
CANShmResult can_shm_subscribe_once(uint32_t can_id,
                                    int32_t timeout_ms,
                                    CANData* data_out);

/**
 * 統計情報取得
 * @param total_sets Set操作回数の格納先
 * @param total_gets Get操作回数の格納先
 * @param total_subscribes Subscribe操作回数の格納先
 * @return CAN_SHM_SUCCESS on success, error code on failure
 */
CANShmResult can_shm_get_stats(uint64_t* total_sets,
                               uint64_t* total_gets,
                               uint64_t* total_subscribes);

/**
 * デバッグ用：共有メモリの状態を出力
 */
void can_shm_debug_print(void);

#ifdef __cplusplus
}
#endif

#endif // CAN_SHM_API_H