#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"

#include <map>
#include <set>
#include <string>


int main() {
  // 默认构造
  absl::btree_set<std::string> set1;
  absl::btree_map<int, std::string> map1;

  // 初始化
  absl::btree_set<std::string> set2 = {{"huey"}, {"dewey"}, {"louie"},};
  absl::btree_map<int, std::string> map2 = {{1, "huey"}, {2, "dewey"}, {3, "louie"}, };

  // 拷贝构造
  absl::btree_set<std::string> set3(set2);
  absl::btree_map<int, std::string> map3(map2);

  // 赋值构造
  absl::btree_set<std::string> set4;
  set4 = set3;
  absl::btree_map<int, std::string> map4;
  map4 = map3;

  // 移动构造
  absl::btree_set<std::string> set5(std::move(set4));
  absl::btree_map<int, std::string> map5(std::move(map4));

  // 移动赋值构造
  absl::btree_set<std::string> set6;
  set6 = std::move(set5);
  absl::btree_map<int, std::string> map6;
  map6 = std::move(map5);

  // Range 构造
  std::vector<std::string> v = {"a", "b"};
  absl::btree_set<std::string> set7(v.begin(), v.end());

  std::vector<std::pair<int, std::string>> v2 = {{1, "a"}, {2, "b"}};
  absl::btree_map<int, std::string> map7(v2.begin(), v2.end());

  return 0;
}
