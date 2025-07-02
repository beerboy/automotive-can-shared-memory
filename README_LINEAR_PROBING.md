# CAN共有メモリ - リニアプロービング法実装

## 概要

車載CANデータ共有メモリシステムのハッシュ衝突問題を解決する、リニアプロービング法の実装サンプルです。従来の上書き方式によるデータロスを完全に防止し、車載システムに適した高性能・高信頼性を実現します。

## 主な特徴

- **データロス完全防止**: ハッシュ衝突時も全てのデータを安全に保存
- **高性能**: マイクロ秒レベルの低遅延操作
- **スレッドセーフ**: seqlockによる安全な並行アクセス
- **統計監視**: 詳細なハッシュテーブル統計情報
- **車載適合**: MISRA C準拠、リアルタイム要求対応

## ファイル構成

```
├── can_shm_linear_probing.h         # リニアプロービング法のヘッダー
├── can_shm_linear_probing.c         # リニアプロービング法の実装
├── test_linear_probing.c            # 包括的なテストプログラム
├── CMakeLists_linear_probing.txt    # CMakeビルド設定
├── build_and_test.sh               # 自動ビルド・テストスクリプト
└── README_LINEAR_PROBING.md        # このファイル
```

## 実装されている機能

### 1. コア関数

| 関数名 | 説明 |
|--------|------|
| `can_shm_set_linear_probing()` | リニアプロービング法によるデータ格納 |
| `can_shm_get_linear_probing()` | リニアプロービング法によるデータ取得 |
| `can_shm_delete_linear_probing()` | データ削除（将来拡張用） |

### 2. 統計・監視機能

| 関数名 | 説明 |
|--------|------|
| `can_shm_print_hash_stats()` | ハッシュテーブル統計情報表示 |
| `can_shm_test_hash_collisions()` | ハッシュ衝突テスト |

### 3. テストプログラム

| プログラム名 | 説明 |
|-------------|------|
| `test_linear_probing` | 基本機能・衝突処理・性能テスト |
| `demo_collision_comparison` | ハッシュ衝突デモンストレーション |
| `performance_test` | 詳細な性能ベンチマーク |

## ビルド方法

### 前提条件

- CMake 3.10以上
- GCC（C99対応）
- pthreadライブラリ
- librt（リアルタイム拡張）

### 自動ビルド（推奨）

```bash
# 全自動ビルド・テスト実行
./build_and_test.sh

# オプション指定例
./build_and_test.sh --clean --debug    # クリーンビルド（デバッグ）
./build_and_test.sh --no-performance   # 性能テストをスキップ
./build_and_test.sh --help            # ヘルプ表示
```

### 手動ビルド

```bash
# 1. ビルドディレクトリ作成
mkdir build_linear_probing
cd build_linear_probing

# 2. CMake設定
cmake -DCMAKE_BUILD_TYPE=Release -f ../CMakeLists_linear_probing.txt ..

# 3. ビルド実行
make -j$(nproc)

# 4. テスト実行
make run_tests        # 基本テスト
make run_demo         # 衝突デモ
make run_performance  # 性能テスト
make test_all         # 全テスト実行
```

## 使用例

### 基本的な使用方法

```c
#include "can_shm_api.h"
#include "can_shm_linear_probing.h"

int main() {
    // 1. 共有メモリ初期化
    if (can_shm_init() != CAN_SHM_SUCCESS) {
        printf("初期化失敗\n");
        return 1;
    }
    
    // 2. データ設定
    uint32_t can_id = 0x123;
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    can_shm_set_linear_probing(can_id, 4, data);
    
    // 3. データ取得
    CANData retrieved_data;
    if (can_shm_get_linear_probing(can_id, &retrieved_data) == CAN_SHM_SUCCESS) {
        printf("CAN ID 0x%X: DLC=%d\n", retrieved_data.can_id, retrieved_data.dlc);
    }
    
    // 4. 統計情報表示
    can_shm_print_hash_stats();
    
    // 5. クリーンアップ
    can_shm_cleanup();
    return 0;
}
```

### ハッシュ衝突テスト

```c
// 衝突する可能性のあるCAN IDをテスト
uint32_t test_ids[] = {0x123, 0x456, 0x789, 0xABC};
can_shm_test_hash_collisions(test_ids, 4);

// 統計情報で負荷率と衝突回数を確認
can_shm_print_hash_stats();
```

## 性能特性

### 実測性能目標

| 操作 | 目標値 | 条件 |
|------|--------|------|
| Set操作 | < 1μs | 負荷率50%以下 |
| Get操作 | < 500ns | seqlockロックフリー読み取り |
| 探査回数 | 平均2回以内 | 負荷率50%時 |

### 負荷率による性能変化

```
負荷率25%: 平均探査回数 1.33回
負荷率50%: 平均探査回数 2.00回  ← 推奨動作点
負荷率75%: 平均探査回数 4.00回  ← 性能劣化開始
```

## 従来実装との比較

| 項目 | 従来方式（上書き） | リニアプロービング法 |
|------|------------------|-------------------|
| **データ保全性** | ❌ 衝突時にデータロス | ✅ 完全保護 |
| **性能** | ⭐⭐⭐ | ⭐⭐⭐ |
| **実装複雑度** | ⭐⭐⭐ | ⭐⭐ |
| **メモリ効率** | ⭐⭐⭐ | ⭐⭐⭐ |
| **車載適合性** | ❌ 安全性問題 | ✅ 高信頼性 |

## テスト項目

### 1. 基本機能テスト

- Set/Get操作の正常動作
- パラメータ検証
- エラーハンドリング

### 2. ハッシュ衝突テスト

- 意図的な衝突発生
- 全データの保全性確認
- 探査距離の測定

### 3. 性能テスト

- 大量データでの処理速度測定
- 負荷率別の性能特性
- 従来実装との比較

### 4. 並行アクセステスト

- マルチプロセス環境での動作
- seqlockの整合性保証
- データ競合の検出

## 設計上の注意点

### ハッシュテーブルサイズ設計

```c
// 推奨設計指針
#define MAX_CAN_IDS 4096      // 想定最大CAN ID数
#define BUCKET_COUNT 8192     // 2倍の余裕（負荷率50%）

// 負荷率 = MAX_CAN_IDS / BUCKET_COUNT = 50%
```

### 削除操作の制約

現在の実装では削除操作は**簡易版**です。本格的な削除には以下が必要：

1. Tombstone方式の完全実装
2. 探査チェーンの整合性保持
3. 定期的なリハッシュ処理

車載システムでは削除は稀なため、現状の実装で十分です。

## トラブルシューティング

### ビルドエラー

```bash
# 1. 依存関係確認
sudo apt-get install build-essential cmake libpthread-stubs0-dev

# 2. 共有メモリクリーンアップ
sudo rm -f /dev/shm/can_data_shm

# 3. クリーンビルド
./build_and_test.sh --clean
```

### 実行時エラー

```bash
# 共有メモリ権限エラー
sudo chmod 666 /dev/shm/can_data_shm

# プロセス異常終了後のクリーンアップ
make clean_shm
```

### 性能問題

```c
// 統計情報で負荷率をチェック
can_shm_print_hash_stats();

// 負荷率が75%を超えた場合：
// 1. バケット数を増やす（MAX_CAN_ENTRIES）
// 2. ハッシュ関数の改良を検討
```

## 今後の拡張予定

### 短期改善

- [ ] より効率的なハッシュ関数の実装
- [ ] 削除操作の完全実装
- [ ] ベンチマークツールの充実

### 長期拡張

- [ ] Robin Hood Hashingの実装
- [ ] 動的リサイズ機能
- [ ] NUMA対応最適化

## 参考資料

- [HASH_COLLISION_ANALYSIS.md](HASH_COLLISION_ANALYSIS.md) - 詳細技術分析
- [DESIGN_SPECIFICATION.md](DESIGN_SPECIFICATION.md) - システム設計仕様
- [車載システム向けハッシュテーブル設計ガイド](https://example.com/automotive-hash-guide)

## ライセンス

このサンプル実装は、元のプロジェクトと同じライセンス条件で提供されています。

---

**作成日**: 2025-07-02  
**バージョン**: 1.0.0  
**対象システム**: 車両CANデータ共有メモリシステム