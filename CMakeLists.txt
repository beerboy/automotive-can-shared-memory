cmake_minimum_required(VERSION 3.10)
project(can_shared_memory C)

# C標準とコンパイラフラグ設定
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -g")

# スレッドライブラリを探す
find_package(Threads REQUIRED)

# 共有メモリライブラリ（Linux/POSIXの場合）
if(UNIX AND NOT APPLE)
    set(RT_LIBRARY rt)
endif()

# ライブラリターゲット
add_library(can_shm SHARED
    can_shm_api.c
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

# テスト用のカスタムターゲット
enable_testing()
add_test(NAME can_shm_tests COMMAND test_can_shm)

# カスタムターゲット：テスト実行
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS test_can_shm
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