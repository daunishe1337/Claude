// Orders.cpp
#include "Orders.h"

OrderBook::OrderBook() {
    orders_.push_back({ 0, L"21:47", L"Особняк Ворониных", L"Северная аллея, 7",
        L"Я знаю, что случилось с вашим домом. Не спрашивайте откуда.\n\nРабота есть. Особняк Ворониных, Северная аллея, 7. Хозяина нашли мёртвым в кабинете. Приезжайте сейчас. Они уже ждут.",
        OrderStatus::Hidden });
    orders_.push_back({ 1, L"23:15", L"Офис «Вектор-Групп»", L"Промзона, 4 этаж",
        L"Офис «Вектор-Групп», четвёртый этаж. Ночью в здании кто-то остался. Охранник не отвечает уже два часа. Приезжайте один. Не включайте свет в серверной.",
        OrderStatus::Hidden });
    orders_.push_back({ 2, L"01:03", L"Дом на Тихой", L"Тихая улица, 13",
        L"Дом на Тихой улице, 13. Хозяев нет три дня. Соседи слышат шаги на втором этаже. Каждую ночь в одно и то же время.",
        OrderStatus::Hidden });
    orders_.push_back({ 3, L"03:33", L"Квартира 42", L"Заречная, 9, кв. 42",
        L"Заречная, 9, квартира 42. Дверь открыта. Свет не горит. Вы уже были в этой квартире. Вспомните.",
        OrderStatus::Hidden });
}

const wchar_t* OrderBook::SenderNumber() { return L"+7 834 747 23 63"; }

bool OrderBook::HasAvailable() const {
    for (const Order& o : orders_) if (o.status == OrderStatus::Available) return true;
    return false;
}

bool OrderBook::HasHidden() const {
    for (const Order& o : orders_) if (o.status == OrderStatus::Hidden) return true;
    return false;
}

bool OrderBook::AllDone() const {
    for (const Order& o : orders_) if (o.status != OrderStatus::Done) return false;
    return true;
}

bool OrderBook::RevealNext() {
    for (Order& o : orders_) {
        if (o.status == OrderStatus::Hidden) { o.status = OrderStatus::Available; return true; }
    }
    return false;
}

void OrderBook::Complete(int index) {
    if (index >= 0 && index < Count()) orders_[index].status = OrderStatus::Done;
}

std::vector<int> OrderBook::VisibleIndices() const {
    std::vector<int> v;
    for (int i = 0; i < Count(); ++i)
        if (orders_[i].status != OrderStatus::Hidden) v.push_back(i);
    return v;
}
