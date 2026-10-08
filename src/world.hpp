// Мир: предметы, иерархия установки (слоты), физика, модели комнаты и деталей
#pragma once
#include "core.hpp"
#include "parts.hpp"
#include <memory>
#include <string>
#include <vector>

struct Entity {
    int id = 0;
    const PartDef* def = nullptr;
    // Свободный предмет
    Vec3 pos{0, 0, 0};    // центр нижней грани габарита
    int yaw = 0;          // поворот шагами по 90°
    float velY = 0;
    bool resting = false;
    bool held = false;
    bool inBox = false;   // ещё запакован в коробку доставки
    // Установка в другой предмет
    int parent = 0;
    int slot = -1;
    // Состояние
    bool osInstalled = false;  // накопитель: установлена NovaOS
    bool pasted = false;       // процессор: нанесена термопаста
    bool broken = false;       // сгорел
    float quality = 0.5f;      // «кремниевая лотерея» 0..1 — запас для разгона
    int uses = 0;              // остаток термопасты
};

struct SlotDef {
    Cat accepts = Cat::Count;
    bool m2 = false;           // для накопителей: слот M.2 (иначе SATA-отсек)
    Mat4 local = identity();
    const char* name = "";
};

struct Surface {               // горизонтальная поверхность, на которую можно класть
    float x0, x1, z0, z1, y;
};

struct Solid {                 // препятствие для игрока
    float x0, x1, z0, z1;
};

// Модельные размеры (метры)
namespace dims {
constexpr float caseW = 0.22f, caseH = 0.47f, caseD = 0.45f;
}

class World {
public:
    std::vector<std::unique_ptr<Entity>> ents;
    int nextId = 1;
    std::vector<Surface> surfaces;
    std::vector<Solid> solids;

    // Комната
    float roomX0 = -4, roomX1 = 4, roomZ0 = -3, roomZ1 = 3, roomH = 3;
    Vec3 monitorPos{-1.1f, 0.775f, -2.8f};   // центр подставки на столе
    Vec3 screenCenter;
    float screenW = 0.62f, screenH = 0.3875f;
    BBox monitorBox;
    Rect deliveryZone{-3.6f, 1.55f, 1.25f, 1.2f};  // x, z, ширина, глубина (на полу)
    Rect shipZone{-2.05f, 2.0f, 1.2f, 0.85f};      // поддон для отправки/продажи
    float shipY = 0.14f;

    World();
    void clear();
    Entity* get(int id);
    Entity& spawn(const PartDef* def, Vec3 pos, bool inBox);
    void remove(int id);                        // вместе со всем, что установлено внутрь
    std::vector<Entity*> children(int id);
    void descendants(int id, std::vector<Entity*>& out);
    Entity* rootOf(Entity* e);
    Entity* childInSlot(int parentId, int slot);

    Mat4 worldMatrix(const Entity& e);
    BBox localBounds(const Entity& e) const;
    BBox worldBounds(const Entity& e);
    std::vector<BBox> hitBoxes(const Entity& e);  // в мировых координатах

    static std::vector<SlotDef> slotsFor(const PartDef* def);
    Mat4 slotWorld(const Entity& parent, int slot);

    void update(float dt);
    void wakeAll();
    float supportHeight(const BBox& b, int ignoreId);
    bool inZone(const Rect& zone, const Entity& e);

    // Отрисовка
    void drawRoom(float time, bool lampOn);
    void drawMonitorBody();
    void drawEntity(Entity& e, float time, bool powered, Color rgb, float fanSpeed);
    void drawEntityAt(const Entity& e, const Mat4& m, float time);   // «призрак» при установке
    void drawGlass(Entity& e);
    void drawFan(float time, bool spinning, bool rgbOn, Color rgb, Color frame);
    Mat4 screenCanvasMatrix(float canvasW, float canvasH);

private:
    void drawPart(const Entity& e, float time, bool powered, Color rgb, float fanSpeed);
    void drawPacked(const Entity& e);
};

Mat4 restMatrix(const PartDef* d);   // ориентация свободной детали
Vec3 boxSize(const PartDef* d);      // размер коробки доставки
float gpuLength(const PartDef* d);
