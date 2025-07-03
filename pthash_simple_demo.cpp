#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <cassert>

#include "pthash/include/pthash.hpp"

using namespace pthash;

int main() {
    std::cout << "=== PTHash 基本デモ ===" << std::endl;
    
    // 1. 基本的なキーセット作成
    std::vector<std::string> keys = {
        "apple", "banana", "cherry", "date", "elderberry",
        "fig", "grape", "honeydew", "kiwi", "lemon"
    };
    
    std::cout << "キーセット数: " << keys.size() << std::endl;
    for (const auto& key : keys) {
        std::cout << "  " << key << std::endl;
    }
    
    // 2. PTHash設定
    build_configuration config;
    config.seed = 42;
    config.lambda = 3.0;      // 平均バケットサイズ
    config.alpha = 0.99;      // 負荷率
    config.verbose = true;
    // config.minimal_output = true;  // このパラメータは存在しない
    
    // 3. PTHashビルド
    std::cout << "\n=== PTHashビルド開始 ===" << std::endl;
    
    // シンプルなsingle_phfを使用
    typedef single_phf<murmurhash2_64,              // ハッシュ関数
                       uniform_bucketer,             // バケッター
                       dictionary_dictionary,        // エンコーダー
                       true,                        // minimal
                       pthash_search_type::xor_displacement  // search type
                       > pthash_type;
    
    pthash_type f;
    
    auto start = std::chrono::high_resolution_clock::now();
    auto timings = f.build_in_internal_memory(keys.begin(), keys.size(), config);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "ビルド時間: " << duration.count() / 1000.0 << " ms" << std::endl;
    std::cout << "メモリ効率: " << static_cast<double>(f.num_bits()) / f.num_keys() 
              << " bits/key" << std::endl;
    
    // 4. 正しさの検証
    std::cout << "\n=== 正しさ検証 ===" << std::endl;
    bool all_correct = true;
    std::vector<uint64_t> hash_values;
    
    for (const auto& key : keys) {
        uint64_t hash_val = f(key);
        hash_values.push_back(hash_val);
        std::cout << "f(\"" << key << "\") = " << hash_val << std::endl;
        
        if (hash_val >= keys.size()) {
            std::cout << "ERROR: ハッシュ値が範囲外!" << std::endl;
            all_correct = false;
        }
    }
    
    // 5. 一意性の検証
    std::cout << "\n=== 一意性検証 ===" << std::endl;
    std::sort(hash_values.begin(), hash_values.end());
    bool unique = true;
    for (size_t i = 1; i < hash_values.size(); i++) {
        if (hash_values[i] == hash_values[i-1]) {
            std::cout << "ERROR: 重複ハッシュ値: " << hash_values[i] << std::endl;
            unique = false;
        }
    }
    
    if (unique) {
        std::cout << "✅ 全てのハッシュ値が一意です" << std::endl;
    } else {
        std::cout << "❌ ハッシュ値に重複があります" << std::endl;
    }
    
    // 6. 性能測定
    std::cout << "\n=== 性能測定 ===" << std::endl;
    const int num_queries = 100000;
    
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_queries; i++) {
        volatile uint64_t result = f(keys[i % keys.size()]);
        (void)result;  // 警告回避
    }
    end = std::chrono::high_resolution_clock::now();
    
    auto query_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    double avg_query_time = static_cast<double>(query_duration.count()) / num_queries;
    
    std::cout << "クエリ数: " << num_queries << std::endl;
    std::cout << "平均クエリ時間: " << avg_query_time << " ns" << std::endl;
    std::cout << "スループット: " << 1000000000.0 / avg_query_time << " queries/sec" << std::endl;
    
    // 7. 結果サマリー
    std::cout << "\n=== 結果サマリー ===" << std::endl;
    std::cout << "キー数: " << keys.size() << std::endl;
    std::cout << "ビルド時間: " << duration.count() / 1000.0 << " ms" << std::endl;
    std::cout << "メモリ効率: " << static_cast<double>(f.num_bits()) / f.num_keys() 
              << " bits/key" << std::endl;
    std::cout << "平均クエリ時間: " << avg_query_time << " ns" << std::endl;
    std::cout << "正しさ: " << (all_correct && unique ? "✅ OK" : "❌ NG") << std::endl;
    
    return 0;
}