#!/usr/bin/env python3
"""
Perfect Hash Function Generator for CAN ID Mapping
==================================================

車載CANデータ用の完全ハッシュ関数生成ツール。
固定されたCAN ID集合から衝突のないハッシュ関数を生成し、
O(1)での検索を実現する。

Usage:
    python3 generate_perfect_hash.py can_id_list.txt > can_perfect_hash.h
    python3 generate_perfect_hash.py --test 1000
"""

import sys
import random
import hashlib
import time
from typing import List, Tuple, Optional, Dict

class PerfectHashGenerator:
    """完全ハッシュ関数生成器"""
    
    def __init__(self, can_ids: List[int], table_size: int = None):
        self.can_ids = sorted(set(can_ids))  # 重複除去・ソート
        self.num_ids = len(self.can_ids)
        self.table_size = table_size or self._calculate_optimal_size()
        self.max_iterations = 10000000  # 最大試行回数
        
        print(f"# Perfect Hash Generator", file=sys.stderr)
        print(f"# CAN IDs: {self.num_ids}", file=sys.stderr)
        print(f"# Table Size: {self.table_size}", file=sys.stderr)
        print(f"# Load Factor: {self.num_ids / self.table_size:.2%}", file=sys.stderr)
        
    def _calculate_optimal_size(self) -> int:
        """最適なテーブルサイズを計算"""
        # 要素数の1.2倍を基準とし、2の累乗に近い値を選択
        base_size = int(self.num_ids * 1.2)
        
        # 2の累乗に調整
        power = 1
        while (1 << power) < base_size:
            power += 1
        
        return 1 << power
    
    def _simple_hash(self, can_id: int, salt: int) -> int:
        """シンプルなハッシュ関数"""
        # 乗法ハッシュ法（黄金比を使用）
        return ((can_id ^ salt) * 0x9E3779B9) % self.table_size
    
    def _jenkins_hash(self, can_id: int, salt: int) -> int:
        """Jenkinsハッシュ関数（高品質）"""
        key = can_id ^ salt
        key = (~key) + (key << 21)
        key = key ^ (key >> 24)
        key = (key + (key << 3)) + (key << 8)
        key = key ^ (key >> 14)
        key = (key + (key << 2)) + (key << 4)
        key = key ^ (key >> 28)
        key = key + (key << 31)
        return (key & 0xFFFFFFFF) % self.table_size
    
    def _murmur_hash(self, can_id: int, salt: int) -> int:
        """MurmurHash3の簡易版"""
        key = can_id ^ salt
        key ^= key >> 16
        key *= 0x85ebca6b
        key ^= key >> 13
        key *= 0xc2b2ae35
        key ^= key >> 16
        return (key & 0xFFFFFFFF) % self.table_size
    
    def _test_hash_function(self, hash_func, salt: int) -> Tuple[bool, Dict[int, int]]:
        """ハッシュ関数をテストし、衝突の有無をチェック"""
        used_indices = set()
        id_to_index = {}
        
        for can_id in self.can_ids:
            index = hash_func(can_id, salt)
            
            if index in used_indices:
                return False, {}  # 衝突発生
            
            used_indices.add(index)
            id_to_index[can_id] = index
        
        return True, id_to_index
    
    def generate_simple_perfect_hash(self) -> Optional[Tuple[int, str, Dict[int, int]]]:
        """シンプルな完全ハッシュ関数を生成"""
        print(f"# Generating simple perfect hash...", file=sys.stderr)
        
        for iteration in range(self.max_iterations):
            salt = random.randint(1, 0xFFFFFFFF)
            success, mapping = self._test_hash_function(self._simple_hash, salt)
            
            if success:
                print(f"# Found perfect hash after {iteration + 1} iterations", file=sys.stderr)
                hash_name = "simple"
                return salt, hash_name, mapping
            
            if (iteration + 1) % 100000 == 0:
                print(f"# Tried {iteration + 1} iterations...", file=sys.stderr)
        
        return None
    
    def generate_jenkins_perfect_hash(self) -> Optional[Tuple[int, str, Dict[int, int]]]:
        """Jenkins完全ハッシュ関数を生成"""
        print(f"# Generating Jenkins perfect hash...", file=sys.stderr)
        
        for iteration in range(self.max_iterations):
            salt = random.randint(1, 0xFFFFFFFF)
            success, mapping = self._test_hash_function(self._jenkins_hash, salt)
            
            if success:
                print(f"# Found Jenkins perfect hash after {iteration + 1} iterations", file=sys.stderr)
                hash_name = "jenkins"
                return salt, hash_name, mapping
            
            if (iteration + 1) % 100000 == 0:
                print(f"# Tried {iteration + 1} iterations...", file=sys.stderr)
        
        return None
    
    def generate_murmur_perfect_hash(self) -> Optional[Tuple[int, str, Dict[int, int]]]:
        """MurmurHash完全ハッシュ関数を生成"""
        print(f"# Generating MurmurHash perfect hash...", file=sys.stderr)
        
        for iteration in range(self.max_iterations):
            salt = random.randint(1, 0xFFFFFFFF)
            success, mapping = self._test_hash_function(self._murmur_hash, salt)
            
            if success:
                print(f"# Found MurmurHash perfect hash after {iteration + 1} iterations", file=sys.stderr)
                hash_name = "murmur"
                return salt, hash_name, mapping
            
            if (iteration + 1) % 100000 == 0:
                print(f"# Tried {iteration + 1} iterations...", file=sys.stderr)
        
        return None
    
    def generate_best_perfect_hash(self) -> Optional[Tuple[int, str, Dict[int, int]]]:
        """最適な完全ハッシュ関数を生成（複数手法を試行）"""
        start_time = time.time()
        
        # 複数のハッシュ関数を順番に試行
        generators = [
            self.generate_simple_perfect_hash,
            self.generate_jenkins_perfect_hash,
            self.generate_murmur_perfect_hash,
        ]
        
        for generator in generators:
            result = generator()
            if result:
                end_time = time.time()
                print(f"# Generation completed in {end_time - start_time:.2f} seconds", file=sys.stderr)
                return result
        
        print(f"# Failed to generate perfect hash function", file=sys.stderr)
        return None
    
    def generate_c_header(self, salt: int, hash_type: str, mapping: Dict[int, int]) -> str:
        """Cヘッダーファイルを生成"""
        
        # ハッシュ関数のC実装
        if hash_type == "simple":
            hash_function = f"""
static inline uint32_t can_id_perfect_hash(uint32_t can_id) {{
    return ((can_id ^ 0x{salt:08X}U) * 0x9E3779B9U) % {self.table_size};
}}"""
        elif hash_type == "jenkins":
            hash_function = f"""
static inline uint32_t can_id_perfect_hash(uint32_t can_id) {{
    uint32_t key = can_id ^ 0x{salt:08X}U;
    key = (~key) + (key << 21);
    key = key ^ (key >> 24);
    key = (key + (key << 3)) + (key << 8);
    key = key ^ (key >> 14);
    key = (key + (key << 2)) + (key << 4);
    key = key ^ (key >> 28);
    key = key + (key << 31);
    return (key & 0xFFFFFFFFU) % {self.table_size};
}}"""
        elif hash_type == "murmur":
            hash_function = f"""
static inline uint32_t can_id_perfect_hash(uint32_t can_id) {{
    uint32_t key = can_id ^ 0x{salt:08X}U;
    key ^= key >> 16;
    key *= 0x85ebca6bU;
    key ^= key >> 13;
    key *= 0xc2b2ae35U;
    key ^= key >> 16;
    return (key & 0xFFFFFFFFU) % {self.table_size};
}}"""
        else:
            hash_function = "// Unknown hash type"
        
        # 検証用のマッピングテーブル
        validation_table = "static const uint32_t CAN_ID_TO_INDEX_MAP[] = {\n"
        for can_id in sorted(mapping.keys()):
            index = mapping[can_id]
            validation_table += f"    // CAN ID 0x{can_id:08X} -> Index {index}\n"
        validation_table += "};"
        
        # 逆マッピングテーブル（インデックス -> CAN ID）
        reverse_mapping = ["0"] * self.table_size
        for can_id, index in mapping.items():
            reverse_mapping[index] = f"0x{can_id:08X}U"
        
        reverse_table = "static const uint32_t INDEX_TO_CAN_ID_MAP[PERFECT_HASH_TABLE_SIZE] = {\n"
        for i in range(0, self.table_size, 8):
            line = "    "
            for j in range(8):
                if i + j < self.table_size:
                    line += f"{reverse_mapping[i + j]:>12}"
                    if i + j < self.table_size - 1:
                        line += ","
                if j < 7 and i + j < self.table_size - 1:
                    line += " "
            reverse_table += line + "\n"
        reverse_table += "};"
        
        # ヘッダーファイル生成
        header = f"""#ifndef CAN_PERFECT_HASH_H
#define CAN_PERFECT_HASH_H

/*
 * Auto-generated Perfect Hash Function for CAN IDs
 * ================================================
 * 
 * Generated at: {time.strftime('%Y-%m-%d %H:%M:%S')}
 * Hash algorithm: {hash_type}
 * Salt: 0x{salt:08X}
 * Number of CAN IDs: {self.num_ids}
 * Table size: {self.table_size}
 * Load factor: {self.num_ids / self.table_size:.2%}
 * 
 * DO NOT EDIT THIS FILE MANUALLY!
 * Regenerate with: python3 tools/generate_perfect_hash.py
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {{
#endif

// Perfect hash parameters
#define PERFECT_HASH_SALT 0x{salt:08X}U
#define PERFECT_HASH_TABLE_SIZE {self.table_size}
#define PERFECT_HASH_NUM_CAN_IDS {self.num_ids}
#define PERFECT_HASH_ALGORITHM "{hash_type}"

// Perfect hash function{hash_function}

// Validation function
static inline int is_valid_can_id_for_perfect_hash(uint32_t can_id) {{
    uint32_t index = can_id_perfect_hash(can_id);
    return (index < PERFECT_HASH_TABLE_SIZE) && 
           (INDEX_TO_CAN_ID_MAP[index] == can_id);
}}

// Reverse mapping: index -> CAN ID
{reverse_table}

// Statistics
typedef struct {{
    uint32_t total_can_ids;
    uint32_t table_size;
    uint32_t salt;
    float load_factor;
    const char* algorithm;
}} PerfectHashStats;

static inline PerfectHashStats get_perfect_hash_stats(void) {{
    PerfectHashStats stats = {{
        .total_can_ids = PERFECT_HASH_NUM_CAN_IDS,
        .table_size = PERFECT_HASH_TABLE_SIZE,
        .salt = PERFECT_HASH_SALT,
        .load_factor = (float)PERFECT_HASH_NUM_CAN_IDS / PERFECT_HASH_TABLE_SIZE,
        .algorithm = PERFECT_HASH_ALGORITHM
    }};
    return stats;
}}

#ifdef __cplusplus
}}
#endif

#endif // CAN_PERFECT_HASH_H"""
        
        return header

def load_can_ids_from_file(filename: str) -> List[int]:
    """ファイルからCAN IDリストを読み込み"""
    can_ids = []
    
    try:
        with open(filename, 'r') as f:
            for line_num, line in enumerate(f, 1):
                line = line.strip()
                
                # コメント行や空行をスキップ
                if not line or line.startswith('#') or line.startswith('//'):
                    continue
                
                # コメント部分を除去
                line = line.split('#')[0].strip()
                if not line:
                    continue
                
                # 複数の形式をサポート
                try:
                    # カンマ区切り対応
                    if ',' in line:
                        for id_str in line.split(','):
                            id_str = id_str.strip()
                            if not id_str:
                                continue
                            if id_str.lower().startswith('0x'):
                                can_id = int(id_str, 16)
                            else:
                                can_id = int(id_str, 10)
                            
                            # CAN ID範囲チェック (29bit)
                            if 0 <= can_id <= 0x1FFFFFFF:
                                can_ids.append(can_id)
                            else:
                                print(f"Warning: CAN ID 0x{can_id:X} out of range (line {line_num})", 
                                      file=sys.stderr)
                        continue
                    
                    # 16進数形式 (0x123, 0X123)
                    if line.lower().startswith('0x'):
                        can_id = int(line, 16)
                    # 10進数形式
                    elif line.isdigit():
                        can_id = int(line, 10)
                    else:
                        # その他の形式は16進として試行
                        can_id = int(line, 16)
                    
                    # CAN ID範囲チェック (29bit)
                    if 0 <= can_id <= 0x1FFFFFFF:
                        can_ids.append(can_id)
                    else:
                        print(f"Warning: CAN ID 0x{can_id:X} out of range (line {line_num})", 
                              file=sys.stderr)
                        
                except ValueError:
                    print(f"Warning: Invalid CAN ID format '{line}' (line {line_num})", 
                          file=sys.stderr)
                    
    except FileNotFoundError:
        print(f"Error: File '{filename}' not found", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error reading file '{filename}': {e}", file=sys.stderr)
        sys.exit(1)
    
    return can_ids

def generate_test_can_ids(count: int) -> List[int]:
    """テスト用のCAN IDセットを生成"""
    can_ids = []
    
    # 実際の車載システムでよく使われるCAN IDパターンを模擬
    ranges = [
        (0x100, 0x1FF),    # Engine ECU
        (0x200, 0x2FF),    # Transmission ECU
        (0x300, 0x3FF),    # Body ECU
        (0x400, 0x4FF),    # ABS/ESP ECU
        (0x500, 0x5FF),    # HVAC ECU
        (0x600, 0x6FF),    # Instrument Cluster
        (0x700, 0x7FF),    # Gateway ECU
        (0x18DA0000, 0x18DAFFFF),  # UDS診断用（拡張CAN）
        (0x18DB0000, 0x18DBFFFF),  # UDS診断応答用
    ]
    
    random.seed(42)  # 再現可能な結果のため
    
    for start, end in ranges:
        range_count = min(count // len(ranges), end - start + 1)
        range_ids = random.sample(range(start, end + 1), range_count)
        can_ids.extend(range_ids)
        
        if len(can_ids) >= count:
            break
    
    # 不足分をランダムで補完
    while len(can_ids) < count:
        can_id = random.randint(0, 0x1FFFFFFF)
        if can_id not in can_ids:
            can_ids.append(can_id)
    
    return sorted(can_ids[:count])

def main():
    """メイン関数"""
    if len(sys.argv) < 2:
        print("Usage: python3 generate_perfect_hash.py <can_id_file>", file=sys.stderr)
        print("       python3 generate_perfect_hash.py --test <count>", file=sys.stderr)
        sys.exit(1)
    
    if sys.argv[1] == "--test":
        if len(sys.argv) < 3:
            count = 1000
        else:
            count = int(sys.argv[2])
        
        print(f"# Generating test CAN IDs ({count} entries)", file=sys.stderr)
        can_ids = generate_test_can_ids(count)
    else:
        filename = sys.argv[1]
        can_ids = load_can_ids_from_file(filename)
    
    if not can_ids:
        print("Error: No valid CAN IDs found", file=sys.stderr)
        sys.exit(1)
    
    # 完全ハッシュ関数生成
    generator = PerfectHashGenerator(can_ids)
    result = generator.generate_best_perfect_hash()
    
    if result is None:
        print("Error: Could not generate perfect hash function", file=sys.stderr)
        sys.exit(1)
    
    salt, hash_type, mapping = result
    
    # Cヘッダーファイル出力
    header_content = generator.generate_c_header(salt, hash_type, mapping)
    print(header_content)
    
    # 統計情報出力
    print(f"# Successfully generated perfect hash function:", file=sys.stderr)
    print(f"#   Algorithm: {hash_type}", file=sys.stderr)
    print(f"#   Salt: 0x{salt:08X}", file=sys.stderr)
    print(f"#   CAN IDs: {len(can_ids)}", file=sys.stderr)
    print(f"#   Table size: {generator.table_size}", file=sys.stderr)
    print(f"#   Load factor: {len(can_ids) / generator.table_size:.2%}", file=sys.stderr)

if __name__ == "__main__":
    main()