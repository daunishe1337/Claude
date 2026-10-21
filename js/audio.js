'use strict';
/* ============================================================
   audio.js — синтезированные звуки через WebAudio (без файлов).
   ============================================================ */
const SFX = (() => {
  let ac = null, master = null, lastTap = 0;
  let gestured = false; // браузеры разрешают звук только после касания

  function ctx() {
    if (!ac) {
      const A = window.AudioContext || window.webkitAudioContext;
      if (!A) return null;
      ac = new A();
      master = ac.createGain();
      master.gain.value = 0.22;
      master.connect(ac.destination);
    }
    if (ac.state === 'suspended') ac.resume();
    return ac;
  }

  // один тон с огибающей: частота, длительность, форма, громкость, сдвиг частоты, задержка
  function tone(f, dur, type = 'sine', vol = 1, slide = 0, delay = 0) {
    const c = ac, t = c.currentTime + delay;
    const o = c.createOscillator(), g = c.createGain();
    o.type = type;
    o.frequency.setValueAtTime(f, t);
    if (slide) o.frequency.exponentialRampToValueAtTime(Math.max(30, f + slide), t + dur);
    g.gain.setValueAtTime(0.0001, t);
    g.gain.exponentialRampToValueAtTime(vol, t + 0.008);
    g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
    o.connect(g); g.connect(master);
    o.start(t); o.stop(t + dur + 0.02);
  }
  // шумовой «хлопок» (взрывы, удары)
  function noise(dur, vol = 0.5, delay = 0) {
    const c = ac, t = c.currentTime + delay;
    const len = Math.floor(c.sampleRate * dur);
    const buf = c.createBuffer(1, len, c.sampleRate);
    const d = buf.getChannelData(0);
    for (let i = 0; i < len; i++) d[i] = (Math.random() * 2 - 1) * (1 - i / len);
    const s = c.createBufferSource(), g = c.createGain();
    s.buffer = buf; g.gain.value = vol;
    s.connect(g); g.connect(master);
    s.start(t);
  }

  const P = {
    tap: (p = 0) => tone(420 + Math.min(p, 60) * 9, 0.06, 'triangle', 0.45, 160),
    crit: () => { tone(880, 0.09, 'square', 0.25, 400); tone(1320, 0.12, 'triangle', 0.3, 0, 0.03); },
    mega: () => { noise(0.25, 0.4); tone(220, 0.35, 'sawtooth', 0.3, 660); tone(1760, 0.25, 'triangle', 0.3, -600, 0.05); },
    buy: () => { tone(660, 0.07, 'triangle', 0.4); tone(990, 0.09, 'triangle', 0.35, 0, 0.05); },
    error: () => tone(160, 0.12, 'square', 0.2, -40),
    coin: () => { tone(1200, 0.06, 'square', 0.18); tone(1800, 0.1, 'triangle', 0.25, 0, 0.04); },
    ach: () => [523, 659, 784, 1047].forEach((f, i) => tone(f, 0.18, 'triangle', 0.35, 0, i * 0.08)),
    level: () => [392, 523, 659, 784, 1047].forEach((f, i) => tone(f, 0.16, 'square', 0.18, 0, i * 0.06)),
    boost: () => { tone(300, 0.3, 'sawtooth', 0.2, 900); tone(600, 0.3, 'triangle', 0.25, 600, 0.05); },
    boss: () => { tone(110, 0.5, 'sawtooth', 0.35, -40); noise(0.3, 0.3); },
    win: () => [523, 659, 784, 1047, 1319].forEach((f, i) => tone(f, 0.25, 'triangle', 0.4, 0, i * 0.09)),
    fail: () => [392, 330, 262].forEach((f, i) => tone(f, 0.25, 'triangle', 0.35, 0, i * 0.12)),
    zap: () => { noise(0.15, 0.35); tone(1500, 0.2, 'sawtooth', 0.2, -1300); },
    open: () => { tone(500, 0.08, 'triangle', 0.3, 300); tone(900, 0.15, 'triangle', 0.3, 300, 0.07); },
    prestige: () => [262, 330, 392, 523, 659, 784, 1047].forEach((f, i) => tone(f, 0.4, 'triangle', 0.35, 0, i * 0.1)),
  };

  return {
    play(name, arg) {
      if (!G.S || !G.S.settings.sound || !gestured) return;
      const c = ctx();
      if (!c || !P[name]) return;
      if (name === 'tap') {           // не чаще ~22 раз в секунду
        if (c.currentTime - lastTap < 0.045) return;
        lastTap = c.currentTime;
      }
      try { P[name](arg); } catch (e) { /* звук не критичен */ }
    },
    unlock() { gestured = true; if (G.S && G.S.settings.sound) ctx(); },
  };
})();

/* Вибрация (navigator.vibrate) с ограничением частоты */
const VIB = (() => {
  let last = 0;
  return (ms = 8) => {
    if (!G.S || !G.S.settings.vib || !navigator.vibrate) return;
    const t = performance.now();
    if (t - last < 35) return;
    last = t;
    try { navigator.vibrate(ms); } catch (e) { /* не поддерживается */ }
  };
})();
