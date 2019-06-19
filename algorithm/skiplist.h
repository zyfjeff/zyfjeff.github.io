#pragma once

#include <memory>

template <typename K, typename E>
struct SkipNode {
  using PairType = std::pair<const K, E>;

  PairType element_;
  std::unique_ptr<SkipNode<K, E>[]> next_;
  SkipNode(const PairType& pair_data, int size)
    : element_(pair_data) {
    next_ = std::make_unique<SkipNode<K, E>[]>(size);
  }
};

template <typename K, typename E>
class SkipList {
 public:
  SkipList(K large_key, int max_pairs, float prob) {
    cut_off_ = prob * RAND_MAX;
  }
 private:
  float cut_off_;
  int levels_;
  int d_size_;
  int max_level_;
  K tail_key_;  // 最大关键字
  std::unique_ptr<SkipNode<K, E>> header_node_;
  std::unique_ptr<SkipNode<K, E>> tail_node_;
  std::unique_ptr<SkipNode<K, E>[]> last_;
};