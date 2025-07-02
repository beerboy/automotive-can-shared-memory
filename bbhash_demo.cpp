/**
 * BBHash Demo for CAN ID Minimal Perfect Hashing
 * 
 * 注意: このファイルは実際のBBHashライブラリをインストールして使用するための
 * デモコードです。BBHashライブラリは以下からダウンロードできます：
 * https://github.com/rizkg/BBHash
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <set>
#include <algorithm>

// BBHashライブラリがインストールされている場合
// #include "BooPHF.h"
// typedef boomphf::SingleHashFunctor<uint64_t> hasher_t;
// typedef boomphf::mphf<uint64_t, hasher_t> boophf_t;

/**
 * BBHash使用例（疑似コード）
 */
class BBHashDemo {
private:
    // std::unique_ptr<boophf_t> mph_;
    std::vector<uint64_t> can_ids_;
    
public:
    BBHashDemo(const std::vector<uint64_t>& can_ids) : can_ids_(can_ids) {
        std::cout << "BBHash Demo: " << can_ids.size() << " CAN IDs\n";
    }
    
    void build_mph() {
        auto start = std::chrono::high_resolution_clock::now();
        
        // 実際のBBHash使用コード
        /*
        mph_ = std::make_unique<boophf_t>(
            can_ids_.size(),    // 要素数
            can_ids_,           // キーのベクター
            8                   // スレッド数
        );
        */
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "MPH construction time: " << duration.count() << "ms\n";
        std::cout << "Memory efficiency: ~3.7 bits per key\n";
    }
    
    uint64_t lookup(uint64_t can_id) {
        // 実際のBBHash検索
        /*
        if (mph_) {
            return mph_->lookup(can_id);
        }
        */
        
        // デモ用の疑似実装
        auto it = std::find(can_ids_.begin(), can_ids_.end(), can_id);
        if (it != can_ids_.end()) {
            return std::distance(can_ids_.begin(), it);
        }
        return UINT64_MAX;  // Not found
    }
    
    void benchmark_lookup() {
        if (can_ids_.empty()) return;
        
        const int NUM_LOOKUPS = 100000;
        auto start = std::chrono::high_resolution_clock::now();
        
        uint64_t sum = 0;
        for (int i = 0; i < NUM_LOOKUPS; i++) {
            uint64_t can_id = can_ids_[i % can_ids_.size()];
            sum += lookup(can_id);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        
        std::cout << "Average lookup time: " 
                  << duration.count() / NUM_LOOKUPS << " ns\n";
        std::cout << "(Checksum: " << sum << ")\n";
    }
};

/**
 * 大規模CAN IDセット生成
 */
std::vector<uint64_t> generate_can_ids(size_t count) {
    std::vector<uint64_t> ids;
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // 実際の車載システムのCAN ID範囲を模擬
    std::vector<std::pair<uint64_t, uint64_t>> ranges = {
        {0x100, 0x7FF},        // 標準CAN ID
        {0x18DA0000, 0x18DAFFFF}, // UDS診断
        {0x18DB0000, 0x18DBFFFF}, // UDS応答
        {0x1CEC0000, 0x1CECFFFF}, // J1939 DM1
        {0x1CECFF00, 0x1CECFFFF}, // J1939 DM2
    };
    
    size_t per_range = count / ranges.size();
    
    for (const auto& range : ranges) {
        std::uniform_int_distribution<uint64_t> dist(range.first, range.second);
        
        std::set<uint64_t> unique_ids;
        while (unique_ids.size() < per_range && unique_ids.size() < (range.second - range.first)) {
            unique_ids.insert(dist(gen));
        }
        
        for (uint64_t id : unique_ids) {
            ids.push_back(id);
        }
    }
    
    // 不足分をランダムで補完
    std::uniform_int_distribution<uint64_t> dist(0x100, 0x1FFFFFFF);
    while (ids.size() < count) {
        uint64_t id = dist(gen);
        if (std::find(ids.begin(), ids.end(), id) == ids.end()) {
            ids.push_back(id);
        }
    }
    
    return ids;
}

int main() {
    std::cout << "BBHash Minimal Perfect Hash Demo\n";
    std::cout << "================================\n\n";
    
    // 様々なサイズでテスト
    std::vector<size_t> test_sizes = {100, 1000, 10000, 100000};
    
    for (size_t size : test_sizes) {
        std::cout << "Testing with " << size << " CAN IDs:\n";
        std::cout << "------------------------\n";
        
        auto can_ids = generate_can_ids(size);
        
        BBHashDemo demo(can_ids);
        demo.build_mph();
        demo.benchmark_lookup();
        
        // メモリ使用量の理論値計算
        size_t memory_bits = size * 37 / 10;  // 3.7 bits per key
        size_t memory_kb = memory_bits / 8 / 1024;
        
        std::cout << "Theoretical memory: " << memory_kb << " KB\n";
        std::cout << "Load factor: 100% (minimal perfect)\n\n";
    }
    
    std::cout << "BBHashライブラリの利点:\n";
    std::cout << "✅ 超大規模データ対応（10^12個まで実証済み）\n";
    std::cout << "✅ 高速生成（並列処理対応）\n";
    std::cout << "✅ 低メモリ（3.7 bits/key）\n";
    std::cout << "✅ 最小完全ハッシュ（負荷率100%）\n";
    std::cout << "✅ 工業利用実績多数\n\n";
    
    std::cout << "インストール方法:\n";
    std::cout << "git clone https://github.com/rizkg/BBHash\n";
    std::cout << "cd BBHash\n";
    std::cout << "make\n";
    std::cout << "# ヘッダーファイルをプロジェクトに追加\n";
    
    return 0;
}