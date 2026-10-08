#include "world.hpp"
#include "render.hpp"
#include <algorithm>

using gfx::box;
using gfx::cylinder;

namespace {
const Color kGold = rgba(212, 175, 55);
const Color kSilver = rgba(195, 198, 205);
const Color kDark = rgba(26, 26, 30);
const Color kCopper = rgba(196, 122, 72);

BBox bb(float x0, float y0, float z0, float x1, float y1, float z1) { return BBox{v3(x0, y0, z0), v3(x1, y1, z1)}; }

BBox transformBox(const Mat4& m, const BBox& b) {
    BBox r{v3(1e9f, 1e9f, 1e9f), v3(-1e9f, -1e9f, -1e9f)};
    for (int i = 0; i < 8; i++) {
        Vec3 p = xformPoint(m, v3((i & 1) ? b.max.x : b.min.x, (i & 2) ? b.max.y : b.min.y,
                                  (i & 4) ? b.max.z : b.min.z));
        r.min = v3(std::min(r.min.x, p.x), std::min(r.min.y, p.y), std::min(r.min.z, p.z));
        r.max = v3(std::max(r.max.x, p.x), std::max(r.max.y, p.y), std::max(r.max.z, p.z));
    }
    return r;
}

float gpuThick(const PartDef* d) {
    if (d->id == "gpu_4090" || d->id == "gpu_4080") return 0.06f;
    if (d->id == "gpu_4070" || d->id == "gpu_3080") return 0.05f;
    return 0.04f;
}

float gpuHeight(const PartDef* d) {
    if (d->id == "gpu_4090") return 0.13f;
    if (d->id == "gpu_4080") return 0.125f;
    return 0.11f;
}

BBox modelBounds(const PartDef* d) {
    using namespace dims;
    switch (d->cat) {
        case Cat::Case: return bb(-caseW / 2, 0, -caseD / 2, caseW / 2, caseH, caseD / 2);
        case Cat::Motherboard: return bb(-0.15f, 0, -0.122f, 0.15f, 0.03f, 0.122f);
        case Cat::CPU: return bb(-0.02f, 0, -0.02f, 0.02f, 0.004f, 0.02f);
        case Cat::Cooler:
            switch (d->coolerKind) {
                case 0: return bb(-0.045f, 0, -0.045f, 0.045f, 0.056f, 0.045f);
                case 1: return bb(-0.0625f, 0, -0.03f, 0.0625f, 0.16f, 0.05f);
                case 2: return bb(-0.0625f, 0, -0.05f, 0.0625f, 0.165f, 0.05f);
                default: return bb(-0.03f, 0, -0.03f, 0.03f, 0.04f, 0.03f);
            }
        case Cat::RAM: return bb(-0.0665f, 0, -0.004f, 0.0665f, 0.036f, 0.004f);
        case Cat::GPU: {
            float L = gpuLength(d), H = gpuHeight(d), T = gpuThick(d);
            return bb(-0.003f, -0.006f, -0.002f, T + 0.003f, H + 0.01f, L);
        }
        case Cat::PSU: return bb(-0.075f, 0, -0.072f, 0.075f, 0.086f, 0.08f);
        case Cat::Storage:
            if (d->stor == StorType::HDD) return bb(-0.05f, 0, -0.0735f, 0.05f, 0.026f, 0.0735f);
            if (d->stor == StorType::SATA_SSD) return bb(-0.035f, 0, -0.05f, 0.035f, 0.007f, 0.05f);
            return bb(-0.011f, 0, -0.04f, 0.011f, 0.003f, 0.04f);
        case Cat::Fan: return bb(-0.06f, 0, -0.06f, 0.06f, 0.025f, 0.06f);
        case Cat::Paste: return bb(-0.008f, 0, -0.05f, 0.008f, 0.016f, 0.05f);
        case Cat::USB: return bb(-0.009f, 0, -0.025f, 0.009f, 0.008f, 0.025f);
        default: return bb(-0.05f, 0, -0.05f, 0.05f, 0.1f, 0.05f);
    }
}

void drawFrameBars(Color c) {
    box(v3(0, 0.0125f, 0.0575f), v3(0.12f, 0.025f, 0.005f), c);
    box(v3(0, 0.0125f, -0.0575f), v3(0.12f, 0.025f, 0.005f), c);
    box(v3(0.0575f, 0.0125f, 0), v3(0.005f, 0.025f, 0.11f), c);
    box(v3(-0.0575f, 0.0125f, 0), v3(0.005f, 0.025f, 0.11f), c);
}

// Крыльчатка (ось Y), радиус r
void impeller(float r, float h, float angle, Color blade, Color hub, int blades = 7) {
    cylinder(v3(0, 0, 0), r * 0.32f, h, hub, 10);
    for (int i = 0; i < blades; i++) {
        gfx::push(rotY(angle + i * 360.0f / blades));
        gfx::push(translate(r * 0.62f, h * 0.5f, 0) * rotX(28));
        box(v3(0, 0, 0), v3(r * 0.66f, h * 0.18f, r * 0.42f), blade);
        gfx::pop();
        gfx::pop();
    }
}
}  // namespace

float gpuLength(const PartDef* d) {
    static const struct {
        const char* id;
        float len;
    } t[] = {{"gpu_750ti", 0.17f}, {"gpu_1050ti", 0.18f}, {"gpu_1660s", 0.22f}, {"gpu_2060", 0.23f},
             {"gpu_3060", 0.24f},  {"gpu_3080", 0.28f},   {"gpu_4040", 0.19f},  {"gpu_4050", 0.20f},
             {"gpu_4060", 0.24f},  {"gpu_4070", 0.27f},   {"gpu_4080", 0.29f},  {"gpu_4090", 0.30f}};
    for (auto& e : t)
        if (d->id == e.id) return e.len;
    return 0.24f;
}

Mat4 restMatrix(const PartDef* d) {
    if (d->cat == Cat::RAM) return rotX(-90);
    if (d->cat == Cat::GPU) return rotZ(90);
    return identity();
}

Vec3 boxSize(const PartDef* d) {
    switch (d->cat) {
        case Cat::Case: return v3(0.32f, 0.56f, 0.54f);
        case Cat::Motherboard: return v3(0.34f, 0.08f, 0.30f);
        case Cat::CPU: return v3(0.12f, 0.07f, 0.12f);
        case Cat::Cooler: return d->coolerKind == 3 ? v3(0.42f, 0.15f, 0.18f) : v3(0.18f, 0.18f, 0.16f);
        case Cat::RAM: return v3(0.17f, 0.04f, 0.06f);
        case Cat::GPU: return v3(0.38f, 0.12f, 0.18f);
        case Cat::PSU: return v3(0.22f, 0.14f, 0.22f);
        case Cat::Storage: return v3(0.16f, 0.05f, 0.12f);
        case Cat::Fan: return v3(0.15f, 0.05f, 0.15f);
        case Cat::Paste: return v3(0.06f, 0.03f, 0.12f);
        case Cat::USB: return v3(0.06f, 0.025f, 0.09f);
        default: return v3(0.2f, 0.2f, 0.2f);
    }
}

// ---------------------------------------------------------------------------

World::World() {
    // Поверхности: пол, стол с монитором, верстак, полки стеллажа, поддон, стул
    surfaces = {
        {roomX0, roomX1, roomZ0, roomZ1, 0.0f},
        {-2.0f, 0.0f, -3.0f, -2.2f, 0.775f},
        {0.7f, 2.5f, -3.0f, -2.2f, 0.78f},
        {3.35f, 3.95f, -1.2f, 0.6f, 0.465f},
        {3.35f, 3.95f, -1.2f, 0.6f, 0.965f},
        {3.35f, 3.95f, -1.2f, 0.6f, 1.465f},
        {shipZone.x, shipZone.x + shipZone.w, shipZone.y, shipZone.y + shipZone.h, shipY},
    };
    solids = {
        {-2.0f, 0.0f, -3.0f, -2.2f},     {0.7f, 2.5f, -3.0f, -2.2f},   {3.35f, 3.95f, -1.2f, 0.6f},
        {shipZone.x, shipZone.x + shipZone.w, shipZone.y, shipZone.y + shipZone.h},
        {3.35f, 3.85f, 2.25f, 2.75f},
    };
    float cy = monitorPos.y + 0.40f;
    screenCenter = v3(monitorPos.x, cy + 0.01f, monitorPos.z + 0.0185f);
    monitorBox = BBox{v3(monitorPos.x - 0.33f, monitorPos.y, monitorPos.z - 0.05f),
                      v3(monitorPos.x + 0.33f, cy + 0.215f, monitorPos.z + 0.02f)};
}

void World::clear() {
    ents.clear();
    nextId = 1;
}

Entity* World::get(int id) {
    if (id <= 0) return nullptr;
    for (auto& e : ents)
        if (e->id == id) return e.get();
    return nullptr;
}

Entity& World::spawn(const PartDef* def, Vec3 pos, bool inBox) {
    auto e = std::make_unique<Entity>();
    e->id = nextId++;
    e->def = def;
    e->pos = pos;
    e->inBox = inBox;
    e->quality = rndf();
    e->uses = def->uses;
    ents.push_back(std::move(e));
    return *ents.back();
}

void World::remove(int id) {
    std::vector<Entity*> del;
    descendants(id, del);
    std::vector<int> ids{id};
    for (Entity* d : del) ids.push_back(d->id);
    ents.erase(std::remove_if(ents.begin(), ents.end(),
                              [&](const std::unique_ptr<Entity>& e) {
                                  return std::find(ids.begin(), ids.end(), e->id) != ids.end();
                              }),
               ents.end());
    wakeAll();
}

std::vector<Entity*> World::children(int id) {
    std::vector<Entity*> out;
    for (auto& e : ents)
        if (e->parent == id) out.push_back(e.get());
    return out;
}

void World::descendants(int id, std::vector<Entity*>& out) {
    for (auto& e : ents)
        if (e->parent == id) {
            out.push_back(e.get());
            descendants(e->id, out);
        }
}

Entity* World::rootOf(Entity* e) {
    while (e && e->parent) e = get(e->parent);
    return e;
}

Entity* World::childInSlot(int parentId, int slot) {
    for (auto& e : ents)
        if (e->parent == parentId && e->slot == slot) return e.get();
    return nullptr;
}

BBox World::localBounds(const Entity& e) const {
    if (e.inBox) {
        Vec3 s = boxSize(e.def);
        return bb(-s.x / 2, 0, -s.z / 2, s.x / 2, s.y, s.z / 2);
    }
    return modelBounds(e.def);
}

Mat4 World::worldMatrix(const Entity& e) {
    if (e.parent) {
        Entity* p = get(e.parent);
        if (p) return slotWorld(*p, e.slot);
    }
    Mat4 rest = e.inBox ? identity() : restMatrix(e.def);
    BBox rb = transformBox(rest, localBounds(e));
    Vec3 off = v3(-(rb.min.x + rb.max.x) * 0.5f, -rb.min.y, -(rb.min.z + rb.max.z) * 0.5f);
    return translate(e.pos.x, e.pos.y, e.pos.z) * rotY(e.yaw * 90.0f) * translate(off.x, off.y, off.z) * rest;
}

BBox World::worldBounds(const Entity& e) { return transformBox(worldMatrix(e), localBounds(e)); }

std::vector<BBox> World::hitBoxes(const Entity& e) {
    std::vector<BBox> out;
    Mat4 m = worldMatrix(e);
    if (e.def->cat == Cat::Case && !e.inBox) {
        using namespace dims;
        // Боковая стенка со стороны +X открыта/стеклянная — через неё видно и достаём детали
        out.push_back(transformBox(m, bb(-caseW / 2, 0, -caseD / 2, -caseW / 2 + 0.012f, caseH, caseD / 2)));
        out.push_back(transformBox(m, bb(-caseW / 2, caseH - 0.012f, -caseD / 2, caseW / 2, caseH, caseD / 2)));
        out.push_back(transformBox(m, bb(-caseW / 2, 0, -caseD / 2, caseW / 2, 0.012f, caseD / 2)));
        out.push_back(transformBox(m, bb(-caseW / 2, 0, caseD / 2 - 0.012f, caseW / 2, caseH, caseD / 2)));
        out.push_back(transformBox(m, bb(-caseW / 2, 0, -caseD / 2, caseW / 2, caseH, -caseD / 2 + 0.012f)));
        return out;
    }
    BBox b = transformBox(m, localBounds(e));
    // Совсем мелкие детали чуть увеличим, чтобы в них было проще попасть
    const float minSize = 0.02f;
    if (b.max.x - b.min.x < minSize) b.min.x -= 0.006f, b.max.x += 0.006f;
    if (b.max.y - b.min.y < minSize) b.min.y -= 0.004f, b.max.y += 0.006f;
    if (b.max.z - b.min.z < minSize) b.min.z -= 0.006f, b.max.z += 0.006f;
    out.push_back(b);
    return out;
}

std::vector<SlotDef> World::slotsFor(const PartDef* d) {
    std::vector<SlotDef> s;
    if (d->cat == Cat::Case) {
        using namespace dims;
        s.push_back({Cat::Motherboard, false, translate(-0.10f, 0.28f, -0.02f) * rotZ(-90), "материнская плата"});
        s.push_back({Cat::PSU, false, translate(0, 0.012f, -0.13f), "блок питания"});
        s.push_back({Cat::Storage, false, translate(0, 0.012f, 0.115f), "SATA-отсек 1"});
        s.push_back({Cat::Storage, false, translate(0, 0.05f, 0.115f), "SATA-отсек 2"});
        s.push_back({Cat::USB, false, translate(0.06f, 0.43f, caseD / 2 + 0.009f), "USB-порт"});
        for (int i = 0; i < d->fanSlots; i++) {
            float y = d->fanSlots == 1 ? 0.265f : 0.14f + 0.125f * i;
            s.push_back({Cat::Fan, false, translate(0, y, 0.21f) * rotX(-90), "вентилятор"});
        }
    } else if (d->cat == Cat::Motherboard) {
        s.push_back({Cat::CPU, false, translate(-0.06f, 0.008f, -0.03f), "сокет процессора"});
        s.push_back({Cat::Cooler, false, translate(-0.06f, 0.012f, -0.03f), "крепление кулера"});
        s.push_back({Cat::GPU, false, translate(0.035f, 0.012f, -0.11f), "PCIe x16"});
        for (int i = 0; i < d->ramSlots; i++) {
            float z = d->ramSlots == 2 ? 0.045f + 0.024f * i : 0.04f + 0.012f * i;
            s.push_back({Cat::RAM, false, translate(-0.04f, 0.008f, z), "слот ОЗУ"});
        }
        for (int i = 0; i < d->m2Slots; i++) {
            float x = i == 0 ? 0.005f : 0.11f;
            s.push_back({Cat::Storage, true, translate(x, 0.004f, -0.03f), "слот M.2"});
        }
    }
    return s;
}

Mat4 World::slotWorld(const Entity& parent, int slot) {
    auto s = slotsFor(parent.def);
    Mat4 pw = worldMatrix(parent);
    if (slot < 0 || slot >= (int)s.size()) return pw;
    return pw * s[slot].local;
}

// ---------------------------------------------------------------------------
// Физика: свободные предметы падают и ложатся на поверхности и друг на друга

void World::wakeAll() {
    for (auto& e : ents)
        if (!e->parent) e->resting = false;
}

float World::supportHeight(const BBox& b, int ignoreId) {
    const float shrink = 0.01f, tol = 0.03f;
    float best = 0.0f;
    for (auto& s : surfaces) {
        if (b.max.x - shrink < s.x0 || b.min.x + shrink > s.x1 || b.max.z - shrink < s.z0 ||
            b.min.z + shrink > s.z1)
            continue;
        if (s.y <= b.min.y + tol) best = std::max(best, s.y);
    }
    for (auto& up : ents) {
        Entity& o = *up;
        if (o.id == ignoreId || o.parent || o.held) continue;
        BBox ob = worldBounds(o);
        if (b.max.x - shrink < ob.min.x || b.min.x + shrink > ob.max.x || b.max.z - shrink < ob.min.z ||
            b.min.z + shrink > ob.max.z)
            continue;
        if (ob.max.y <= b.min.y + tol) best = std::max(best, ob.max.y);
    }
    return best;
}

void World::update(float dt) {
    for (auto& up : ents) {
        Entity& e = *up;
        if (e.parent || e.held || e.resting) continue;
        e.velY -= 9.8f * dt;
        e.pos.y += e.velY * dt;
        BBox b = worldBounds(e);
        float sup = supportHeight(b, e.id);
        if (b.min.y <= sup) {
            e.pos.y += sup - b.min.y;
            e.velY = 0;
            e.resting = true;
        }
        // Не даём вылететь за стены
        e.pos.x = clampf(e.pos.x, roomX0 + 0.15f, roomX1 - 0.15f);
        e.pos.z = clampf(e.pos.z, roomZ0 + 0.15f, roomZ1 - 0.15f);
        if (e.pos.y < -1) e.pos.y = 1;
    }
}

bool World::inZone(const Rect& z, const Entity& e) {
    if (e.parent || e.held) return false;
    return e.pos.x >= z.x && e.pos.x <= z.x + z.w && e.pos.z >= z.y && e.pos.z <= z.y + z.h;
}

// ---------------------------------------------------------------------------
// Комната

void World::drawRoom(float time, bool lampOn) {
    (void)time;
    gfx::resetXform();
    // Пол: доски
    for (int i = 0; i < 20; i++) {
        float z = roomZ0 + i * 0.3f + 0.15f;
        Color c = (i % 2) ? rgba(152, 112, 76) : rgba(142, 104, 70);
        if (i % 5 == 3) c = rgba(158, 118, 80);
        box(v3(0, -0.025f, z), v3(8, 0.05f, 0.295f), c);
    }
    Color wall = rgba(214, 208, 196), wall2 = rgba(196, 204, 210);
    // Задняя стена
    box(v3(0, 1.5f, -3.05f), v3(8.2f, 3, 0.1f), wall);
    // Правая стена
    box(v3(4.05f, 1.5f, 0), v3(0.1f, 3, 6.2f), wall);
    // Передняя стена с дверью (x -3.05..-2.05)
    box(v3(-3.55f, 1.5f, 3.05f), v3(1.0f, 3, 0.1f), wall2);
    box(v3(0.975f, 1.5f, 3.05f), v3(6.05f, 3, 0.1f), wall2);
    box(v3(-2.55f, 2.55f, 3.05f), v3(1.0f, 0.9f, 0.1f), wall2);
    box(v3(-2.55f, 1.05f, 3.0f), v3(0.95f, 2.1f, 0.04f), rgba(120, 82, 52));
    box(v3(-2.2f, 1.0f, 2.97f), v3(0.1f, 0.03f, 0.04f), kSilver);
    // Левая стена с окном (z -1..1, y 1..2.2)
    box(v3(-4.05f, 1.5f, -2.0f), v3(0.1f, 3, 2.0f), wall2);
    box(v3(-4.05f, 1.5f, 2.0f), v3(0.1f, 3, 2.0f), wall2);
    box(v3(-4.05f, 0.5f, 0), v3(0.1f, 1.0f, 2.0f), wall2);
    box(v3(-4.05f, 2.6f, 0), v3(0.1f, 0.8f, 2.0f), wall2);
    gfx::setEmissive(true);
    box(v3(-4.07f, 1.6f, 0), v3(0.02f, 1.2f, 2.0f), rgba(150, 200, 245));
    box(v3(-4.06f, 1.25f, 0), v3(0.01f, 0.5f, 2.0f), rgba(120, 175, 120));
    gfx::setEmissive(false);
    box(v3(-3.98f, 1.6f, 0), v3(0.04f, 1.2f, 0.05f), rgba(240, 240, 240));
    box(v3(-3.98f, 1.6f, 0), v3(0.04f, 0.05f, 2.0f), rgba(240, 240, 240));
    box(v3(-3.93f, 0.98f, 0), v3(0.16f, 0.04f, 2.1f), rgba(240, 240, 240));
    // Плинтусы
    Color base = rgba(90, 70, 55);
    box(v3(0, 0.04f, -2.995f), v3(8, 0.08f, 0.01f), base);
    box(v3(3.995f, 0.04f, 0), v3(0.01f, 0.08f, 6), base);
    box(v3(-3.995f, 0.04f, 0), v3(0.01f, 0.08f, 6), base);
    // Потолок и лампа
    box(v3(0, 3.05f, 0), v3(8.2f, 0.1f, 6.2f), rgba(236, 236, 236));
    gfx::setEmissive(true);
    box(v3(0, 2.98f, 0), v3(0.9f, 0.04f, 0.9f), lampOn ? rgba(255, 250, 230) : rgba(150, 150, 150));
    gfx::setEmissive(false);

    // Ковёр
    box(v3(0.4f, 0.003f, 0.3f), v3(2.8f, 0.006f, 1.9f), rgba(70, 92, 128));
    box(v3(0.4f, 0.004f, 0.3f), v3(2.5f, 0.006f, 1.6f), rgba(84, 108, 148));

    // Стол с монитором
    Color wood = rgba(176, 132, 90), woodDark = rgba(120, 86, 58);
    box(v3(-1.0f, 0.75f, -2.6f), v3(2.0f, 0.05f, 0.8f), wood);
    for (float x : {-1.95f, -0.05f})
        for (float z : {-2.95f, -2.25f}) box(v3(x, 0.3625f, z), v3(0.05f, 0.725f, 0.05f), woodDark);
    box(v3(-0.25f, 0.45f, -2.6f), v3(0.4f, 0.55f, 0.7f), woodDark);
    box(v3(-0.25f, 0.55f, -2.248f), v3(0.36f, 0.22f, 0.01f), wood);
    box(v3(-0.25f, 0.30f, -2.248f), v3(0.36f, 0.22f, 0.01f), wood);
    box(v3(-0.25f, 0.6f, -2.24f), v3(0.08f, 0.015f, 0.015f), kSilver);
    box(v3(-0.25f, 0.35f, -2.24f), v3(0.08f, 0.015f, 0.015f), kSilver);
    // Клавиатура, мышь, коврик
    box(v3(-1.1f, 0.778f, -2.45f), v3(0.8f, 0.006f, 0.32f), rgba(40, 40, 46));
    box(v3(-1.2f, 0.79f, -2.45f), v3(0.44f, 0.018f, 0.14f), rgba(30, 30, 34));
    for (int r = 0; r < 4; r++)
        for (int k = 0; k < 14; k++)
            box(v3(-1.405f + k * 0.0295f, 0.801f, -2.5f + r * 0.032f), v3(0.025f, 0.006f, 0.026f),
                rgba(60, 60, 68));
    box(v3(-0.82f, 0.79f, -2.45f), v3(0.06f, 0.025f, 0.1f), rgba(30, 30, 34));
    // Кружка
    cylinder(v3(-1.75f, 0.775f, -2.4f), 0.04f, 0.1f, rgba(200, 70, 60), 12);

    // Верстак с антистатическим ковриком
    box(v3(1.6f, 0.75f, -2.6f), v3(1.8f, 0.05f, 0.8f), rgba(110, 110, 118));
    box(v3(1.6f, 0.7775f, -2.6f), v3(1.6f, 0.005f, 0.7f), rgba(40, 70, 120));
    for (float x : {0.75f, 2.45f})
        for (float z : {-2.95f, -2.25f}) box(v3(x, 0.3625f, z), v3(0.05f, 0.725f, 0.05f), rgba(70, 70, 76));
    box(v3(1.6f, 0.15f, -2.6f), v3(1.7f, 0.03f, 0.7f), rgba(90, 90, 96));
    // Лампа над верстаком
    box(v3(2.3f, 0.79f, -2.85f), v3(0.12f, 0.02f, 0.12f), kDark);
    box(v3(2.3f, 1.05f, -2.85f), v3(0.02f, 0.5f, 0.02f), kDark);
    box(v3(2.1f, 1.3f, -2.85f), v3(0.4f, 0.02f, 0.02f), kDark);
    gfx::setEmissive(true);
    box(v3(1.92f, 1.27f, -2.85f), v3(0.14f, 0.04f, 0.08f), rgba(255, 245, 200));
    gfx::setEmissive(false);
    // Постер над верстаком
    box(v3(1.6f, 1.85f, -2.995f), v3(1.0f, 0.7f, 0.01f), rgba(30, 34, 48));
    gfx::setEmissive(true);
    box(v3(1.35f, 1.95f, -2.989f), v3(0.3f, 0.3f, 0.005f), rgba(80, 200, 255));
    box(v3(1.75f, 1.75f, -2.989f), v3(0.45f, 0.08f, 0.005f), rgba(255, 120, 60));
    box(v3(1.75f, 1.65f, -2.989f), v3(0.35f, 0.04f, 0.005f), rgba(200, 200, 210));
    gfx::setEmissive(false);

    // Стеллаж
    Color shelf = rgba(150, 110, 75);
    for (float z : {-1.2f, 0.6f}) box(v3(3.65f, 0.8f, z), v3(0.6f, 1.6f, 0.03f), shelf);
    for (float y : {0.45f, 0.95f, 1.45f}) box(v3(3.65f, y, -0.3f), v3(0.6f, 0.03f, 1.83f), shelf);
    box(v3(3.94f, 0.8f, -0.3f), v3(0.02f, 1.6f, 1.83f), shelf);

    // Стул
    box(v3(-1.0f, 0.48f, -1.75f), v3(0.5f, 0.06f, 0.5f), rgba(40, 40, 46));
    box(v3(-1.0f, 0.82f, -1.52f), v3(0.48f, 0.6f, 0.06f), rgba(40, 40, 46));
    box(v3(-1.0f, 0.82f, -1.49f), v3(0.3f, 0.4f, 0.01f), rgba(200, 60, 60));
    cylinder(v3(-1.0f, 0.05f, -1.75f), 0.03f, 0.43f, kSilver, 8);
    for (int i = 0; i < 5; i++) {
        gfx::push(translate(-1.0f, 0.04f, -1.75f) * rotY(i * 72.0f));
        box(v3(0.15f, 0, 0), v3(0.3f, 0.03f, 0.04f), kDark);
        gfx::pop();
    }

    // Зона доставки: жёлто-чёрная разметка
    Rect dz = deliveryZone;
    box(v3(dz.x + dz.w / 2, 0.002f, dz.y + dz.h / 2), v3(dz.w, 0.004f, dz.h), rgba(230, 190, 40));
    box(v3(dz.x + dz.w / 2, 0.003f, dz.y + dz.h / 2), v3(dz.w - 0.12f, 0.004f, dz.h - 0.12f), rgba(120, 120, 120));
    for (int i = 0; i < 6; i++) {
        float x = dz.x + 0.1f + i * 0.21f;
        box(v3(x, 0.0035f, dz.y + 0.03f), v3(0.08f, 0.004f, 0.06f), kDark);
        box(v3(x, 0.0035f, dz.y + dz.h - 0.03f), v3(0.08f, 0.004f, 0.06f), kDark);
    }
    // Поддон отправки/продажи
    Rect sz = shipZone;
    Color pal = rgba(186, 150, 100);
    for (float z : {sz.y + 0.06f, sz.y + sz.h / 2, sz.y + sz.h - 0.06f})
        box(v3(sz.x + sz.w / 2, 0.05f, z), v3(sz.w, 0.1f, 0.1f), rgba(160, 125, 82));
    for (int i = 0; i < 7; i++)
        box(v3(sz.x + 0.085f + i * 0.172f, 0.12f, sz.y + sz.h / 2), v3(0.14f, 0.04f, sz.h), pal);
    box(v3(sz.x + sz.w / 2, 0.139f, sz.y + sz.h / 2), v3(sz.w * 0.5f, 0.002f, 0.15f), rgba(60, 170, 80));

    // Растение
    cylinder(v3(3.6f, 0, 2.5f), 0.18f, 0.35f, rgba(170, 90, 60), 12);
    for (int i = 0; i < 9; i++) {
        gfx::push(translate(3.6f, 0.35f, 2.5f) * rotY(i * 40.0f) * rotZ(25.0f + (i % 3) * 12));
        box(v3(0, 0.25f, 0), v3(0.06f, 0.5f, 0.02f), rgba(60, 140 + (i % 3) * 20, 70));
        gfx::pop();
    }
    // Корзина
    cylinder(v3(0.3f, 0, -2.0f), 0.15f, 0.35f, rgba(70, 70, 76), 12);

    drawMonitorBody();
}

void World::drawMonitorBody() {
    Vec3 m = monitorPos;
    float cy = m.y + 0.40f;
    box(v3(m.x, m.y + 0.006f, m.z), v3(0.26f, 0.012f, 0.18f), kDark);
    box(v3(m.x, m.y + 0.17f, m.z - 0.03f), v3(0.05f, 0.33f, 0.03f), rgba(40, 40, 44));
    box(v3(m.x, cy, m.z), v3(0.66f, 0.425f, 0.035f), rgba(22, 22, 26));
    box(v3(m.x, cy, m.z - 0.03f), v3(0.4f, 0.25f, 0.03f), rgba(32, 32, 36));
    // Экран (чёрная поверхность — сверху на неё кладётся холст ОС)
    gfx::setEmissive(true);
    box(v3(screenCenter.x, screenCenter.y, screenCenter.z - 0.001f), v3(screenW, screenH, 0.002f), rgba(8, 8, 10));
    gfx::setEmissive(false);
}

Mat4 World::screenCanvasMatrix(float cw, float ch) {
    Vec3 tl = v3(screenCenter.x - screenW / 2, screenCenter.y + screenH / 2, screenCenter.z + 0.0012f);
    Mat4 r = identity();
    r.m[0] = screenW / cw;
    r.m[5] = -screenH / ch;
    r.m[12] = tl.x;
    r.m[13] = tl.y;
    r.m[14] = tl.z;
    return r;
}

// ---------------------------------------------------------------------------
// Модели деталей

void World::drawFan(float time, bool spinning, bool rgbOn, Color rgb, Color frame) {
    drawFrameBars(frame);
    float ang = spinning ? std::fmod(time * 1400.0f, 360.0f) : 0.0f;
    Color blade = rgbOn ? rgba(235, 235, 240, 210) : rgba(45, 45, 50);
    if (rgbOn) {
        gfx::setEmissive(true);
        float a = 0.0535f;
        box(v3(0, 0.0125f, a), v3(0.1f, 0.012f, 0.003f), rgb);
        box(v3(0, 0.0125f, -a), v3(0.1f, 0.012f, 0.003f), rgb);
        box(v3(a, 0.0125f, 0), v3(0.003f, 0.012f, 0.1f), rgb);
        box(v3(-a, 0.0125f, 0), v3(0.003f, 0.012f, 0.1f), rgb);
        gfx::setEmissive(false);
    }
    impeller(0.055f, 0.022f, ang, blade, rgba(35, 35, 40));
}

void World::drawEntity(Entity& e, float time, bool powered, Color rgb, float fanSpeed) {
    gfx::resetXform();
    gfx::push(worldMatrix(e));
    if (e.inBox)
        drawPacked(e);
    else
        drawPart(e, time, powered, rgb, fanSpeed);
    gfx::resetXform();
}

void World::drawEntityAt(const Entity& e, const Mat4& m, float time) {
    gfx::resetXform();
    gfx::push(m);
    drawPart(e, time, false, rgba(0, 0, 0, 0), 0);
    gfx::resetXform();
}

void World::drawPacked(const Entity& e) {
    Vec3 s = boxSize(e.def);
    box(v3(0, s.y / 2, 0), s, rgba(196, 154, 108));
    box(v3(0, s.y + 0.0005f, 0), v3(0.06f, 0.001f, s.z + 0.002f), rgba(214, 186, 140));
    box(v3(s.x * 0.22f, s.y + 0.0008f, -s.z * 0.22f), v3(s.x * 0.3f, 0.0008f, s.z * 0.25f), rgba(240, 240, 240));
    Color cat = hsv((float)e.def->cat * 33.0f, 0.65f, 0.85f);
    box(v3(s.x / 2 + 0.0006f, s.y * 0.7f, 0), v3(0.001f, std::min(0.04f, s.y * 0.3f), s.z * 0.6f), cat);
    box(v3(-s.x / 2 - 0.0006f, s.y * 0.7f, 0), v3(0.001f, std::min(0.04f, s.y * 0.3f), s.z * 0.6f), cat);
}

void World::drawGlass(Entity& e) {
    if (e.inBox || e.def->cat != Cat::Case || !e.def->glass) return;
    using namespace dims;
    gfx::resetXform();
    gfx::push(worldMatrix(e));
    box(v3(caseW / 2 - 0.002f, caseH / 2, 0), v3(0.004f, caseH - 0.02f, caseD - 0.02f), rgba(150, 190, 220, 50));
    gfx::resetXform();
}

void World::drawPart(const Entity& e, float time, bool powered, Color rgb, float fanSpeed) {
    const PartDef* d = e.def;
    Color c = d->color;
    bool spin = powered && fanSpeed > 0.01f;
    float t = time * std::max(0.3f, fanSpeed);
    switch (d->cat) {
        case Cat::Case: {
            using namespace dims;
            Color dark = lerpColor(c, kDark, 0.35f);
            box(v3(0, 0.006f, 0), v3(caseW, 0.012f, caseD), c);
            box(v3(0, caseH - 0.005f, 0), v3(caseW, 0.01f, caseD), c);
            box(v3(-caseW / 2 + 0.005f, caseH / 2, 0), v3(0.01f, caseH, caseD), c);
            box(v3(-caseW / 2 + 0.011f, caseH / 2, 0), v3(0.002f, caseH - 0.03f, caseD - 0.03f), dark);
            box(v3(0, caseH / 2, caseD / 2 - 0.005f), v3(caseW, caseH, 0.01f), c);
            box(v3(0, caseH / 2, -caseD / 2 + 0.005f), v3(caseW, caseH, 0.01f), c);
            if (d->id == "case_air" || d->id == "case_tower") {
                // Сетка на передней панели
                for (int i = 0; i < 9; i++)
                    box(v3(0, 0.04f + i * 0.045f, caseD / 2 + 0.001f), v3(caseW - 0.03f, 0.02f, 0.002f), dark);
            } else {
                box(v3(0, 0.3f, caseD / 2 + 0.001f), v3(0.12f, 0.004f, 0.002f), dark);
            }
            // Задняя панель: I/O и заглушки слотов
            box(v3(-0.07f, 0.34f, -caseD / 2 - 0.001f), v3(0.05f, 0.15f, 0.002f), kDark);
            for (int i = 0; i < 5; i++)
                box(v3(0.0f, 0.24f - i * 0.022f, -caseD / 2 - 0.001f), v3(0.11f, 0.012f, 0.002f), dark);
            box(v3(0, 0.055f, -caseD / 2 - 0.001f), v3(0.14f, 0.075f, 0.002f), kDark);
            // Ножки
            for (float x : {-0.08f, 0.08f})
                for (float z : {-0.18f, 0.18f}) box(v3(x, 0.003f, z), v3(0.03f, 0.006f, 0.04f), kDark);
            // Кнопка питания, индикатор, USB
            cylinder(v3(-0.05f, caseH, 0.17f), 0.013f, 0.004f, lerpColor(c, kDark, 0.5f), 12);
            gfx::setEmissive(true);
            box(v3(-0.02f, caseH + 0.001f, 0.17f), v3(0.008f, 0.002f, 0.004f),
                powered ? rgba(80, 255, 120) : rgba(40, 60, 40));
            gfx::setEmissive(false);
            box(v3(0.06f, 0.434f, caseD / 2 + 0.001f), v3(0.016f, 0.008f, 0.002f), kDark);
            box(v3(0.085f, 0.434f, caseD / 2 + 0.001f), v3(0.016f, 0.008f, 0.002f), kDark);
            if (d->rgb) {
                gfx::setEmissive(powered && rgb.a > 0);
                Color strip = (powered && rgb.a > 0) ? withAlpha(rgb, 255) : rgba(200, 200, 205);
                box(v3(caseW / 2 - 0.012f, caseH / 2, caseD / 2 - 0.014f), v3(0.004f, caseH - 0.04f, 0.006f), strip);
                box(v3(caseW / 2 - 0.012f, caseH - 0.016f, 0), v3(0.004f, 0.006f, caseD - 0.04f), strip);
                gfx::setEmissive(false);
            }
            break;
        }
        case Cat::Motherboard: {
            Color hs = rgba(70, 72, 80);
            if (d->id == "mb_x670") hs = rgba(120, 30, 34);
            box(v3(0, 0.002f, 0), v3(0.30f, 0.004f, 0.244f), c);
            bool amd = d->socket.rfind("AM", 0) == 0;
            box(v3(-0.06f, 0.006f, -0.03f), v3(0.052f, 0.004f, 0.052f), amd ? rgba(225, 225, 225) : rgba(160, 160, 165));
            box(v3(-0.06f, 0.0062f, -0.03f), v3(0.042f, 0.004f, 0.042f), rgba(60, 60, 64));
            box(v3(-0.06f, 0.016f, -0.095f), v3(0.09f, 0.02f, 0.025f), hs);
            box(v3(-0.122f, 0.016f, -0.03f), v3(0.025f, 0.02f, 0.09f), hs);
            auto slots = slotsFor(d);
            int ri = 0;
            for (auto& s : slots) {
                Vec3 p = xformPoint(s.local, v3(0, 0, 0));
                if (s.accepts == Cat::RAM) {
                    Color sc = (ri++ % 2) ? rgba(60, 60, 66) : rgba(150, 150, 158);
                    box(v3(p.x, 0.007f, p.z), v3(0.14f, 0.006f, 0.007f), sc);
                } else if (s.accepts == Cat::Storage && s.m2) {
                    box(v3(p.x, 0.006f, p.z - 0.045f), v3(0.022f, 0.004f, 0.006f), kDark);
                    box(v3(p.x, 0.005f, p.z + 0.04f), v3(0.004f, 0.002f, 0.004f), kSilver);
                }
            }
            box(v3(0.035f, 0.008f, -0.065f), v3(0.008f, 0.008f, 0.09f), kDark);
            box(v3(0.035f, 0.0122f, -0.065f), v3(0.009f, 0.0008f, 0.092f), kSilver);
            box(v3(0.075f, 0.007f, -0.095f), v3(0.007f, 0.006f, 0.03f), kDark);
            box(v3(0.135f, 0.007f, -0.065f), v3(0.007f, 0.006f, 0.09f), kDark);
            box(v3(0.075f, 0.009f, 0.065f), v3(0.045f, 0.01f, 0.045f), hs);
            box(v3(-0.05f, 0.012f, 0.113f), v3(0.06f, 0.016f, 0.012f), kDark);
            box(v3(-0.14f, 0.012f, -0.075f), v3(0.012f, 0.014f, 0.02f), kDark);
            box(v3(-0.07f, 0.02f, -0.11f), v3(0.13f, 0.032f, 0.02f), hs);
            cylinder(v3(0.07f, 0.004f, -0.005f), 0.01f, 0.003f, kSilver, 12);
            for (int i = 0; i < 6; i++)
                cylinder(v3(-0.095f + i * 0.008f, 0.004f, 0.01f), 0.003f, 0.008f, rgba(40, 40, 50), 6);
            if (d->id == "mb_x670" || d->id == "mb_z790" || d->id == "mb_b650") {
                gfx::setEmissive(powered);
                box(v3(-0.06f, 0.0265f, -0.095f), v3(0.07f, 0.001f, 0.006f),
                    powered ? rgb.a ? withAlpha(rgb, 255) : rgba(255, 60, 60) : rgba(90, 90, 96));
                gfx::setEmissive(false);
            }
            break;
        }
        case Cat::CPU: {
            box(v3(0, 0.001f, 0), v3(0.04f, 0.002f, 0.04f), rgba(30, 100, 60));
            box(v3(0, 0.003f, 0), v3(0.032f, 0.002f, 0.032f), e.broken ? rgba(80, 70, 60) : rgba(200, 200, 206));
            box(v3(-0.0175f, 0.0021f, -0.0175f), v3(0.004f, 0.0003f, 0.004f), kGold);
            if (e.broken) box(v3(0.004f, 0.0041f, 0.003f), v3(0.012f, 0.0004f, 0.01f), rgba(20, 15, 10));
            if (e.pasted) cylinder(v3(0, 0.004f, 0), 0.009f, 0.0012f, rgba(170, 170, 176), 10);
            break;
        }
        case Cat::Cooler: {
            switch (d->coolerKind) {
                case 0:
                    box(v3(0, 0.006f, 0), v3(0.07f, 0.012f, 0.07f), rgba(175, 175, 180));
                    for (int i = 0; i < 7; i++)
                        box(v3(-0.03f + i * 0.01f, 0.022f, 0), v3(0.004f, 0.02f, 0.08f), rgba(175, 175, 180));
                    gfx::push(translate(0, 0.031f, 0) * scale(0.72f, 1, 0.72f));
                    drawFan(t, spin, false, rgb, c);
                    gfx::pop();
                    break;
                case 1:
                case 2: {
                    box(v3(0, 0.006f, 0), v3(0.04f, 0.012f, 0.04f), kCopper);
                    for (int i = 0; i < 4; i++) cylinder(v3(-0.03f + i * 0.02f, 0.01f, 0), 0.003f, 0.14f, kCopper, 6);
                    if (d->coolerKind == 1) {
                        for (int i = 0; i < 12; i++)
                            box(v3(0, 0.04f + i * 0.0095f, 0), v3(0.125f, 0.0025f, 0.05f), c);
                        box(v3(0, 0.157f, 0), v3(0.125f, 0.006f, 0.05f), kDark);
                        gfx::push(translate(0, 0.095f, 0.025f) * rotX(90));
                        drawFan(t, spin, false, rgb, kDark);
                        gfx::pop();
                    } else {
                        for (float z : {-0.032f, 0.032f}) {
                            for (int i = 0; i < 13; i++)
                                box(v3(0, 0.04f + i * 0.0095f, z), v3(0.125f, 0.0025f, 0.035f), rgba(60, 60, 66));
                            box(v3(0, 0.162f, z), v3(0.125f, 0.006f, 0.035f), kDark);
                        }
                        gfx::push(translate(0, 0.1f, -0.0125f) * rotX(90));
                        drawFan(t, spin, false, rgb, kDark);
                        gfx::pop();
                    }
                    break;
                }
                default: {
                    cylinder(v3(0, 0, 0), 0.03f, 0.035f, kDark, 16);
                    bool lit = powered && rgb.a > 0;
                    gfx::setEmissive(lit);
                    cylinder(v3(0, 0.035f, 0), 0.026f, 0.003f, lit ? withAlpha(rgb, 255) : rgba(200, 200, 205), 16);
                    gfx::setEmissive(false);
                    box(v3(-0.06f, 0.03f, -0.012f), v3(0.07f, 0.008f, 0.008f), kDark);
                    box(v3(-0.06f, 0.03f, 0.012f), v3(0.07f, 0.008f, 0.008f), kDark);
                    break;
                }
            }
            break;
        }
        case Cat::RAM: {
            box(v3(0, 0.001f, 0), v3(0.12f, 0.002f, 0.0016f), kGold);
            if (d->ram == RamType::DDR3) {
                box(v3(0, 0.016f, 0), v3(0.133f, 0.03f, 0.0015f), rgba(30, 110, 60));
                for (int i = 0; i < 8; i++)
                    for (float z : {-0.0012f, 0.0012f})
                        box(v3(-0.056f + i * 0.016f, 0.016f, z), v3(0.011f, 0.01f, 0.001f), kDark);
            } else {
                box(v3(0, 0.016f, 0), v3(0.133f, 0.03f, 0.0015f), rgba(30, 80, 50));
                box(v3(0, 0.018f, 0), v3(0.131f, 0.03f, 0.006f), c);
                if (d->ram == RamType::DDR5) {
                    bool lit = powered && rgb.a > 0;
                    gfx::setEmissive(lit);
                    box(v3(0, 0.0345f, 0), v3(0.128f, 0.004f, 0.005f), lit ? withAlpha(rgb, 255) : rgba(230, 230, 235));
                    gfx::setEmissive(false);
                } else {
                    box(v3(0, 0.0335f, 0), v3(0.131f, 0.003f, 0.006f), rgba(190, 40, 40));
                }
            }
            break;
        }
        case Cat::GPU: {
            float L = gpuLength(d), H = gpuHeight(d), T = gpuThick(d);
            box(v3(0.001f, H * 0.5f, L * 0.5f), v3(0.002f, H, L), rgba(20, 60, 34));
            box(v3(-0.0015f, H * 0.5f, L * 0.5f), v3(0.003f, H - 0.004f, L * 0.98f), rgba(52, 54, 60));
            box(v3(T * 0.5f + 0.001f, H * 0.5f + 0.003f, L * 0.5f), v3(T - 0.002f, H - 0.006f, L), c);
            int fans = L > 0.26f ? 3 : (L < 0.185f ? 1 : 2);
            float fr = std::min(H * 0.42f, L / fans * 0.45f);
            for (int i = 0; i < fans; i++) {
                float zc = L * (i + 0.5f) / fans;
                gfx::push(translate(T, H * 0.5f + 0.003f, zc) * rotZ(-90));
                cylinder(v3(0, 0, 0), fr * 1.04f, 0.002f, rgba(15, 15, 18), 18);
                gfx::push(translate(0, -0.012f, 0));
                impeller(fr, 0.012f, spin ? std::fmod(t * 900.0f + i * 40, 360.0f) : i * 20.0f, rgba(55, 55, 60),
                         rgba(30, 30, 34), 9);
                gfx::pop();
                gfx::pop();
            }
            box(v3(0.001f, -0.003f, 0.06f), v3(0.0025f, 0.006f, 0.075f), kGold);
            box(v3(T * 0.5f, H * 0.5f, -0.001f), v3(T + 0.006f, H + 0.012f, 0.002f), kSilver);
            bool lit = powered && d->supported;
            gfx::setEmissive(lit);
            box(v3(T * 0.5f, H + 0.0005f, L * 0.45f), v3(T * 0.5f, 0.002f, L * 0.5f),
                d->supported ? (lit ? rgba(130, 220, 40) : rgba(100, 160, 40)) : rgba(110, 110, 116));
            gfx::setEmissive(false);
            box(v3(T * 0.35f, H + 0.005f, L * 0.75f), v3(0.012f, 0.01f, 0.025f), kDark);
            break;
        }
        case Cat::PSU: {
            box(v3(0, 0.043f, 0), v3(0.15f, 0.086f, 0.14f), c);
            box(v3(0, 0.0863f, 0), v3(0.1f, 0.0006f, 0.09f), rgba(205, 205, 210));
            Color band = d->watts >= 1000 ? rgba(200, 200, 220) : (d->watts >= 750 ? kGold : rgba(170, 110, 60));
            box(v3(0, 0.0866f, 0.032f), v3(0.1f, 0.0006f, 0.02f), band);
            box(v3(0.04f, 0.045f, -0.0705f), v3(0.03f, 0.025f, 0.002f), kDark);
            box(v3(-0.03f, 0.045f, -0.0705f), v3(0.07f, 0.06f, 0.001f), rgba(55, 55, 60));
            box(v3(0, 0.06f, 0.075f), v3(0.07f, 0.024f, 0.012f), kDark);
            break;
        }
        case Cat::Storage: {
            if (d->stor == StorType::HDD) {
                box(v3(0, 0.013f, 0), v3(0.1f, 0.026f, 0.147f), c);
                box(v3(0, 0.0263f, 0.01f), v3(0.09f, 0.0006f, 0.1f), rgba(232, 232, 236));
                box(v3(0, 0.002f, 0), v3(0.09f, 0.004f, 0.13f), rgba(20, 60, 34));
            } else if (d->stor == StorType::SATA_SSD) {
                box(v3(0, 0.0035f, 0), v3(0.07f, 0.007f, 0.1f), c);
                box(v3(0, 0.0072f, 0), v3(0.06f, 0.0006f, 0.07f), rgba(80, 140, 230));
            } else {
                box(v3(0, 0.0005f, 0), v3(0.022f, 0.001f, 0.08f), rgba(20, 60, 34));
                box(v3(0, 0.0015f, -0.016f), v3(0.016f, 0.0012f, 0.022f), kDark);
                box(v3(0, 0.0015f, 0.012f), v3(0.016f, 0.0012f, 0.02f), kDark);
                box(v3(0, 0.0023f, 0), v3(0.02f, 0.0004f, 0.06f), d->capGB >= 2000 ? rgba(240, 120, 40) : rgba(70, 70, 76));
                box(v3(0, 0.0005f, -0.038f), v3(0.018f, 0.0012f, 0.004f), kGold);
            }
            break;
        }
        case Cat::Fan: {
            bool lit = d->fanRgb && powered && rgb.a > 0;
            drawFan(t, spin, lit, rgb, c);
            break;
        }
        case Cat::Paste: {
            gfx::push(translate(0, 0.008f, -0.045f) * rotX(90));
            cylinder(v3(0, 0, 0), 0.008f, 0.07f, rgba(235, 235, 240), 10);
            cylinder(v3(0, 0.07f, 0), 0.003f, 0.02f, rgba(150, 150, 155), 8);
            cylinder(v3(0, -0.004f, 0), 0.006f, 0.004f, rgba(60, 120, 220), 8);
            gfx::pop();
            break;
        }
        case Cat::USB: {
            box(v3(0, 0.004f, 0.008f), v3(0.018f, 0.008f, 0.034f), c);
            box(v3(0, 0.004f, -0.017f), v3(0.012f, 0.0045f, 0.016f), kSilver);
            break;
        }
        default:
            break;
    }
}
