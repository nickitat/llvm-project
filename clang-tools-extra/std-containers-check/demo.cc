#include <vector>

namespace ns {

///
/// Negative cases without 'allow-std-containers' annotation.
///

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

///
/// Positive cases with 'allow-std-containers' annotation.
///

void test4() {
  [[maybe_unused]] const std::vector<int> *v; // pls allow-std-containers
}

void test4(const std::vector<float> *v) // allow-std-containers
{}

void test4(int a, char b,
           float f,                      //
           const std::vector<double> &v, // allow-std-containers
           double d) {}

} // namespace ns
