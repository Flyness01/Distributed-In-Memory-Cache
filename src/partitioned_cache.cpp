#include "partitioned_cache.hpp"
#include <functional>
#include <stdexcept>

PartitionedCache::Partition::Partition() {
  if (pthread_rwlock_init(&lock, nullptr) != 0) throw std::runtime_error("failed to initialize partition lock");
}
PartitionedCache::Partition::~Partition() { pthread_rwlock_destroy(&lock); }

PartitionedCache::PartitionedCache(std::size_t count) {
  if (count == 0) throw std::invalid_argument("partition_count must be greater than zero");
  partitions_.reserve(count);
  try { for (std::size_t i = 0; i < count; ++i) partitions_.push_back(new Partition()); }
  catch (...) { for (auto* partition : partitions_) delete partition; throw; }
}
PartitionedCache::~PartitionedCache() { for (auto* partition : partitions_) delete partition; }

std::size_t PartitionedCache::partition_for(const std::string& key) const {
  return std::hash<std::string>{}(key) % partitions_.size();
}
void PartitionedCache::put(const std::string& key, const std::string& value) {
  auto* partition = partitions_[partition_for(key)];
  pthread_rwlock_wrlock(&partition->lock);
  partition->entries[key] = value;
  pthread_rwlock_unlock(&partition->lock);
}
std::optional<std::string> PartitionedCache::get(const std::string& key) const {
  auto* partition = partitions_[partition_for(key)];
  pthread_rwlock_rdlock(&partition->lock);
  const auto found = partition->entries.find(key);
  std::optional<std::string> result;
  if (found != partition->entries.end()) result = found->second;
  pthread_rwlock_unlock(&partition->lock);
  return result;
}
bool PartitionedCache::erase(const std::string& key) {
  auto* partition = partitions_[partition_for(key)];
  pthread_rwlock_wrlock(&partition->lock);
  const bool removed = partition->entries.erase(key) > 0;
  pthread_rwlock_unlock(&partition->lock);
  return removed;
}
std::size_t PartitionedCache::size() const {
  std::size_t total = 0;
  for (auto* partition : partitions_) {
    pthread_rwlock_rdlock(&partition->lock);
    total += partition->entries.size();
    pthread_rwlock_unlock(&partition->lock);
  }
  return total;
}
std::size_t PartitionedCache::partition_count() const noexcept { return partitions_.size(); }
