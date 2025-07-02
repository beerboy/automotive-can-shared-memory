#!/usr/bin/env python3
"""
完全ハッシュ関数の生成困難性分析
"""
import math

def collision_probability(n, m):
    """衝突が発生しない確率を計算"""
    if n > m:
        return 0.0
    
    # 近似計算（大きなnでは正確な計算は困難）
    return math.exp(-n * (n - 1) / (2 * m))

def analyze_hash_difficulty():
    """様々なサイズでの生成困難性を分析"""
    print("完全ハッシュ関数の生成困難性分析")
    print("=" * 50)
    print(f"{'N(IDs)':<8} {'M(Table)':<10} {'負荷率':<8} {'成功確率':<12} {'期待試行回数':<12}")
    print("-" * 50)
    
    test_cases = [
        (16, 32),
        (32, 64), 
        (50, 64),
        (100, 128),
        (500, 512),
        (1000, 1024),
        (5000, 5120),
        (10000, 10240),
    ]
    
    for n, m in test_cases:
        load_factor = n / m
        prob = collision_probability(n, m)
        expected_trials = 1 / prob if prob > 1e-10 else float('inf')
        
        print(f"{n:<8} {m:<10} {load_factor:<8.2f} {prob:<12.2e} {expected_trials:<12.1e}")

if __name__ == "__main__":
    analyze_hash_difficulty()