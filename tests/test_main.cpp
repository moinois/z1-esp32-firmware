// Runs every statically registered host-side unit test.
#include "test.hpp"

int main() {
    int failures = 0;
    for (const auto& item : test::cases()) {
        try {
            item.second();
            std::cout << "PASS " << item.first << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "FAIL " << item.first << ": " << error.what() << '\n';
        }
    }
    std::cout << test::cases().size() - failures << '/' << test::cases().size() << " tests passed\n";
    return failures == 0 ? 0 : 1;
}
