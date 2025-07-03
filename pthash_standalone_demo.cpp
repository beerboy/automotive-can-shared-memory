/*
 * PTHash スタンドアロンデモ
 * PTHashライブラリのビルドされたバイナリを直接使用
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <string>
#include <iomanip>
#include <fstream>
#include <cstdlib>

int main() {
    std::cout << "=== PTHash スタンドアロンデモ ===" << std::endl;
    
    // 1. 車載CAN IDセットをファイルに書き出し
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
    
    // 2. CAN IDをファイルに書き出し
    std::ofstream can_file("can_ids.txt");
    for (const auto& id : can_ids) {
        can_file << id << std::endl;
    }
    can_file.close();
    
    std::cout << "CAN IDファイル 'can_ids.txt' を作成しました" << std::endl;
    
    // 3. PTHashコマンドでMPHF構築
    std::string pthash_cmd = "cd pthash/build && ./build"
                            " -n " + std::to_string(can_ids.size()) +
                            " -i ../../can_ids.txt"
                            " -l 2.0"      // lambda: 平均バケットサイズ
                            " -a 0.99"     // alpha: 負荷率
                            " -e D-D"      // encoder: Dictionary-Dictionary
                            " -r xor"      // search: XOR displacement
                            " -b skew"     // bucketer: skew
                            " -s 12345"    // seed
                            " -q 1000000"  // queries for benchmark
                            " --minimal --verbose --check"
                            " -o ../../can_pthash.mph";
    
    std::cout << "\n=== PTHashビルド実行 ===" << std::endl;
    std::cout << "実行コマンド:" << std::endl;
    std::cout << pthash_cmd << std::endl;
    
    auto start = std::chrono::high_resolution_clock::now();
    int result = std::system(pthash_cmd.c_str());
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    if (result == 0) {
        std::cout << "\n✅ PTHashビルド成功!" << std::endl;
        std::cout << "総実行時間: " << duration.count() << " ms" << std::endl;
        
        // 4. 生成されたファイルの情報
        std::ifstream mph_file("can_pthash.mph", std::ios::binary | std::ios::ate);
        if (mph_file.is_open()) {
            std::streamsize file_size = mph_file.tellg();
            mph_file.close();
            
            std::cout << "\n=== ファイル情報 ===" << std::endl;
            std::cout << "MPHFファイルサイズ: " << file_size << " bytes" << std::endl;
            std::cout << "1キーあたり: " << static_cast<double>(file_size * 8) / can_ids.size() 
                      << " bits/key" << std::endl;
            
            // 5. メモリ効率比較
            std::cout << "\n=== メモリ効率比較 ===" << std::endl;
            
            // 単純配列の場合
            size_t array_size = 0x800;  // 2048 (2^11)
            size_t array_memory = array_size * 8;
            
            // リニアプロービングの場合（50%負荷率）
            size_t linear_size = can_ids.size() * 2;
            size_t linear_memory = linear_size * 8;
            
            std::cout << "PTHash         : " << file_size << " bytes" << std::endl;
            std::cout << "単純配列       : " << array_memory << " bytes" << std::endl;
            std::cout << "リニアプロービング: " << linear_memory << " bytes" << std::endl;
            
            double vs_array = static_cast<double>(array_memory) / file_size;
            double vs_linear = static_cast<double>(linear_memory) / file_size;
            
            std::cout << "単純配列比     : " << std::fixed << std::setprecision(1) 
                      << vs_array << "x メモリ効率化" << std::endl;
            std::cout << "リニアプロービング比: " << std::fixed << std::setprecision(1) 
                      << vs_linear << "x メモリ効率化" << std::endl;
        }
        
    } else {
        std::cout << "\n❌ PTHashビルド失敗 (返り値: " << result << ")" << std::endl;
    }
    
    // 6. クリーンアップ
    std::cout << "\n=== クリーンアップ ===" << std::endl;
    std::remove("can_ids.txt");
    std::remove("can_pthash.mph");
    std::cout << "一時ファイルを削除しました" << std::endl;
    
    return result == 0 ? 0 : 1;
}