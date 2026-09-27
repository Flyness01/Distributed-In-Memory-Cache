#pragma once
#include <cstddef>
#include <optional>
#include <pthread.h>
#include <string>
#include <unordered_map>
#include <vector>

class PartitionedCache {
 public:
  explicit PartitionedCache(std::size_t partition_count);
  ~PartitionedCache();
  PartitionedCache(const PartitionedCache&) = delete;
  PartitionedCache& operator=(const PartitionedCache&) = delete;
  void put(const std::string& key, const std::string& value);
  std::optional<std::string> get(const std::string& key) const;
  bool erase(const std::string& key);
  std::size_t size() const;
  std::size_t partition_count() const noexcept;
  std::size_t partition_for(const std::string& key) const;

 private:
  struct Partition {
    Partition();
    ~Partition();
    mutable pthread_rwlock_t lock;
    std::unordered_map<std::string, std::string> entries;
  };
  std::vector<Partition*> partitions_;
};
