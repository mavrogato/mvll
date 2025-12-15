
#include <iostream>
#include <sycl/sycl.hpp>

int main() {
    sycl::queue q;
    std::cout << q.get_device().get_info<sycl::info::device::name>() << std::endl;
    return 0;
}
