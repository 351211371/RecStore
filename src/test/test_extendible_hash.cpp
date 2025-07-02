#include "storage/dram/extendible_hash.h"
#include <chrono>
#include <gtest/gtest.h>
#include <random>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class ExtendibleHashTest : public ::testing::Test {
protected:
  void SetUp() override {
    hash_table_ = new ExtendibleHash(16); // 初始容量为16
  }

  void TearDown() override { delete hash_table_; }

  ExtendibleHash *hash_table_;
};

// 基本插入和查找测试
TEST_F(ExtendibleHashTest, BasicInsertAndGet) {
  Key_t key1 = 123;
  Value_t value1 = 456;

  hash_table_->Insert(key1, value1);
  Value_t retrieved = hash_table_->Get(key1);

  EXPECT_EQ(retrieved, value1);
}

// 测试不存在的键
TEST_F(ExtendibleHashTest, GetNonExistentKey) {
  Key_t key = 999;
  Value_t retrieved = hash_table_->Get(key);

  EXPECT_EQ(retrieved, NONE);
}

// 测试键值覆盖
TEST_F(ExtendibleHashTest, KeyOverwrite) {
  Key_t key = 100;
  Value_t value1 = 200;
  Value_t value2 = 300;

  hash_table_->Insert(key, value1);
  hash_table_->Insert(key, value2);

  Value_t retrieved = hash_table_->Get(key);
  EXPECT_EQ(retrieved, value2);
}

// 测试多个键值对
TEST_F(ExtendibleHashTest, MultipleInsertAndGet) {
  const int num_pairs = 100;
  std::vector<std::pair<Key_t, Value_t>> test_data;

  // 准备测试数据
  for (int i = 0; i < num_pairs; i++) {
    test_data.emplace_back(i, i * 10);
  }

  // 插入数据
  for (const auto &pair : test_data) {
    hash_table_->Insert(pair.first, pair.second);
  }

  // 验证数据
  for (const auto &pair : test_data) {
    Value_t retrieved = hash_table_->Get(pair.first);
    EXPECT_EQ(retrieved, pair.second) << "Failed for key " << pair.first;
  }
}

// 测试InsertOnly方法
TEST_F(ExtendibleHashTest, InsertOnly) {
  Key_t key = 50;
  Value_t value = 100;

  bool result = hash_table_->InsertOnly(key, value);
  EXPECT_TRUE(result);

  Value_t retrieved = hash_table_->Get(key);
  EXPECT_EQ(retrieved, value);
}

// 测试容量和利用率
TEST_F(ExtendibleHashTest, CapacityAndUtilization) {
  // 插入一些数据
  for (Key_t i = 1; i <= 10; i++) {
    hash_table_->Insert(i, i * 2);
  }

  size_t capacity = hash_table_->Capacity();
  double utilization = hash_table_->Utilization();

  EXPECT_GT(capacity, 0);
  EXPECT_GE(utilization, 0.0);
  EXPECT_LE(utilization, 100.0);
}

// 测试扩容行为
TEST_F(ExtendibleHashTest, DirectoryExpansion) {
  const int num_keys = 1000;

  // 插入大量数据以触发扩容
  for (Key_t i = 1; i <= num_keys; i++) {
    hash_table_->Insert(i, i);
  }

  // 验证所有数据都能正确检索
  for (Key_t i = 1; i <= num_keys; i++) {
    Value_t retrieved = hash_table_->Get(i);
    EXPECT_EQ(retrieved, i) << "Failed for key " << i;
  }
}

// 测试随机数据
TEST_F(ExtendibleHashTest, RandomData) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<Key_t> key_dist(1, 10000);
  std::uniform_int_distribution<Value_t> value_dist(1, 100000);

  const int num_operations = 500;
  std::vector<std::pair<Key_t, Value_t>> inserted_data;

  for (int i = 0; i < num_operations; i++) {
    Key_t key = key_dist(gen);
    Value_t value = value_dist(gen);

    hash_table_->Insert(key, value);
    inserted_data.emplace_back(key, value);
  }

  // 验证最后插入的值（因为可能有重复键）
  std::unordered_map<Key_t, Value_t> expected;
  for (const auto &pair : inserted_data) {
    expected[pair.first] = pair.second;
  }

  for (const auto &pair : expected) {
    Value_t retrieved = hash_table_->Get(pair.first);
    EXPECT_EQ(retrieved, pair.second);
  }
}

// 边界测试 - 特殊键值
TEST_F(ExtendibleHashTest, SpecialKeys) {
  // 测试0值
  hash_table_->Insert(0, 42);
  EXPECT_EQ(hash_table_->Get(0), 42);

  // 测试最大值
  Key_t max_key = UINT64_MAX - 10; // 避免使用INVALID和SENTINEL
  Value_t max_value = UINT64_MAX - 10;
  hash_table_->Insert(max_key, max_value);
  EXPECT_EQ(hash_table_->Get(max_key), max_value);
}

// 并发测试
TEST_F(ExtendibleHashTest, ConcurrentInsertAndGet) {
  const int num_threads = 4;
  const int operations_per_thread = 100;
  std::vector<std::thread> threads;

  // 每个线程插入不同范围的键
  for (int t = 0; t < num_threads; t++) {
    threads.emplace_back([this, t, operations_per_thread]() {
      Key_t start_key = t * operations_per_thread;
      for (int i = 0; i < operations_per_thread; i++) {
        Key_t key = start_key + i;
        Value_t value = key * 2;
        hash_table_->Insert(key, value);
      }
    });
  }

  // 等待所有线程完成
  for (auto &thread : threads) {
    thread.join();
  }

  // 验证所有数据
  for (int t = 0; t < num_threads; t++) {
    Key_t start_key = t * operations_per_thread;
    for (int i = 0; i < operations_per_thread; i++) {
      Key_t key = start_key + i;
      Value_t expected_value = key * 2;
      Value_t retrieved = hash_table_->Get(key);
      EXPECT_EQ(retrieved, expected_value) << "Thread " << t << ", key " << key;
    }
  }
}

// 性能测试
TEST_F(ExtendibleHashTest, PerformanceTest) {
  const int num_operations = 10000;
  auto start_time = std::chrono::high_resolution_clock::now();

  // 插入操作
  for (Key_t i = 1; i <= num_operations; i++) {
    hash_table_->Insert(i, i);
  }

  auto insert_end_time = std::chrono::high_resolution_clock::now();

  // 查找操作
  for (Key_t i = 1; i <= num_operations; i++) {
    Value_t value = hash_table_->Get(i);
    EXPECT_EQ(value, i);
  }

  auto get_end_time = std::chrono::high_resolution_clock::now();

  auto insert_duration = std::chrono::duration_cast<std::chrono::microseconds>(
      insert_end_time - start_time);
  auto get_duration = std::chrono::duration_cast<std::chrono::microseconds>(
      get_end_time - insert_end_time);

  std::cout << "Performance Results for " << num_operations << " operations:\n";
  std::cout << "Insert time: " << insert_duration.count() << " microseconds\n";
  std::cout << "Get time: " << get_duration.count() << " microseconds\n";
  std::cout << "Insert throughput: "
            << (num_operations * 1000000.0 / insert_duration.count())
            << " ops/sec\n";
  std::cout << "Get throughput: "
            << (num_operations * 1000000.0 / get_duration.count())
            << " ops/sec\n";
}

// 内存使用测试
TEST_F(ExtendibleHashTest, MemoryUsage) {
  const int num_keys = 1000;

  // 插入数据前的容量
  size_t initial_capacity = hash_table_->Capacity();

  // 插入数据
  for (Key_t i = 1; i <= num_keys; i++) {
    hash_table_->Insert(i, i);
  }

  size_t final_capacity = hash_table_->Capacity();
  double utilization = hash_table_->Utilization();

  // 验证容量增长
  EXPECT_GE(final_capacity, initial_capacity);

  // 验证利用率合理
  EXPECT_GT(utilization, 0.0);
  EXPECT_LE(utilization, 100.0);

  std::cout << "Memory usage test:\n";
  std::cout << "Initial capacity: " << initial_capacity << "\n";
  std::cout << "Final capacity: " << final_capacity << "\n";
  std::cout << "Utilization: " << utilization << "%\n";
}

// 哈希冲突测试
TEST_F(ExtendibleHashTest, HashCollisions) {
  // 使用会产生相同哈希值的键（这取决于具体的哈希函数实现）
  // 这里我们插入大量连续的键来增加冲突概率
  const int num_keys = 256; // 选择一个可能导致哈希冲突的数量

  for (Key_t i = 0; i < num_keys; i++) {
    hash_table_->Insert(i, i * 3);
  }

  // 验证所有键都能正确检索
  for (Key_t i = 0; i < num_keys; i++) {
    Value_t retrieved = hash_table_->Get(i);
    EXPECT_EQ(retrieved, i * 3) << "Failed for key " << i;
  }
}