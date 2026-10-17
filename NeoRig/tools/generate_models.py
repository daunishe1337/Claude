#!/usr/bin/env python3
"""
Генератор .glb-моделей для NeoRig (только стандартная библиотека Python 3).

Запуск:  python3 tools/generate_models.py
Результат: app/src/main/assets/models/*.glb

Соглашения (их же понимает приложение, см. GlbAdapter в SceneComposer.kt):
  * имена материалов задают роль: body / accent — цвета детали из JSON,
    rgb — RGB-подсветка, screen — светящийся экран (без освещения),
    glass — стекло; остальные материалы рисуются своим цветом;
  * узлы с «spin» в имени вращаются вокруг своей локальной оси Y (крыльчатки);
  * модель вписывается в габариты детали, поэтому единицы измерения не важны
    (здесь — сантиметры), но важна ориентация осей:
      видеокарта: X — от платы наружу, Y — толщина (вниз до 0), Z — длина;
      кулер/помпа: X — от платы наружу, Y — вверх, Z — к передней панели;
      ОЗУ: X — высота планки, Y — длина, Z — толщина;
      монитор: основание в начале координат, экран смотрит в +Z.
Можно заменить любой файл своей моделью (например, из Blender), соблюдая эти правила.
"""
import json
import math
import os
import struct

OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "app", "src", "main", "assets", "models")


# ----------------------------------------------------------------------------------------------
# Геометрия
# ----------------------------------------------------------------------------------------------

class Mesh:
    def __init__(self):
        self.pos, self.nrm, self.col, self.idx = [], [], [], []

    def add_tri_list(self, verts, normals, indices, colors=None):
        base = len(self.pos)
        self.pos += verts
        self.nrm += normals
        if colors:
            self.col += colors
        self.idx += [base + i for i in indices]
        return self

    def merge(self, other):
        return self.add_tri_list(other.pos, other.nrm, other.idx, other.col or None)

    def transformed(self, m):
        out = Mesh()
        out.pos = [mat_point(m, p) for p in self.pos]
        nm = normal_matrix(m)
        out.nrm = [normalize(mat3_vec(nm, n)) for n in self.nrm]
        out.col = list(self.col)
        out.idx = list(self.idx)
        return out


def normalize(v):
    l = math.sqrt(sum(c * c for c in v)) or 1.0
    return tuple(c / l for c in v)


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


# Матрицы 4x4 по строкам (row-major) — для удобства в Python
def mat_identity():
    return [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]]


def mat_mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def chain(*ms):
    r = mat_identity()
    for m in ms:
        r = mat_mul(r, m)
    return r


def T(x, y, z):
    m = mat_identity()
    m[0][3], m[1][3], m[2][3] = x, y, z
    return m


def S(x, y=None, z=None):
    y = x if y is None else y
    z = x if z is None else z
    return [[x, 0, 0, 0], [0, y, 0, 0], [0, 0, z, 0], [0, 0, 0, 1]]


def R(deg, axis):
    ax = normalize(axis)
    a = math.radians(deg)
    c, s, t = math.cos(a), math.sin(a), 1 - math.cos(a)
    x, y, z = ax
    return [
        [t * x * x + c, t * x * y - s * z, t * x * z + s * y, 0],
        [t * x * y + s * z, t * y * y + c, t * y * z - s * x, 0],
        [t * x * z - s * y, t * y * z + s * x, t * z * z + c, 0],
        [0, 0, 0, 1],
    ]


def mat_point(m, p):
    return tuple(m[i][0] * p[0] + m[i][1] * p[1] + m[i][2] * p[2] + m[i][3] for i in range(3))


def mat3_vec(m, v):
    return tuple(m[i][0] * v[0] + m[i][1] * v[1] + m[i][2] * v[2] for i in range(3))


def normal_matrix(m):
    a, b, c = m[0][:3]
    d, e, f = m[1][:3]
    g, h, i = m[2][:3]
    det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g)
    inv = 1.0 / det
    # (M^-1)^T = cofactor / det
    return [
        [(e * i - f * h) * inv, -(d * i - f * g) * inv, (d * h - e * g) * inv],
        [-(b * i - c * h) * inv, (a * i - c * g) * inv, -(a * h - b * g) * inv],
        [(b * f - c * e) * inv, -(a * f - c * d) * inv, (a * e - b * d) * inv],
    ]


def box(cx=0, cy=0, cz=0, sx=1, sy=1, sz=1):
    m = Mesh()
    faces = [
        ((1, 0, 0), (0, 0, -1), (0, 1, 0)), ((-1, 0, 0), (0, 0, 1), (0, 1, 0)),
        ((0, 1, 0), (1, 0, 0), (0, 0, -1)), ((0, -1, 0), (1, 0, 0), (0, 0, 1)),
        ((0, 0, 1), (1, 0, 0), (0, 1, 0)), ((0, 0, -1), (-1, 0, 0), (0, 1, 0)),
    ]
    for n, u, v in faces:
        base = len(m.pos)
        for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            p = [0.5 * n[k] + 0.5 * su * u[k] + 0.5 * sv * v[k] for k in range(3)]
            m.pos.append((cx + p[0] * sx, cy + p[1] * sy, cz + p[2] * sz))
            m.nrm.append(n)
        m.idx += [base, base + 1, base + 2, base, base + 2, base + 3]
    return m


def box_between(mn, mx):
    return box((mn[0] + mx[0]) / 2, (mn[1] + mx[1]) / 2, (mn[2] + mx[2]) / 2, mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2])


def cylinder_y(radius, height, segments=32, y0=None):
    """Цилиндр вдоль Y с центром в начале координат."""
    m = Mesh()
    h = height / 2
    for i in range(segments + 1):
        a = 2 * math.pi * i / segments
        x, z = math.cos(a), math.sin(a)
        m.pos += [(x * radius, -h, z * radius), (x * radius, h, z * radius)]
        m.nrm += [(x, 0, z), (x, 0, z)]
    for i in range(segments):
        a, b, c, d = 2 * i, 2 * i + 1, 2 * i + 2, 2 * i + 3
        m.idx += [a, b, c, c, b, d]
    for cap in (-h, h):
        ci = len(m.pos)
        ny = 1 if cap > 0 else -1
        m.pos.append((0, cap, 0)); m.nrm.append((0, ny, 0))
        for i in range(segments + 1):
            a = 2 * math.pi * i / segments
            m.pos.append((math.cos(a) * radius, cap, math.sin(a) * radius)); m.nrm.append((0, ny, 0))
        for i in range(segments):
            a = ci + 1 + i
            m.idx += [ci, a + 1, a] if cap > 0 else [ci, a, a + 1]
    return m


def torus_y(radius, tube, major=40, minor=10):
    m = Mesh()
    for i in range(major + 1):
        u = 2 * math.pi * i / major
        cu, su = math.cos(u), math.sin(u)
        for j in range(minor + 1):
            v = 2 * math.pi * j / minor
            cv, sv = math.cos(v), math.sin(v)
            r = radius + tube * cv
            m.pos.append((r * cu, tube * sv, r * su))
            m.nrm.append((cv * cu, sv, cv * su))
    stride = minor + 1
    for i in range(major):
        for j in range(minor):
            a = i * stride + j
            b = (i + 1) * stride + j
            m.idx += [a, a + 1, b, a + 1, b + 1, b]
    return m


def extrude_z(profile, z0, z1):
    """Призма: выпуклый многоугольник в плоскости XY (против часовой) вытянут по Z."""
    m = Mesh()
    n = len(profile)
    for i in range(n):
        p, q = profile[i], profile[(i + 1) % n]
        normal = normalize((q[1] - p[1], -(q[0] - p[0]), 0))
        base = len(m.pos)
        m.pos += [(p[0], p[1], z0), (q[0], q[1], z0), (q[0], q[1], z1), (p[0], p[1], z1)]
        m.nrm += [normal] * 4
        m.idx += [base, base + 1, base + 2, base, base + 2, base + 3]
    for z, nz in ((z1, 1), (z0, -1)):
        base = len(m.pos)
        for p in profile:
            m.pos.append((p[0], p[1], z)); m.nrm.append((0, 0, nz))
        for i in range(1, n - 1):
            m.idx += [base, base + i, base + i + 1] if nz > 0 else [base, base + i + 1, base + i]
    return m


def rotor(diameter, blades=9):
    """Крыльчатка вокруг оси Y: изогнутые лопасти + ступица."""
    m = Mesh()
    r_hub, r_tip = diameter * 0.17, diameter * 0.46
    for b in range(blades):
        base_angle = 360.0 * b / blades
        steps = 6
        verts = []
        for s in range(steps + 1):
            t = s / steps
            r = r_hub + (r_tip - r_hub) * t
            sweep = math.radians(base_angle + 22 * t)          # изгиб лопасти
            width = math.radians(26 - 8 * t)
            pitch = diameter * 0.06
            for side in (-1, 1):
                a = sweep + side * width / 2
                verts.append((r * math.cos(a), side * pitch * (1 - 0.4 * t), r * math.sin(a)))
        blade = Mesh()
        blade.pos = verts
        for k in range(steps):
            a, b2, c, d = 2 * k, 2 * k + 1, 2 * k + 2, 2 * k + 3
            blade.idx += [a, c, b2, b2, c, d]
        blade.nrm = [(0, 1, 0)] * len(verts)
        recompute_normals(blade)
        m.merge(blade)
    hub = cylinder_y(r_hub * 1.05, diameter * 0.12, 24)
    m.merge(hub)
    return m


def recompute_normals(mesh):
    acc = [[0.0, 0.0, 0.0] for _ in mesh.pos]
    for t in range(0, len(mesh.idx), 3):
        a, b, c = mesh.idx[t:t + 3]
        n = cross(sub(mesh.pos[b], mesh.pos[a]), sub(mesh.pos[c], mesh.pos[a]))
        for i in (a, b, c):
            for k in range(3):
                acc[i][k] += n[k]
    mesh.nrm = [normalize(n) for n in acc]


# ----------------------------------------------------------------------------------------------
# Запись glTF 2.0 (.glb)
# ----------------------------------------------------------------------------------------------

class Gltf:
    def __init__(self):
        self.nodes, self.meshes, self.materials, self.accessors, self.views = [], [], [], [], []
        self.bin = bytearray()
        self.material_index = {}

    def material(self, name, color, metallic=0.0, roughness=0.6, emissive=None):
        if name in self.material_index:
            return self.material_index[name]
        mat = {
            "name": name,
            "pbrMetallicRoughness": {
                "baseColorFactor": [srgb_to_linear(c) for c in color[:3]] + [color[3] if len(color) > 3 else 1.0],
                "metallicFactor": metallic,
                "roughnessFactor": roughness,
            },
        }
        if emissive:
            mat["emissiveFactor"] = emissive
        self.materials.append(mat)
        self.material_index[name] = len(self.materials) - 1
        return self.material_index[name]

    def _view(self, data, target):
        while len(self.bin) % 4:
            self.bin.append(0)
        offset = len(self.bin)
        self.bin += data
        self.views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(data), "target": target})
        return len(self.views) - 1

    def _accessor(self, data, comp, count, typ, target, minmax=None):
        view = self._view(data, target)
        acc = {"bufferView": view, "componentType": comp, "count": count, "type": typ}
        if minmax:
            acc["min"], acc["max"] = minmax
        self.accessors.append(acc)
        return len(self.accessors) - 1

    def add_mesh(self, mesh, material, name):
        pos = b"".join(struct.pack("<3f", *p) for p in mesh.pos)
        nrm = b"".join(struct.pack("<3f", *n) for n in mesh.nrm)
        mins = [min(p[k] for p in mesh.pos) for k in range(3)]
        maxs = [max(p[k] for p in mesh.pos) for k in range(3)]
        attrs = {
            "POSITION": self._accessor(pos, 5126, len(mesh.pos), "VEC3", 34962, (mins, maxs)),
            "NORMAL": self._accessor(nrm, 5126, len(mesh.nrm), "VEC3", 34962),
        }
        if mesh.col:
            col = b"".join(struct.pack("<4f", *c) for c in mesh.col)
            attrs["COLOR_0"] = self._accessor(col, 5126, len(mesh.col), "VEC4", 34962)
        if len(mesh.pos) < 65536:
            idx = b"".join(struct.pack("<H", i) for i in mesh.idx)
            ia = self._accessor(idx, 5123, len(mesh.idx), "SCALAR", 34963)
        else:
            idx = b"".join(struct.pack("<I", i) for i in mesh.idx)
            ia = self._accessor(idx, 5125, len(mesh.idx), "SCALAR", 34963)
        self.meshes.append({"name": name, "primitives": [{"attributes": attrs, "indices": ia, "material": material, "mode": 4}]})
        return len(self.meshes) - 1

    def node(self, name, mesh, material, translation=None, rotation=None):
        node = {"name": name, "mesh": self.add_mesh(mesh, material, name)}
        if translation:
            node["translation"] = list(translation)
        if rotation:
            node["rotation"] = list(rotation)
        self.nodes.append(node)

    def save(self, path):
        doc = {
            "asset": {"version": "2.0", "generator": "NeoRig generate_models.py"},
            "scene": 0,
            "scenes": [{"nodes": list(range(len(self.nodes)))}],
            "nodes": self.nodes,
            "meshes": self.meshes,
            "materials": self.materials,
            "accessors": self.accessors,
            "bufferViews": self.views,
            "buffers": [{"byteLength": len(self.bin)}],
        }
        js = json.dumps(doc, separators=(",", ":")).encode("utf-8")
        while len(js) % 4:
            js += b" "
        while len(self.bin) % 4:
            self.bin.append(0)
        total = 12 + 8 + len(js) + 8 + len(self.bin)
        with open(path, "wb") as f:
            f.write(struct.pack("<III", 0x46546C67, 2, total))
            f.write(struct.pack("<II", len(js), 0x4E4F534A)); f.write(js)
            f.write(struct.pack("<II", len(self.bin), 0x004E4942)); f.write(self.bin)
        print(f"{os.path.basename(path)}: {len(self.nodes)} узлов, {total // 1024} КБ")


def srgb_to_linear(c):
    return c ** 2.2


def quat_axis_angle(axis, deg):
    ax = normalize(axis)
    h = math.radians(deg) / 2
    s = math.sin(h)
    return (ax[0] * s, ax[1] * s, ax[2] * s, math.cos(h))


# ----------------------------------------------------------------------------------------------
# Модели
# ----------------------------------------------------------------------------------------------

def gpu(path, fans, length):
    g = Gltf()
    h, t = (13.5, 6.2) if fans == 3 else (12.2, 4.6)
    body = g.material("body", (0.2, 0.2, 0.22), 0.45, 0.38)
    accent = g.material("accent", (0.6, 0.6, 0.6), 0.5, 0.35)
    fins = g.material("fins", (0.75, 0.77, 0.8), 0.85, 0.3)
    backplate = g.material("backplate", (0.12, 0.12, 0.14), 0.7, 0.35)
    dark = g.material("dark", (0.06, 0.06, 0.07), 0.1, 0.8)
    metal = g.material("bracket", (0.8, 0.82, 0.86), 0.9, 0.25)
    rgb = g.material("rgb", (0.1, 0.1, 0.1), 0.0, 0.3)
    blade = g.material("blades", (0.12, 0.12, 0.14), 0.0, 0.55)

    g.node("backplate", box_between((0, -0.15, 0), (h, 0.25, length - 0.3)), backplate)
    g.node("pcb", box_between((0, -0.45, 0), (h - 0.3, -0.15, length - 0.5)), dark)
    g.node("bracket", box_between((-0.5, -t - 0.2, -0.3), (h * 0.92, 0.6, 0)), metal)
    # Рёбра радиатора: пластины поперёк длины
    fin_mesh = Mesh()
    z = 0.6
    while z < length - 0.8:
        fin_mesh.merge(box(h / 2 - 0.2, (-t + 1.1 - 0.45) / 2, z, h - 1.2, t - 1.55, 0.05))
        z += 0.26
    g.node("fins", fin_mesh, fins)
    # Кожух со скосом: нижняя крышка (выпуклый профиль в плоскости XY) + боковая стенка
    c = 0.9
    g.node("shroud", extrude_z([(0, -t), (h - c, -t), (h, -t + c), (h, -t + 1.2), (0, -t + 1.2)], 0, length), body)
    g.node("shroud_side", box_between((h - 0.6, -t + 1.2, 0), (h, -0.45, length)), body)
    # Акцентные вставки-«стрелки» на нижней крышке
    for zc in (length * 0.04, length * 0.96):
        g.node("accent", box(h / 2, -t - 0.02, zc, h * 0.7, 0.08, length * 0.04), accent)
    for i in range(fans):
        zc = length * (i + 0.5) / fans
        d = min(h - 1.6, length / fans - 1.2)
        g.node(f"fan_ring_{i}", torus_y(d * 0.47, 0.25), accent, translation=(h / 2, -t - 0.3, zc))
        g.node(f"spin_fan_{i}", rotor(d * 0.95), blade, translation=(h / 2, -t - 0.35, zc))
    g.node("rgb_strip", box_between((h + 0.01, -t * 0.55, length * 0.2), (h + 0.1, -t * 0.35, length * 0.8)), rgb)
    g.node("power", box(h - 0.2, 0.6, length * 0.7, 1.2, 1.0, 2.4), dark)
    g.save(path)


def tower(path, dual):
    g = Gltf()
    height = 15.5
    fins = g.material("body", (0.75, 0.77, 0.8), 0.8, 0.3)
    accent = g.material("accent", (0.1, 0.1, 0.12), 0.5, 0.35)
    pipes = g.material("pipes", (0.78, 0.5, 0.3), 0.9, 0.3)
    nickel = g.material("nickel", (0.83, 0.85, 0.88), 0.9, 0.25)
    frame = g.material("frame", (0.1, 0.1, 0.12), 0.0, 0.7)
    blade = g.material("blades", (0.12, 0.12, 0.14), 0.0, 0.55)
    rgb = g.material("rgb", (0.1, 0.1, 0.1), 0.0, 0.3)

    depth = 4.2 if dual else 5.2
    stacks = (-3.6, 3.6) if dual else (0.0,)
    g.node("base", box(1.1, 0, 0, 1.0, 4.2, 4.2), nickel)
    pipe_mesh = Mesh()
    for zc in stacks:
        for i in range(4):
            y = -2.7 + i * 1.8
            pipe_mesh.merge(cylinder_y(0.32, height - 1.6, 16).transformed(chain(T(height / 2 + 0.6, y, zc), R(-90, (0, 0, 1)))))
    g.node("heatpipes", pipe_mesh, pipes)
    fin_mesh = Mesh()
    for zc in stacks:
        x = 3.0
        while x < height - 0.6:
            # Пластина с «зубчатым» передним краем
            fin_mesh.merge(box(x, 0, zc, 0.06, 12, depth))
            x += 0.3
    g.node("fins", fin_mesh, fins)
    for zc in stacks:
        g.node("top_cover", box(height - 0.25, 0, zc, 0.5, 12.2, depth + 0.2), accent)
        g.node("rgb_top", box(height + 0.02, 0, zc, 0.06, 10.5, 0.5), rgb)
    fan_x = 3.0 + (height - 3.5) / 2
    fan_zs = (0.0, 3.6 + depth / 2 + 1.3) if dual else (depth / 2 + 1.3,)
    # Узел крыльчатки повёрнут так, что его ось Y смотрит вдоль -Z (поток к задней стенке)
    rot = quat_axis_angle((1, 0, 0), -90)
    for i, zc in enumerate(fan_zs):
        frame_mesh = Mesh()
        for (dx, dy, sx, sy) in ((0, 5.55, 12, 0.9), (0, -5.55, 12, 0.9), (5.55, 0, 0.9, 10.2), (-5.55, 0, 0.9, 10.2)):
            frame_mesh.merge(box(fan_x + dx, dy, zc, sx, sy, 2.5))
        g.node(f"fan_frame_{i}", frame_mesh, frame)
        g.node(f"fan_ring_{i}", torus_y(5.3, 0.4).transformed(R(90, (1, 0, 0))), rgb, translation=(fan_x, 0, zc))
        g.node(f"spin_fan_{i}", rotor(11.2), blade, translation=(fan_x, 0, zc), rotation=rot)
    g.save(path)


def aio_pump(path):
    g = Gltf()
    body = g.material("body", (0.12, 0.12, 0.14), 0.4, 0.4)
    accent = g.material("accent", (0.2, 0.2, 0.24), 0.5, 0.35)
    rgb = g.material("rgb", (0.1, 0.1, 0.1), 0.0, 0.3)
    nickel = g.material("nickel", (0.83, 0.85, 0.88), 0.9, 0.25)
    g.node("plate", box(0.8, 0, 0, 0.4, 5.6, 5.6), nickel)
    g.node("pump", cylinder_y(3.3, 3.4, 48).transformed(chain(T(2.6, 0, 0), R(-90, (0, 0, 1)))), body)
    g.node("cap", cylinder_y(2.7, 0.1, 48).transformed(chain(T(4.33, 0, 0), R(-90, (0, 0, 1)))), accent)
    g.node("rgb_ring", torus_y(3.0, 0.28, 48, 10).transformed(chain(T(4.2, 0, 0), R(-90, (0, 0, 1)))), rgb)
    g.node("fittings", box(3.0, 3.6, -1.2, 1.0, 1.0, 1.0), body)
    g.node("fittings2", box(3.0, 3.6, 1.2, 1.0, 1.0, 1.0), body)
    g.save(path)


def ram(path):
    g = Gltf()
    body = g.material("body", (0.12, 0.12, 0.14), 0.55, 0.35)
    accent = g.material("accent", (0.3, 0.3, 0.34), 0.4, 0.4)
    pcb = g.material("pcb", (0.05, 0.25, 0.15), 0.1, 0.5)
    rgb = g.material("rgb", (0.1, 0.1, 0.1), 0.0, 0.3)
    g.node("pcb", box(1.6, 0, 0, 3.2, 13.3, 0.12), pcb)
    # Радиатор со ступенчатым верхом (профиль в плоскости XY, толщина по Z)
    profile = [(0.4, -6.6), (3.5, -6.6), (3.9, -5.6), (3.9, 5.6), (3.5, 6.6), (0.4, 6.6)]
    g.node("spreader", extrude_z(profile, -0.35, 0.35), body)
    g.node("accent_cut", box(2.6, -3.5, 0.36, 1.2, 4.0, 0.02), accent)
    g.node("rgb_bar", box(4.15, 0, 0, 0.5, 11.6, 0.62), rgb)
    g.save(path)


def monitor(path):
    g = Gltf()
    w, h = 59.8, 33.6  # 27"
    body = g.material("body", (0.08, 0.08, 0.09), 0.3, 0.45)
    screen = g.material("screen", (1.0, 1.0, 1.0), 0.0, 0.2)
    accent = g.material("accent", (0.2, 0.2, 0.22), 0.4, 0.4)
    bottom = 11.0
    g.node("base", box(0, 0.4, -9, 26, 0.8, 10), body)
    g.node("neck", box(0, bottom / 2 + 3, -11, 4.5, bottom + 4, 1.6), body)
    g.node("panel", box(0, bottom + h / 2, -5.2, w + 1.2, h + 1.2, 1.6), body)
    g.node("back", box(0, bottom + h / 2, -6.6, w * 0.6, h * 0.6, 1.6), accent)
    # Экран: градиент через цвета вершин (сверху фиолетовый, снизу бирюзовый)
    top = (0.36 ** 2.2, 0.16 ** 2.2, 0.72 ** 2.2, 1.0)
    mid = (0.10 ** 2.2, 0.08 ** 2.2, 0.22 ** 2.2, 1.0)
    low = (0.05 ** 2.2, 0.62 ** 2.2, 0.68 ** 2.2, 1.0)
    quad = Mesh()
    rows = [(0.0, low), (0.45, mid), (1.0, top)]
    for (t0, c0), (t1, c1) in zip(rows, rows[1:]):
        y0, y1 = bottom + h * t0, bottom + h * t1
        quad.add_tri_list(
            [(-w / 2, y0, -4.38), (w / 2, y0, -4.38), (w / 2, y1, -4.38), (-w / 2, y1, -4.38)],
            [(0, 0, 1)] * 4, [0, 1, 2, 0, 2, 3], [c0, c0, c1, c1])
    g.node("screen", quad, screen)
    g.save(path)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    gpu(os.path.join(OUT_DIR, "gpu_dual_fan.glb"), fans=2, length=24.0)
    gpu(os.path.join(OUT_DIR, "gpu_triple_fan.glb"), fans=3, length=31.0)
    tower(os.path.join(OUT_DIR, "cooler_tower.glb"), dual=False)
    tower(os.path.join(OUT_DIR, "cooler_dual_tower.glb"), dual=True)
    aio_pump(os.path.join(OUT_DIR, "aio_pump.glb"))
    ram(os.path.join(OUT_DIR, "ram_stick.glb"))
    monitor(os.path.join(OUT_DIR, "monitor.glb"))


if __name__ == "__main__":
    main()
