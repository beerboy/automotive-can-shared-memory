cmake_minimum_required(VERSION 3.15)
project(can_shared_memory C CXX)

# C標準とコンパイラフラグ設定
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -g")

# C++標準設定
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g")

# スレッドライブラリを探す
find_package(Threads REQUIRED)

# 共有メモリライブラリ（Linux/POSIXの場合）
if(UNIX AND NOT APPLE)
    set(RT_LIBRARY rt)
endif()

# ライブラリターゲット
add_library(can_shm SHARED
    can_shm_api.c
    can_shm_linear_probing.c
    can_shm_perfect_hash.c
)

target_link_libraries(can_shm 
    Threads::Threads
    ${RT_LIBRARY}
)

target_include_directories(can_shm PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# テスト実行可能ファイル
add_executable(test_can_shm
    test_can_shm.c
)

target_link_libraries(test_can_shm
    can_shm
    Threads::Threads
    ${RT_LIBRARY}
)

# 完全ハッシュテスト実行可能ファイル
add_executable(test_perfect_hash
    test_perfect_hash.c
)

target_link_libraries(test_perfect_hash
    can_shm
    Threads::Threads
    ${RT_LIBRARY}
)

# リニアプロービングテスト実行可能ファイル
add_executable(test_linear_probing
    test_linear_probing.c
)

target_link_libraries(test_linear_probing
    can_shm
    Threads::Threads
    ${RT_LIBRARY}
)

# テスト用のカスタムターゲット
enable_testing()
add_test(NAME can_shm_tests COMMAND test_can_shm)
add_test(NAME perfect_hash_tests COMMAND test_perfect_hash)
add_test(NAME linear_probing_tests COMMAND test_linear_probing)

# カスタムターゲット：テスト実行
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS test_can_shm test_perfect_hash test_linear_probing
    COMMENT "Running CAN shared memory tests"
)

# カスタムターゲット：共有メモリクリーンアップ
add_custom_target(cleanup_shm
    COMMAND ${CMAKE_COMMAND} -E remove -f /dev/shm/can_data_shm
    COMMENT "Cleaning up shared memory segments"
)

# カスタムターゲット：デバッグ情報表示
add_custom_target(debug_shm
    COMMAND ls -la /dev/shm/can_data_shm* 2>/dev/null || echo "No shared memory segments found"
    COMMENT "Showing shared memory debug information"
)

# インストール設定
install(TARGETS can_shm
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES
    can_shm_api.h
    can_shm_types.h
    DESTINATION include
)

# PTHashライブラリを追加
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/pthash)
    add_subdirectory(pthash)
    
    # PTHashデモ実行ファイル
    add_executable(pthash_simple_demo pthash_simple_demo.cpp)
    target_link_libraries(pthash_simple_demo INTERFACE PTHASH)
    
    # 車載CAN IDデモ実行ファイル
    add_executable(pthash_can_demo pthash_can_demo.cpp)
    target_link_libraries(pthash_can_demo INTERFACE PTHASH)
    
    # PTHashテストターゲット
    add_custom_target(run_pthash_tests
        COMMAND ./pthash_simple_demo
        COMMAND ./pthash_can_demo
        DEPENDS pthash_simple_demo pthash_can_demo
        COMMENT "Running PTHash demo tests"
    )
endif()