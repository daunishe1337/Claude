'use strict';
/* ============================================================
   ui.js — интерфейс:
   • верхняя панель (монеты, доход/с, доход/тап, валюты);
   • нижние вкладки и подвкладки, сетка «Ещё»;
   • панели систем (каждый модуль описывает свои панели);
   • делегирование нажатий: data-a="модуль.действие:арг1:арг2";
   • модальные окна, всплывающие уведомления, красные точки;
   • постепенное открытие систем.
   ============================================================ */
const UI = (() => {
  const UI = {
    tab: 'tap',          // текущая вкладка
    sub: {},             // выбранная панель в каждой группе
    more: null,          // открытая панель во вкладке «Ещё»
    panels: [],          // все панели всех модулей
    views: {},           // виджеты экрана тапа: контейнер -> [{order, fn}]
    lastHtml: '',
  };

  const TABS = [
    { id: 'tap', icon: '👆', name: 'Тап' },
    { id: 'upg', icon: '⬆️', name: 'Улучшения' },
    { id: 'bld', icon: '🏭', name: 'Здания' },
    { id: 'hero', icon: '🦸', name: 'Герой' },
    { id: 'pres', icon: '🌟', name: 'Престиж' },
    { id: 'more', icon: '☰', name: 'Ещё' },
  ];

  const $ = (id) => document.getElementById(id);
  let el = {};

  /* ---------- Виджеты экрана тапа ---------- */
  UI.view = (container, order, fn) => {
    (UI.views[container] = UI.views[container] || []).push({ order, fn });
    UI.views[container].sort((a, b) => a.order - b.order);
  };

  /* ---------- Открытие систем ---------- */
  // панель открыта навсегда, как только выполнено условие unlock()
  UI.isOpen = (p) => {
    const F = G.S.flags;
    if (F['p:' + p.id]) return true;
    if (!p.unlock || p.unlock()) {
      F['p:' + p.id] = true;
      if (UI.ready && p.unlock) {
        F['n:' + p.id] = true; // пометка «новое»
        G.toast(`🔓 Открыто: <b>${p.icon} ${p.name}</b>`, 'unlock');
        SFX.play('open');
      }
      return true;
    }
    return false;
  };
  const groupPanels = (g) => UI.panels.filter((p) => p.group === g);
  const openPanels = (g) => groupPanels(g).filter(UI.isOpen);
  UI.byId = (id) => UI.panels.find((p) => p.id === id);
  const hint = (p) => (typeof p.hint === 'function' ? p.hint() : p.hint) || 'Продолжайте играть';
  const badge = (p) => UI.isOpen(p) && (G.S.flags['n:' + p.id] || (p.badge && p.badge()));

  /* ---------- Вкладки ---------- */
  function renderTabs() {
    let h = '';
    for (const t of TABS) {
      const locked = t.id !== 'tap' && t.id !== 'more' && !openPanels(t.id).length;
      let dot = false;
      if (!locked && t.id !== UI.tab || (t.id === 'more' && UI.tab === 'more' && UI.more)) {
        dot = groupPanels(t.id).some(badge);
      }
      h += `<button class="tab${UI.tab === t.id ? ' on' : ''}${locked ? ' locked' : ''}" data-a="ui.tab:${t.id}" data-k="${t.id}">
        <span class="ti">${locked ? '🔒' : t.icon}</span><span class="tn">${t.name}</span>${dot ? '<i class="dot"></i>' : ''}</button>`;
    }
    U.morph(el.tabs, h);
  }

  /* ---------- Подвкладки и тело панели ---------- */
  function currentPanel() {
    if (UI.tab === 'more') return UI.more ? UI.byId(UI.more) : null;
    const list = openPanels(UI.tab);
    if (!list.length) return null;
    let p = list.find((x) => x.id === UI.sub[UI.tab]);
    if (!p) { p = list[0]; UI.sub[UI.tab] = p.id; }
    return p;
  }

  function renderSubTabs(p) {
    let h = '';
    if (UI.tab === 'more') {
      if (p) h = `<div class="subhead"><button class="back" data-a="ui.back">‹ Ещё</button><b>${p.icon} ${p.name}</b></div>`;
    } else {
      const list = openPanels(UI.tab);
      if (list.length > 1) {
        h = '<div class="chips">' + list.map((x) =>
          `<button class="chip${x === p ? ' on' : ''}" data-a="ui.sub:${x.id}" data-k="${x.id}">${x.icon} ${x.name}${badge(x) && x !== p ? '<i class="dot"></i>' : ''}</button>`).join('') + '</div>';
      }
    }
    U.morph(el.sub, h);
  }

  function renderMoreGrid() {
    const list = groupPanels('more').slice().sort((a, b) => (a.order || 0) - (b.order || 0));
    let h = '<div class="grid">';
    for (const p of list) {
      const open = UI.isOpen(p);
      h += open
        ? `<button class="tile" data-a="ui.open:${p.id}" data-k="${p.id}"><span class="ti">${p.icon}</span><span class="tn">${p.name}</span>${badge(p) ? '<i class="dot"></i>' : ''}</button>`
        : `<div class="tile locked" data-k="${p.id}"><span class="ti">🔒</span><span class="tn">${p.name}</span><small>${hint(p)}</small></div>`;
    }
    return h + '</div>';
  }

  function renderPanel() {
    const p = currentPanel();
    renderSubTabs(p);
    let body;
    if (UI.tab === 'more' && !p) body = renderMoreGrid();
    else if (!p) body = '<div class="info">Пока недоступно</div>';
    else {
      if (G.S.flags['n:' + p.id]) delete G.S.flags['n:' + p.id];
      try { body = p.render(); } catch (e) { console.error(e); body = `<div class="info bad">Ошибка панели: ${U.esc(e.message)}</div>`; }
    }
    const key = UI.tab + ':' + (p ? p.id : 'grid');
    const html = `<div class="pbody" data-k="${key}">${body}</div>`;
    if (key !== UI.lastKey) { el.body.scrollTop = 0; UI.lastKey = key; }
    U.morph(el.body, html);
  }

  /* ---------- Экран тапа ---------- */
  function renderTapViews() {
    for (const c in UI.views) {
      const node = $(c);
      if (!node) continue;
      let h = '';
      for (const v of UI.views[c]) {
        try { h += v.fn() || ''; } catch (e) { console.error(e); }
      }
      U.morph(node, h);
    }
  }

  /* ---------- Верхняя панель ---------- */
  let hudCache = {};
  function setText(node, key, txt) { if (hudCache[key] !== txt) { hudCache[key] = txt; node.textContent = txt; } }
  function renderHud() {
    setText(el.coins, 'c', U.fint(G.cur('coins')));
    setText(el.cps, 'cps', '⚡ ' + U.fmt(G.cps) + '/с');
    setText(el.tapv, 'tap', '👆 ' + U.fmt(G.tapVal) + '/тап');
  }
  function renderCurRow() {
    let h = '';
    for (const id in G.CUR) {
      const c = G.CUR[id];
      if (!c.hud || !(G.S.life[id] > 0 || G.cur(id) > 0)) continue;
      h += `<span class="cchip" data-k="${id}">${c.icon} ${U.fint(G.cur(id))}</span>`;
    }
    for (const v of (UI.views.hudExtra || [])) h += v.fn() || '';
    U.morph(el.curRow, h);
  }

  /* ---------- Ближняя и дальняя цели ---------- */
  // модули отдают кандидатов через goals(): [{icon, name, cost}] (монеты)
  UI.income = () => G.cps + G.tapVal * (G.byId.tap ? G.byId.tap.rate() : 0);
  function renderGoals() {
    const inc = UI.income();
    let best = null;
    for (const m of G.mods) {
      if (!m.goals) continue;
      for (const g of m.goals()) {
        const need = g.cost - G.cur('coins');
        if (need <= 0) continue;
        const eta = inc > 0 ? need / inc : Infinity;
        if (!best || eta < best.eta) best = { ...g, eta };
      }
    }
    let far = null;
    for (const m of G.mods) if (m.farGoal) { const f = m.farGoal(); if (f && (!far || (f.prio || 0) > (far.prio || 0))) far = f; }
    let h = '';
    if (best) h += `<div class="goal"><span>🎯</span><span class="gt">${best.icon} ${best.name}</span><b>${best.eta === Infinity ? '—' : '≈ ' + U.time(best.eta)}</b></div>`;
    if (far) h += `<div class="goal far"><span>🏔️</span><span class="gt">${far.text}</span><b>${far.eta || ''}</b></div>`;
    U.morph(el.goals, h);
  }

  /* ---------- Обновление ---------- */
  let slowT = 0;
  UI.refresh = (force) => {
    if (!UI.ready) return;
    if (UI.tab === 'tap') { renderTapViews(); renderGoals(); }
    else renderPanel();
    renderCurRow();
    const now = performance.now();
    if (force || now - slowT > 900) { slowT = now; renderTabs(); }
  };

  /* ---------- Модальные окна ---------- */
  const queue = [];
  let modalOpen = false;
  // opts: {title, html, buttons:[{text, cls, fn, keep}], cls}
  UI.modal = (opts) => {
    if (modalOpen) { queue.push(opts); return; }
    modalOpen = true;
    const root = el.modal;
    const btns = (opts.buttons || [{ text: 'OK' }]);
    root.innerHTML = `<div class="mback"></div><div class="mbox ${opts.cls || ''}">
      ${opts.title ? `<div class="mtitle">${opts.title}</div>` : ''}
      <div class="mbody">${opts.html || ''}</div>
      <div class="mbtns">${btns.map((b, i) => `<button class="btn ${b.cls || ''}" data-mi="${i}">${b.text}</button>`).join('')}</div></div>`;
    root.classList.add('on');
    root.onclick = (e) => {
      const b = e.target.closest('[data-mi]');
      if (b) {
        const def = btns[+b.dataset.mi];
        const keep = def.fn ? def.fn(root) === false : false;
        if (!keep) UI.closeModal();
        G.dirty = true; UI.refresh(true);
      } else if (e.target.classList.contains('mback') && opts.dismiss !== false) UI.closeModal();
      else {
        const a = e.target.closest('[data-a]');
        if (a && !a.disabled) { UI.act(a.dataset.a, a); }
      }
    };
    if (opts.onOpen) opts.onOpen(root);
  };
  UI.closeModal = () => {
    el.modal.classList.remove('on');
    el.modal.innerHTML = '';
    modalOpen = false;
    if (queue.length) setTimeout(() => UI.modal(queue.shift()), 120);
  };
  UI.confirm = (title, html, fn, yes = 'Да') => UI.modal({
    title, html, buttons: [{ text: 'Отмена' }, { text: yes, cls: 'danger', fn }],
  });

  /* ---------- Уведомления ---------- */
  function toast(html, cls = '') {
    const t = document.createElement('div');
    t.className = 'toast ' + cls;
    t.innerHTML = html;
    el.toasts.appendChild(t);
    while (el.toasts.children.length > 4) el.toasts.removeChild(el.toasts.firstChild);
    setTimeout(() => t.classList.add('out'), 2600);
    setTimeout(() => t.remove(), 3100);
  }

  /* ---------- Хелперы разметки ---------- */
  UI.cost = (cur, v) => `<span class="cost${G.cur(cur) >= v * (1 - 1e-9) ? '' : ' no'}">${G.CUR[cur].icon} ${U.fmt(v)}</span>`;
  UI.costs = (obj) => Object.keys(obj).map((k) => UI.cost(k, obj[k])).join(' ');
  // кнопка покупки по «предложению» G.offer()
  UI.buy = (act, o, txt) => {
    if (o.max) return '<button class="buy max" disabled>МАКС</button>';
    const n = o.n > 1 || G.S.settings.buy !== 1 ? `<small>+${U.fint(o.n)}</small>` : '';
    return `<button class="buy${o.ok ? ' ok' : ''}" data-a="${act}"${U.dis(o.ok)}>${txt ? `<small>${txt}</small>` : n}<span>${G.CUR[o.cur].icon} ${U.fmt(o.cost)}</span></button>`;
  };
  // кнопка с произвольной стоимостью {валюта: число}
  UI.buyC = (act, cost, txt, extraOk = true) => {
    const ok = extraOk && G.can(cost);
    return `<button class="buy${ok ? ' ok' : ''}" data-a="${act}"${U.dis(ok)}>${txt ? `<small>${txt}</small>` : ''}<span>${Object.keys(cost).map((k) => G.CUR[k].icon + ' ' + U.fmt(cost[k])).join(' ')}</span></button>`;
  };
  UI.bar = (f, cls = '', txt = '') => `<div class="bar ${cls}"><i style="width:${(U.clamp(f, 0, 1) * 100).toFixed(1)}%"></i>${txt ? `<span>${txt}</span>` : ''}</div>`;
  // переключатель ×1 / ×10 / ×100 / MAX
  UI.amt = () => {
    const cur = G.S.settings.buy;
    return '<div class="amt">' + [1, 10, 100, 'max'].map((v) =>
      `<button class="${cur === v ? 'on' : ''}" data-a="ui.amt:${v}">${v === 'max' ? 'MAX' : '×' + v}</button>`).join('') + '</div>';
  };
  // карточка: {k, icon, title, lv, desc, extra, btn, cls}
  UI.card = (c) => `<div class="card ${c.cls || ''}" data-k="${c.k}">
      <div class="cico">${c.icon}</div>
      <div class="cmain"><div class="ctitle">${c.title}${c.lv !== undefined && c.lv !== '' ? ` <span class="lv">${c.lv}</span>` : ''}</div>
      ${c.desc ? `<div class="cdesc">${c.desc}</div>` : ''}${c.extra || ''}</div>
      ${c.btn ? `<div class="cbtn">${c.btn}</div>` : ''}</div>`;
  UI.sec = (title, sub = '') => `<div class="sec">${title}${sub ? ` <small>${sub}</small>` : ''}</div>`;
  UI.info = (html, cls = '') => `<div class="info ${cls}">${html}</div>`;
  // внутренние вкладки панели: tabs=[[id, label]], cur, act — действие модуля
  UI.inTabs = (tabs, cur, act) => '<div class="chips small">' + tabs.map(([id, label]) =>
    `<button class="chip${cur === id ? ' on' : ''}" data-a="${act}:${id}">${label}</button>`).join('') + '</div>';

  /* ---------- Действия ---------- */
  UI.act = (str, node) => {
    const parts = str.split(':');
    const [mid, fn] = parts[0].split('.');
    const args = parts.slice(1).map((a) => (a !== '' && !isNaN(a) ? +a : a));
    const m = mid === 'ui' ? UI : G.byId[mid];
    const f = m && m.actions && m.actions[fn];
    if (!f) { console.warn('Нет действия', str); return; }
    try { f.apply(m, args.concat([node])); } catch (e) { console.error(e); }
    G.dirty = true;
    G.recalc();
    UI.refresh(true);
  };

  UI.actions = {
    tab(id) {
      if (id !== 'tap' && id !== 'more' && !openPanels(id).length) {
        const p = groupPanels(id)[0];
        G.toast('🔒 ' + (p ? hint(p) : 'Скоро'));
        SFX.play('error');
        return;
      }
      if (id === 'more' && UI.tab === 'more') UI.more = null; // повторное нажатие — к сетке
      UI.tab = id;
      el.scrTap.classList.toggle('active', id === 'tap');
      el.scrPanel.classList.toggle('active', id !== 'tap');
      UI.lastKey = null;
    },
    sub(id) { UI.sub[UI.tab] = id; },
    open(id) {
      const p = UI.byId(id);
      if (!p) return;
      if (p.group === 'more') { UI.actions.tab('more'); UI.more = id; }
      else { UI.actions.tab(p.group); UI.sub[p.group] = id; }
    },
    back() { UI.more = null; },
    amt(v) { G.S.settings.buy = v === 'max' ? 'max' : +v; },
    curs() {
      let h = '<div class="curlist">';
      for (const id in G.CUR) {
        if (!(G.S.life[id] > 0 || G.cur(id) > 0)) continue;
        const c = G.CUR[id];
        h += `<div><span>${c.icon} ${c.name}</span><b>${U.fmt(G.cur(id))}</b></div>`;
      }
      UI.modal({ title: '💰 Валюты', html: h + '</div>' });
    },
  };

  /* ---------- Инициализация ---------- */
  UI.init = () => {
    el = {
      coins: $('coinsVal'), cps: $('cpsVal'), tapv: $('tapValHud'), curRow: $('curRow'),
      tabs: $('tabs'), sub: $('subTabs'), body: $('panelBody'), goals: $('goals'),
      scrTap: $('scrTap'), scrPanel: $('scrPanel'), modal: $('modalRoot'), toasts: $('toasts'),
    };
    for (const m of G.mods) for (const p of (m.panels || [])) { p.mod = m; UI.panels.push(p); }
    // единый обработчик нажатий по data-a
    document.addEventListener('click', (e) => {
      SFX.unlock();
      const a = e.target.closest('[data-a]');
      if (!a || a.disabled || el.modal.contains(a)) return;
      UI.act(a.dataset.a, a);
    });
    G.on('toast', toast);
    G.on('frame', renderHud);
    setInterval(() => UI.refresh(), 250);
    // первичная проверка открытых систем без уведомлений
    for (const p of UI.panels) UI.isOpen(p);
    UI.ready = true;
    renderTabs();
    UI.refresh(true);
  };

  return UI;
})();
