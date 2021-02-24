#include <chrono>
#include <condition_variable>
#include <mutex>
#include <utility>
#include <vector>


struct closed_queue : std::exception {};

template <typename T, typename Clock = std::chrono::system_clock>
class delay_queue {
public:
  using value_type = T;
  using time_point = typename Clock::time_point;

  delay_queue() = default;
  explicit delay_queue(std::size_t initial_capacity)
    { q_.reserve(initial_capacity); }
  ~delay_queue() = default;

  delay_queue(const delay_queue&) = delete;
  delay_queue& operator=(const delay_queue&) = delete;

  void enqueue(value_type v, time_point tp)
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    if (closed_)
      throw closed_queue{};
    q_.emplace_back(std::move(v), tp);
    // descending sort on time_point
    std::sort(begin(q_), end(q_), [](auto&& a, auto&& b) { return a.second > b.second; });
    cv_.notify_one();
  }

  value_type dequeue()
  {
    std::unique_lock<decltype(mtx_)> lk(mtx_);
    auto now = Clock::now();
    // wait condition: (empty && closed) || (!empty && back.tp <= now)
    while (!(q_.empty() && closed_) && !(!q_.empty() && q_.back().second <= now)) {
      if (q_.empty())
        cv_.wait(lk);
      else
        cv_.wait_until(lk, q_.back().second);
      now = Clock::now();
    }
    if (q_.empty() && closed_)
      return {};  // invalid value
    value_type ret = std::move(q_.back().first);
    q_.pop_back();
    if (q_.empty() && closed_)
      cv_.notify_all();
    return ret;
  }

  void close()
  {
    std::lock_guard<decltype(mtx_)> lk(mtx_);
    closed_ = true;  
    cv_.notify_all();
  }

private:
  std::vector<std::pair<value_type, time_point>> q_;
  bool closed_ = false;
  std::mutex mtx_;
  std::condition_variable cv_;
};


#if 1
#include <cassert>
#include <future>
#include <iostream>
#include <memory>
#include <string>

template <typename T>
void dump(const T& v, std::chrono::system_clock::time_point epoch)
{
  using namespace std::chrono;
  auto elapsed = duration_cast<milliseconds>(system_clock::now() - epoch).count() / 1000.;
  std::cout << elapsed << ":" << v << std::endl;
}

int main()
{
  auto base = std::chrono::system_clock::now();
  constexpr std::size_t capacity = 5;
  delay_queue<std::unique_ptr<std::string>> q{capacity};

  auto f = std::async(std::launch::async, [&q, base] {
    using namespace std::chrono_literals;
    q.enqueue(std::make_unique<std::string>("two"),   base + 2s);
    q.enqueue(std::make_unique<std::string>("three"), base + 3s);
    q.enqueue(std::make_unique<std::string>("one"),   base + 1s);
    q.close();
    try {
      q.enqueue(std::make_unique<std::string>("ng"), base);  // operation shall fail
      assert(!"not reached");
    } catch (closed_queue) {}
  });

  dump("start", base);
  dump(*q.dequeue(), base);  // "one"
  dump(*q.dequeue(), base);  // "two"
  dump(*q.dequeue(), base);  // "three"
  assert(q.dequeue() == nullptr);  // end of data
}
#endif