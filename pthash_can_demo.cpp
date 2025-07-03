#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <cassert>
#include <iomanip>

#include "pthash/include/pthash.hpp"

using namespace pthash;

int main() {
    std::cout << "=== PTHash 車載CAN IDデモ ===" << std::endl;
    
    // 1. 実際の車載CAN IDセット（16進数）
    std::vector<uint64_t> can_ids = {
        0x10C, 0x18C, 0x1A0, 0x1A8, 0x1AA, 0x1C4, 0x1D0, 0x1D8,
        0x1E8, 0x1F8, 0x200, 0x208, 0x210, 0x218, 0x220, 0x228,
        0x230, 0x238, 0x240, 0x248, 0x250, 0x258, 0x260, 0x268,
        0x270, 0x278, 0x280, 0x288, 0x290, 0x298, 0x2A0, 0x2A8,
        0x2B0, 0x2B8, 0x2C0, 0x2C8, 0x2D0, 0x2D8, 0x2E0, 0x2E8,
        0x2F0, 0x2F8, 0x300, 0x308, 0x310, 0x318, 0x320, 0x328,
        0x330, 0x338, 0x340, 0x348, 0x350, 0x358, 0x360, 0x368,
        0x370, 0x378, 0x380, 0x388, 0x390, 0x398, 0x3A0, 0x3A8,
        0x3B0, 0x3B8, 0x3C0, 0x3C8, 0x3D0, 0x3D8, 0x3E0, 0x3E8,
        0x3F0, 0x3F8, 0x400, 0x408, 0x410, 0x418, 0x420, 0x428,
        0x430, 0x438, 0x440, 0x448, 0x450, 0x458, 0x460, 0x468,
        0x470, 0x478, 0x480, 0x488, 0x490, 0x498, 0x4A0, 0x4A8,
        0x4B0, 0x4B8, 0x4C0, 0x4C8, 0x4D0, 0x4D8, 0x4E0, 0x4E8,
        0x4F0, 0x4F8, 0x500, 0x508, 0x510, 0x518, 0x520, 0x528,
        0x530, 0x538, 0x540, 0x548, 0x550, 0x558, 0x560, 0x568,
        0x570, 0x578, 0x580, 0x588, 0x590, 0x598, 0x5A0, 0x5A8,
        0x5B0, 0x5B8, 0x5C0, 0x5C8, 0x5D0, 0x5D8, 0x5E0, 0x5E8,
        0x5F0, 0x5F8, 0x600, 0x608, 0x610, 0x618, 0x620, 0x628,
        0x630, 0x638, 0x640, 0x648, 0x650, 0x658, 0x660, 0x668,
        0x670, 0x678, 0x680, 0x688, 0x690, 0x698, 0x6A0, 0x6A8,
        0x6B0, 0x6B8, 0x6C0, 0x6C8, 0x6D0, 0x6D8, 0x6E0, 0x6E8,
        0x6F0, 0x6F8, 0x700, 0x708, 0x710, 0x718, 0x720, 0x728,
        0x730, 0x738, 0x740, 0x748, 0x750, 0x758, 0x760, 0x768,
        0x770, 0x778, 0x780, 0x788, 0x790, 0x798, 0x7A0, 0x7A8,
        0x7B0, 0x7B8, 0x7C0, 0x7C8, 0x7D0, 0x7D8, 0x7E0, 0x7E8,
        0x7F0, 0x7F8
    };
    
    std::cout << "CAN ID数: " << can_ids.size() << std::endl;
    std::cout << "CAN ID範囲: 0x" << std::hex << can_ids.front() 
              << " - 0x" << can_ids.back() << std::dec << std::endl;
    
    // 2. PTHash設定（高メモリ効率重視）
    build_configuration config;
    config.seed = 12345;
    config.lambda = 2.0;      // 小さなバケットサイズで高効率
    config.alpha = 0.99;      // 高負荷率
    config.verbose = true;
    // config.minimal_output = true;  // このパラメータは存在しない
    
    // 3. PTHashビルド（最高メモリ効率のPHOBIC使用）
    std::cout << "\n=== PTHashビルド開始 ===" << std::endl;
    
    typedef phobic<murmurhash2_64> pthash_type;
    pthash_type f;
    
    auto start = std::chrono::high_resolution_clock::now();
    auto timings = f.build_in_internal_memory(can_ids.begin(), can_ids.size(), config);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "ビルド時間: " << duration.count() / 1000.0 << " ms" << std::endl;
    double bits_per_key = static_cast<double>(f.num_bits()) / f.num_keys();
    std::cout << "メモリ効率: " << bits_per_key << " bits/key" << std::endl;
    std::cout << "総メモリ使用量: " << f.num_bits() / 8.0 << " bytes" << std::endl;
    
    // 4. 正しさの検証
    std::cout << "\n=== 正しさ検証 ===" << std::endl;
    bool all_correct = true;
    std::vector<uint64_t> hash_values;
    
    for (size_t i = 0; i < std::min(size_t(10), can_ids.size()); i++) {
        uint64_t hash_val = f(can_ids[i]);
        hash_values.push_back(hash_val);
        std::cout << "f(0x" << std::hex << can_ids[i] << std::dec 
                  << ") = " << hash_val << std::endl;
        
        if (hash_val >= can_ids.size()) {
            std::cout << "ERROR: ハッシュ値が範囲外!" << std::endl;
            all_correct = false;
        }
    }
    
    // 5. 全体の一意性検証
    std::cout << "\n=== 全体一意性検証 ===" << std::endl;
    std::vector<uint64_t> all_hash_values;
    for (const auto& can_id : can_ids) {
        all_hash_values.push_back(f(can_id));
    }
    
    std::sort(all_hash_values.begin(), all_hash_values.end());
    bool unique = true;
    for (size_t i = 1; i < all_hash_values.size(); i++) {
        if (all_hash_values[i] == all_hash_values[i-1]) {
            std::cout << "ERROR: 重複ハッシュ値: " << all_hash_values[i] << std::endl;
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
    const int num_queries = 1000000;
    
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_queries; i++) {
        volatile uint64_t result = f(can_ids[i % can_ids.size()]);
        (void)result;  // 警告回避
    }
    end = std::chrono::high_resolution_clock::now();
    
    auto query_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    double avg_query_time = static_cast<double>(query_duration.count()) / num_queries;
    
    std::cout << "クエリ数: " << num_queries << std::endl;
    std::cout << "平均クエリ時間: " << avg_query_time << " ns" << std::endl;
    std::cout << "スループット: " << 1000000000.0 / avg_query_time << " queries/sec" << std::endl;
    
    // 7. 従来手法との比較
    std::cout << "\n=== 従来手法との比較 ===" << std::endl;
    
    // 単純配列の場合
    size_t array_size = 0x800;  // 2048 (2^11)
    size_t array_memory = array_size * 8;  // 8 bytes per entry
    
    // リニアプロービングの場合（50%負荷率）
    size_t linear_size = can_ids.size() * 2;
    size_t linear_memory = linear_size * 8;
    
    std::cout << "PTHash      : " << f.num_bits() / 8 << " bytes" << std::endl;
    std::cout << "単純配列    : " << array_memory << " bytes" << std::endl;
    std::cout << "リニアプロービング: " << linear_memory << " bytes" << std::endl;
    
    double vs_array = static_cast<double>(array_memory) / (f.num_bits() / 8.0);
    double vs_linear = static_cast<double>(linear_memory) / (f.num_bits() / 8.0);
    
    std::cout << "単純配列比: " << vs_array << "x メモリ効率化" << std::endl;
    std::cout << "リニアプロービング比: " << vs_linear << "x メモリ効率化" << std::endl;
    
    // 8. 結果サマリー
    std::cout << "\n=== 結果サマリー ===" << std::endl;
    std::cout << "CAN ID数: " << can_ids.size() << std::endl;
    std::cout << "ビルド時間: " << duration.count() / 1000.0 << " ms" << std::endl;
    std::cout << "メモリ効率: " << bits_per_key << " bits/key" << std::endl;
    std::cout << "総メモリ: " << f.num_bits() / 8 << " bytes" << std::endl;
    std::cout << "平均クエリ時間: " << avg_query_time << " ns" << std::endl;
    std::cout << "正しさ: " << (all_correct && unique ? "✅ OK" : "❌ NG") << std::endl;
    
    return 0;
}