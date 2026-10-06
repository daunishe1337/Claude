#pragma once
// Orders.h - система заказов: сообщения приходят на телефон героя.
#include <string>
#include <vector>

enum class OrderStatus { Hidden, Available, Done };

struct Order {
    int mapId;            // какая локация (см. Map::Load)
    std::wstring time;    // время сообщения
    std::wstring title;   // короткое название
    std::wstring address; // адрес
    std::wstring text;    // текст сообщения
    OrderStatus status = OrderStatus::Hidden;
};

class OrderBook {
public:
    OrderBook();

    // Номер, с которого приходят заказы (в игре - просто текст на экране телефона)
    static const wchar_t* SenderNumber();

    int Count() const { return static_cast<int>(orders_.size()); }
    Order& At(int i) { return orders_[i]; }
    const Order& At(int i) const { return orders_[i]; }

    bool HasAvailable() const;
    bool HasHidden() const;
    bool AllDone() const;
    bool RevealNext();         // открывает следующий скрытый заказ
    void Complete(int index);

    // Индексы заказов, которые уже пришли (Available или Done)
    std::vector<int> VisibleIndices() const;

private:
    std::vector<Order> orders_;
};
