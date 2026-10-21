'use strict';
/* ============================================================
   main.js — запуск игры и регистрация service worker (офлайн).
   Подключается последним, когда все модули уже объявлены.
   ============================================================ */
window.addEventListener('DOMContentLoaded', () => {
  FX.init();
  G.boot();   // загрузка сохранения и инициализация модулей
  UI.init();  // интерфейс
  G.run();    // офлайн-прогресс и игровой цикл

  // service worker работает только по http(s), не с file://
  if ('serviceWorker' in navigator && location.protocol.startsWith('http')) {
    navigator.serviceWorker.register('sw.js').catch((e) => console.warn('SW не зарегистрирован', e));
  }
  // запрет масштабирования щипком (iOS)
  document.addEventListener('gesturestart', (e) => e.preventDefault());
});
