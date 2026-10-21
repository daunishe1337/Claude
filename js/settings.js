'use strict';
/* ============================================================
   settings.js — настройки и статистика:
   звук, вибрация, эффекты, экспорт/импорт сохранения строкой,
   полный сброс с подтверждением.
   ============================================================ */
(() => {
  const toggle = (key, label, desc) => {
    const on = G.S.settings[key] !== false;
    return `<div class="card" data-k="set-${key}"><div class="cmain"><div class="ctitle">${label}</div><div class="cdesc">${desc}</div></div>
      <div class="cbtn"><button class="switch${on ? ' on' : ''}" data-a="settings.toggle:${key}"><i></i></button></div></div>`;
  };

  // строки статистики: [подпись, значение]
  const statRows = [];
  G.statRow = (order, fn) => { statRows.push({ order, fn }); statRows.sort((a, b) => a.order - b.order); };
  G.statRow(1, () => [
    ['👆 Тапов всего', U.fint(G.S.stats.taps || 0)],
    ['🪙 Монет за всё время', U.fmt(G.S.life.coins || 0)],
    ['🪙 Монет за этот забег', U.fmt(G.S.c.run)],
    ['⏱️ Время в игре', U.time(G.S.t.play)],
    ['⏱️ Текущий забег', U.time((Date.now() - G.S.t.runStart) / 1000)],
    ['🏭 Уровней зданий', U.fint(G.byId.buildings.total())],
    ['📈 Множитель дохода', U.mul(G.st.all)],
    ['🛒 Множитель цен', U.mul(G.st.cost)],
  ]);

  const M = G.mod({
    id: 'settings',
    panels: [
      {
        id: 'settings', group: 'more', name: 'Настройки', icon: '⚙️', order: 999,
        render() {
          let h = UI.sec('Звук и отклик');
          h += toggle('sound', '🔊 Звук', 'Синтезированные звуки тапов и покупок');
          h += toggle('vib', '📳 Вибрация', 'Отклик при тапах (если поддерживается)');
          h += toggle('fx', '✨ Эффекты', 'Частицы и тряска экрана');
          h += UI.sec('Сохранение', 'автосохранение каждые 5 с');
          h += `<div class="btnrow"><button class="btn" data-a="settings.saveNow">💾 Сохранить</button>
            <button class="btn" data-a="settings.exp">📤 Экспорт</button>
            <button class="btn" data-a="settings.imp">📥 Импорт</button></div>`;
          h += UI.sec('Опасная зона');
          h += `<button class="btn danger wide" data-a="settings.reset">🗑️ Полный сброс прогресса</button>`;
          h += UI.info('Тап-Империя · офлайн-игра (PWA). Установка: меню браузера → «Добавить на главный экран».', 'dim');
          return h;
        },
      },
      {
        id: 'stats', group: 'more', name: 'Статистика', icon: '📊', order: 998,
        render() {
          let h = '<div class="stats">';
          for (const r of statRows) for (const [a, b] of r.fn()) h += `<div><span>${a}</span><b>${b}</b></div>`;
          return h + '</div>';
        },
      },
    ],
    actions: {
      toggle(key) { G.S.settings[key] = G.S.settings[key] === false; SFX.play('buy'); },
      saveNow() { G.save(); G.toast('💾 Сохранено'); },
      exp() {
        const str = G.exportSave();
        UI.modal({
          title: '📤 Экспорт сохранения',
          html: `<p class="dim">Скопируйте строку и сохраните её в надёжном месте.</p><textarea class="savebox" readonly>${str}</textarea>`,
          buttons: [
            { text: '📋 Копировать', cls: 'pri', fn: (root) => {
              const ta = root.querySelector('textarea');
              ta.select();
              (navigator.clipboard ? navigator.clipboard.writeText(str) : Promise.reject()).then(
                () => G.toast('📋 Скопировано'),
                () => { document.execCommand('copy'); G.toast('📋 Скопировано'); });
              return false;
            } },
            { text: 'Закрыть' },
          ],
        });
      },
      imp() {
        UI.modal({
          title: '📥 Импорт сохранения',
          html: '<p class="dim">Вставьте строку сохранения. Текущий прогресс будет заменён.</p><textarea class="savebox" placeholder="Строка сохранения…"></textarea>',
          buttons: [
            { text: 'Отмена' },
            { text: 'Загрузить', cls: 'pri', fn: (root) => {
              const v = root.querySelector('textarea').value;
              try { G.importSave(v); } catch (e) { G.toast('❌ Неверная строка сохранения', 'bad'); return false; }
            } },
          ],
        });
      },
      reset() {
        UI.confirm('🗑️ Полный сброс', 'Весь прогресс будет удалён без возможности восстановления. Продолжить?', () => {
          setTimeout(() => UI.confirm('⚠️ Точно?', 'Это последнее предупреждение.', () => G.hardReset(), 'Удалить всё'), 150);
        }, 'Сбросить');
      },
    },
  });
  void M;
})();
