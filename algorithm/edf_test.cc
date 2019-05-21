#include "edf.h"
#include <cassert>
#include <iostream>

int main() {
  EdfScheduler<uint32_t> sched;
  constexpr uint32_t num_entries = 128;
  std::shared_ptr<uint32_t> entries[num_entries];
  uint32_t pick_count[num_entries];

  for (uint32_t i = 0; i < num_entries; ++i) {
    entries[i] = std::make_shared<uint32_t>(i);
    sched.add(i + 1, entries[i]);
    pick_count[i] = 0;
  }

  for (uint32_t i = 0; i < num_entries * (num_entries + 1) / 2; ++i) {
    auto p = sched.pick();
    ++pick_count[*p];
    sched.add(*p + 1, p);
  }

  for (uint32_t i = 0; i < num_entries; ++i) {
    assert((i + 1) == pick_count[i]);
  }


  uint32_t pick_count2[3];
  EdfScheduler<uint32_t> sched2;
  sched2.add(5, entries[4]);
  sched2.add(1, entries[0]);
  sched2.add(1, entries[0]);

  for(uint32_t i = 0; i < 6; ++i) {
    auto p = sched2.pick();
    std::cout << *p + 1 << std::endl;
    sched2.add(*p + 1, p);
  }
}
