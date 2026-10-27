// Página de la app de prueba: sólo dice si una app ve la sesión del hub (ver src/test_app.c).
// Usa lib.js; textos en web/lang.

getJSON('/api/state').then(async st => {
  const box = $('status');
  await useLang(st.lang);
  if (!st.user) {
    box.className += ' off';
    box.textContent = t('test_off');
    return;
  }
  box.textContent = t('test_on');
  box.appendChild(document.createElement('b')).textContent = st.user;
});
