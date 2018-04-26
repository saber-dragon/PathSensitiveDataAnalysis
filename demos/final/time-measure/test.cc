#include <iostream>
#include <chrono>


extern int foo(int);

int main(){
    size_t T = size_t(1e9);
    // auto start = std::chrono::steady_clock::now();
    // for (size_t k = 0;k < T;++ k);
    // auto end = std::chrono::steady_clock::now();
    // auto duration = std::chrono::duration_cast<std::chrono::microseconds> 
    //                         (end - start);

    // std::cout << "Dummy Loop : " << duration.count() << std::endl;
    auto start = std::chrono::steady_clock::now();
    for (size_t k = 0;k < T;++ k) foo(100);
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds> 
                            (end - start);
    std::cout << "Call foo : " << 1.0 * duration.count() / T << " us" << std::endl;

    return 0;
}