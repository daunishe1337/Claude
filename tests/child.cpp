// Тестовая «программа пользователя» для selftest: проверяет потоковый вывод, stdin, stderr, коды выхода
#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    std::string mode = argc > 1 ? argv[1] : "echo";
    if (mode == "echo") {
        std::cout << "Enter your name: ";               // без \n — должно прийти сразу
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        std::string name;
        std::cin >> name;
        std::cout << "Access granted, " << name << ".\n";
        std::cerr << "warn on stderr\n";
        return 7;
    }
    if (mode == "hang") {
        for (;;) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (mode == "crash") {
        volatile int* p = nullptr;
        *p = 42;
    }
    if (mode == "utf8") {
        std::cout << "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82\n";  // «Привет» в UTF-8
        return 0;
    }
    return 0;
}
