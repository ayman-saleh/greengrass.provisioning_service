#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

int main(int argc, char **argv) {
    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);
    
    // Set up test logging
    spdlog::set_level(spdlog::level::warn);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    
    // Run all tests
    return RUN_ALL_TESTS();
} 