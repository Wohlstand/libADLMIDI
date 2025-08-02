// A test program to verify C++ compiler for C++14 support
// Based on The Javatar's example: https://stackoverflow.com/questions/30940504/simplest-c-example-to-verify-my-compiler-is-c14-compliant

#include <iostream>
#include <tuple>
#include <cstdint>
#include <assert.h>
#include <functional>

class TestClass
{
public:
    static constexpr int test_constant = 42;

    static constexpr int  test_funk(uint32_t chnum)
    {
        assert(chnum >= 42);
        return chnum;
    }
};

auto f() // this function returns multiple values
{
    int x = 5;
    return std::make_tuple(x, 7); // not "return {x,7};" because the corresponding
                                  // tuple constructor is explicit (LWG 2051)
}

int main()
{
    // heterogeneous tuple construction
    int n = 1;
    TestClass tc;
    auto t = std::make_tuple(10, "Test", 3.14, std::ref(n), n);
    n = 7;
    std::cout << "The value of t is "  << "("
              << std::get<0>(t) << ", " << std::get<1>(t) << ", "
              << std::get<2>(t) << ", " << std::get<3>(t) << ", "
              << std::get<4>(t) << ")\n";

    int q = tc.test_funk(40);

    // function returning multiple values
    int a, b;
    std::tie(a, b) = f();
    std::cout << a << " " << b << "\n";

    return 0;
}
