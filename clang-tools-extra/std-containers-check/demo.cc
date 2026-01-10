#include <vector>

namespace ns {

using Vec = std::vector<int>;

typedef std::vector<int> OldStyleVec;

template <typename T> using VecT = std::vector<T>;

using VecAlias = VecT<int>;

class C {
private:
  std::vector<double> mem_v;
};

void test() { std::vector<int> v; }

void test1(const std::vector<int> v) {}

void test1(const std::vector<int> &v) {}

void test1(const std::vector<int> *v) {}

void test2() {
  auto f = [&](const std::vector<float> &v) { return v.size(); };

  f({1.0f, 2.0f, 3.0f});
}

void test3() {
  auto f = [&](const auto &v) { return v.size(); };

  f(std::vector{1.0f, 2.0f, 3.0f});
}

} // namespace ns
