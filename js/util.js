'use strict';
/* ============================================================
   util.js — общие утилиты:
   форматирование чисел и времени, случайность, прогрессии цен,
   кодирование сохранений и «морфинг» DOM (обновление разметки
   без пересоздания элементов, чтобы не терять нажатия кнопок).
   ============================================================ */
const U = (() => {
  // ---------- Форматирование чисел ----------
  // K, M, B, T, затем двухбуквенные aa, ab, ac … az, ba …
  const BASE_SUF = ['', 'K', 'M', 'B', 'T'];
  function suffix(k) {
    if (k < BASE_SUF.length) return BASE_SUF[k];
    const i = k - BASE_SUF.length;
    return String.fromCharCode(97 + Math.floor(i / 26)) + String.fromCharCode(97 + (i % 26));
  }
  // убираем хвостовые нули: "1.50" -> "1.5", "2.00" -> "2"
  function trimZeros(s) { return s.indexOf('.') >= 0 ? s.replace(/\.?0+$/, '') : s; }

  function fmt(n) {
    if (n !== n) return '0';            // NaN
    if (n === Infinity) return '∞';
    if (n < 0) return '-' + fmt(-n);
    if (n < 1000) {
      if (n >= 100 || Number.isInteger(n)) return String(Math.floor(n));
      return trimZeros(n.toFixed(n < 10 ? 2 : 1));
    }
    let k = Math.floor(Math.log10(n) / 3);
    let v = n / Math.pow(10, k * 3);
    let s = v < 10 ? v.toFixed(2) : v < 100 ? v.toFixed(1) : v.toFixed(0);
    // защита от округления 999.99 -> "1000"
    if (parseFloat(s) >= 1000) { k++; v = n / Math.pow(10, k * 3); s = v.toFixed(2); }
    return trimZeros(s) + suffix(k);
  }
  // целое число (уровни, счётчики) — без дробей
  const fint = (n) => fmt(Math.floor(n));
  // проценты: 0.153 -> "15.3%"
  function pct(x, d = 1) { return trimZeros((x * 100).toFixed(d)) + '%'; }
  // множитель: 2.5 -> "×2.5"
  function mul(x) { return '×' + (x < 1000 ? trimZeros(x.toFixed(x < 10 ? 2 : 1)) : fmt(x)); }

  // ---------- Время ----------
  const pad = (n) => (n < 10 ? '0' : '') + n;
  function time(s) {
    s = Math.max(0, Math.floor(s));
    if (s < 60) return s + 'с';
    const m = Math.floor(s / 60), h = Math.floor(m / 60), d = Math.floor(h / 24);
    if (d > 0) return d + 'д ' + (h % 24) + 'ч';
    if (h > 0) return h + 'ч ' + pad(m % 60) + 'м';
    return m + 'м ' + pad(s % 60) + 'с';
  }
  // ключ локального дня "2026-10-21" — для ежедневных механик
  function dayKey(t = Date.now()) {
    const d = new Date(t);
    return d.getFullYear() + '-' + pad(d.getMonth() + 1) + '-' + pad(d.getDate());
  }
  // номер локального дня (для серий и сезонов)
  function dayNum(t = Date.now()) {
    const d = new Date(t);
    return Math.floor((Date.UTC(d.getFullYear(), d.getMonth(), d.getDate())) / 86400000);
  }

  // ---------- Случайность ----------
  const rand = (a = 0, b = 1) => a + Math.random() * (b - a);
  const ri = (a, b) => Math.floor(rand(a, b + 1));
  const chance = (p) => Math.random() < p;
  const pick = (arr) => arr[Math.floor(Math.random() * arr.length)];
  // взвешенный выбор: items — массив, w(item) — вес
  function wpick(items, w, rnd = Math.random) {
    let sum = 0;
    for (const it of items) sum += w(it);
    let r = rnd() * sum;
    for (const it of items) { r -= w(it); if (r <= 0) return it; }
    return items[items.length - 1];
  }
  // детерминированный ГПСЧ (для ротаций магазина, курсов биржи)
  function seeded(seed) {
    let a = seed >>> 0;
    return () => {
      a |= 0; a = (a + 0x6D2B79F5) | 0;
      let t = Math.imul(a ^ (a >>> 15), 1 | a);
      t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }
  const clamp = (x, a, b) => Math.max(a, Math.min(b, x));

  // ---------- Прогрессии цен ----------
  // сумма цен n уровней начиная с уровня lvl: base·r^lvl·(r^n−1)/(r−1)
  function geoSum(base, r, lvl, n) {
    if (n <= 0) return 0;
    return base * Math.pow(r, lvl) * (Math.pow(r, n) - 1) / (r - 1);
  }
  // максимум уровней, которые можно купить на money
  function geoMax(base, r, lvl, money) {
    const c0 = base * Math.pow(r, lvl);
    if (!(money >= c0)) return 0;
    let n = Math.floor(Math.log(money * (r - 1) / c0 + 1) / Math.log(r));
    while (n > 0 && geoSum(base, r, lvl, n) > money * (1 + 1e-9)) n--;
    return Math.min(n, 100000);
  }

  // ---------- Base64 с поддержкой юникода (экспорт сохранения) ----------
  function b64enc(str) {
    const bytes = new TextEncoder().encode(str);
    let bin = '';
    for (let i = 0; i < bytes.length; i += 0x8000) bin += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x8000));
    return btoa(bin);
  }
  function b64dec(b64) {
    const bin = atob(b64.replace(/\s+/g, ''));
    const bytes = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
    return new TextDecoder().decode(bytes);
  }

  // ---------- Глубокое слияние сохранения с состоянием по умолчанию ----------
  // новые поля из defaults добавляются, сохранённые значения сохраняются
  const isObj = (o) => o && typeof o === 'object' && !Array.isArray(o);
  function merge(def, saved) {
    if (saved === undefined) return def;
    // поле по умолчанию пустое (null) — берём сохранённое значение как есть
    if (def === null || def === undefined) return saved;
    if (saved === null) return def;
    if (Array.isArray(def)) {
      if (!Array.isArray(saved)) return def;
      const out = def.slice();
      for (let i = 0; i < saved.length; i++) out[i] = (i < def.length) ? merge(def[i], saved[i]) : saved[i];
      return out;
    }
    if (isObj(def)) {
      if (!isObj(saved)) return def;
      const out = {};
      for (const k in saved) out[k] = saved[k];
      for (const k in def) out[k] = (k in saved) ? merge(def[k], saved[k]) : def[k];
      return out;
    }
    // примитив: тип должен совпадать
    return (typeof saved === typeof def) ? saved : def;
  }
  const clone = (o) => JSON.parse(JSON.stringify(o));

  // ---------- Морфинг DOM ----------
  // Обновляет содержимое target под новую разметку html, сохраняя
  // существующие элементы (нажатие не теряется, скролл не прыгает).
  const tpl = document.createElement('template');
  function morph(target, html) {
    tpl.innerHTML = html;
    syncChildren(target, tpl.content);
  }
  function sameNode(o, n) {
    if (o.nodeType !== n.nodeType || o.nodeName !== n.nodeName) return false;
    if (o.nodeType === 1 && o.getAttribute('data-k') !== n.getAttribute('data-k')) return false;
    return true;
  }
  function syncAttrs(o, n) {
    const oa = o.attributes;
    for (let i = oa.length - 1; i >= 0; i--) {
      const name = oa[i].name;
      if (!n.hasAttribute(name)) o.removeAttribute(name);
    }
    const na = n.attributes;
    for (let i = 0; i < na.length; i++) {
      const a = na[i];
      if (o.getAttribute(a.name) !== a.value) o.setAttribute(a.name, a.value);
    }
  }
  function syncChildren(oldP, newP) {
    const nn = Array.from(newP.childNodes);
    const on = Array.from(oldP.childNodes);
    for (let i = 0; i < nn.length; i++) {
      const n = nn[i], o = on[i];
      if (!o) { oldP.appendChild(n); continue; }
      if (!sameNode(o, n)) { oldP.replaceChild(n, o); continue; }
      if (o.nodeType !== 1) { if (o.nodeValue !== n.nodeValue) o.nodeValue = n.nodeValue; continue; }
      syncAttrs(o, n);
      if (o.hasAttribute('data-static')) continue;
      syncChildren(o, n);
    }
    for (let i = on.length - 1; i >= nn.length; i--) oldP.removeChild(on[i]);
  }

  // экранирование строк для вставки в HTML
  const esc = (s) => String(s).replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
  // атрибут disabled для кнопок
  const dis = (ok) => (ok ? '' : ' disabled');

  return {
    fmt, fint, pct, mul, time, pad, dayKey, dayNum,
    rand, ri, chance, pick, wpick, seeded, clamp,
    geoSum, geoMax, b64enc, b64dec, merge, clone, isObj,
    morph, esc, dis,
  };
})();
