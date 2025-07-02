#!/usr/bin/env python3
"""
大規模IDセット用の実用的ハッシュアプローチ
"""
import hashlib
import random

class TwoLevelPerfectHash:
    """2段階完全ハッシュ関数（簡易版）"""
    
    def __init__(self, keys):
        self.keys = set(keys)
        self.num_buckets = len(keys) // 10 + 1  # 第1段のバケット数
        self.bucket_tables = {}
        self.bucket_salts = {}
        self._build()
    
    def _build(self):
        # 第1段：キーをバケットに分割
        buckets = [[] for _ in range(self.num_buckets)]
        
        for key in self.keys:
            bucket_id = hash(key) % self.num_buckets
            buckets[bucket_id].append(key)
        
        # 第2段：各バケット内で完全ハッシュを生成
        for bucket_id, bucket_keys in enumerate(buckets):
            if not bucket_keys:
                continue
                
            # 小さなバケットなら完全ハッシュが見つかりやすい
            bucket_size = len(bucket_keys) * 2  # 負荷率50%
            
            # 簡単な試行でバケット用完全ハッシュを生成
            for salt in range(1000):  # 最大1000回試行
                used_indices = set()
                mapping = {}
                success = True
                
                for key in bucket_keys:
                    index = (hash((key, salt)) % bucket_size)
                    if index in used_indices:
                        success = False
                        break
                    used_indices.add(index)
                    mapping[key] = index
                
                if success:
                    self.bucket_tables[bucket_id] = mapping
                    self.bucket_salts[bucket_id] = salt
                    break
            else:
                # 完全ハッシュが見つからない場合は通常のハッシュ
                self.bucket_tables[bucket_id] = {
                    key: hash((key, 42)) % bucket_size 
                    for key in bucket_keys
                }
                self.bucket_salts[bucket_id] = 42
    
    def lookup(self, key):
        """キーのインデックスを取得"""
        if key not in self.keys:
            return None
            
        bucket_id = hash(key) % self.num_buckets
        if bucket_id not in self.bucket_tables:
            return None
            
        return self.bucket_tables[bucket_id].get(key)
    
    def stats(self):
        """統計情報"""
        total_buckets = len(self.bucket_tables)
        perfect_buckets = 0
        
        for bucket_id, table in self.bucket_tables.items():
            # 衝突チェック（簡易）
            indices = list(table.values())
            if len(indices) == len(set(indices)):
                perfect_buckets += 1
        
        return {
            'total_keys': len(self.keys),
            'total_buckets': total_buckets,
            'perfect_buckets': perfect_buckets,
            'perfect_ratio': perfect_buckets / total_buckets if total_buckets > 0 else 0
        }

def test_large_scale():
    """大規模テスト"""
    print("大規模ID用2段階ハッシュテスト")
    print("=" * 40)
    
    for size in [100, 1000, 10000]:
        # ランダムなIDセットを生成
        ids = random.sample(range(0x100000, 0x1FFFFF), size)
        
        # 2段階ハッシュを構築
        hash_table = TwoLevelPerfectHash(ids)
        stats = hash_table.stats()
        
        print(f"\nIDセットサイズ: {size}")
        print(f"バケット数: {stats['total_buckets']}")
        print(f"完全ハッシュバケット: {stats['perfect_buckets']}")
        print(f"完全ハッシュ率: {stats['perfect_ratio']:.1%}")
        
        # 検索テスト
        test_key = ids[0]
        result = hash_table.lookup(test_key)
        print(f"テスト検索 ({test_key:06X}): {result}")

if __name__ == "__main__":
    test_large_scale()