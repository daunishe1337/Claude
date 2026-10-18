#include <cstdlib>

int main()
{
    // Открываем новое окно CMD, которое остаётся открытым (/K),
    // переключаем кодовую страницу на UTF-8 (chcp 65001),
    // чтобы кириллица отображалась правильно, и выводим текст.
    std::system("start cmd /K \"chcp 65001 > nul && echo \xD0\xA5\xD0\xB0\xD1\x85\xD0\xB0\xD1\x85\xD0\xB0\"");
    return 0;
}
