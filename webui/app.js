const settings = document.querySelector('#settings');
const template = document.querySelector('#setting-template');
const status = document.querySelector('#save-status');
const summary = document.querySelector('#settings-summary');
const empty = document.querySelector('#empty-settings');
const save = document.querySelector('#save');

function updateState() {
  const rows = [...settings.querySelectorAll('.setting-row')];
  const changed = rows.filter(item => item.dataset.originalValue !== item.querySelector('.value').value || item.dataset.isNew === 'true');
  save.disabled = changed.length === 0;
  empty.hidden = rows.length !== 0;
  summary.textContent = `${rows.filter(item => item.dataset.isNew !== 'true').length} existing setting${rows.filter(item => item.dataset.isNew !== 'true').length === 1 ? '' : 's'} loaded${changed.length ? ` · ${changed.length} unsaved change${changed.length === 1 ? '' : 's'}` : ''}`;
}

function row(setting = {key: '', value: ''}, isNew = false) {
  const fragment = template.content.cloneNode(true);
  const item = fragment.querySelector('.setting-row');
  const key = fragment.querySelector('.key');
  const value = fragment.querySelector('.value');
  const kind = fragment.querySelector('.setting-kind');
  const remove = fragment.querySelector('.remove');
  item.dataset.isNew = String(isNew);
  item.dataset.originalValue = setting.value;
  key.value = setting.key;
  key.readOnly = !isNew;
  value.value = setting.value;
  kind.textContent = isNew ? 'New' : 'Existing';
  kind.classList.toggle('new', isNew);
  remove.hidden = !isNew;
  remove.addEventListener('click', () => {
    item.remove();
    updateState();
  });
  value.addEventListener('input', updateState);
  key.addEventListener('input', updateState);
  settings.append(fragment);
}

async function json(path) {
  const response = await fetch(path);
  if (!response.ok) throw new Error(`${path} returned HTTP ${response.status}`);
  return response.json();
}

async function load() {
  status.textContent = '';
  summary.textContent = 'Loading current settings…';
  const [firmware, configuration, wifi] = await Promise.all([
    json('/api/firmware/info'),
    json('/api/config'),
    json('/api/wifi/diagnostics'),
  ]);
  document.querySelector('#device-status').textContent = `Firmware ${firmware.version} · ${wifi.ipv4_address || 'no network address'}`;
  settings.replaceChildren();
  configuration.settings.sort((left, right) => left.key.localeCompare(right.key)).forEach(setting => row(setting, false));
  updateState();
  const diagnostics = document.querySelector('#wifi');
  diagnostics.replaceChildren();
  for (const [label, value] of [['Connected', wifi.connected ? 'Yes' : 'No'], ['RSSI', `${wifi.rssi_dbm} dBm`], ['Channel', wifi.channel], ['Security', wifi.authentication]]) {
    const group = document.createElement('div');
    const term = document.createElement('dt'); term.textContent = label;
    const detail = document.createElement('dd'); detail.textContent = value;
    group.append(term, detail); diagnostics.append(group);
  }
}

document.querySelector('#reload-settings').addEventListener('click', () => load().catch(error => {
  summary.textContent = `Could not load settings: ${error.message}`;
}));
document.querySelector('#add-setting').addEventListener('click', () => {
  row({key: '', value: ''}, true);
  updateState();
  settings.querySelector('.setting-row:last-child .key').focus();
});
document.querySelector('#settings-form').addEventListener('submit', async event => {
  event.preventDefault();
  status.textContent = 'Saving…';
  const records = [...settings.querySelectorAll('.setting-row')]
    .filter(item => item.dataset.isNew === 'true' || item.dataset.originalValue !== item.querySelector('.value').value)
    .map(item => ({key:item.querySelector('.key').value.trim(),value:item.querySelector('.value').value}));
  try {
    for (const record of records) {
      const response = await fetch('/api/config', {method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(record)});
      if (!response.ok) throw new Error(await response.text());
    }
    await load();
    status.textContent = `${records.length} change${records.length === 1 ? '' : 's'} saved`;
  } catch (error) { status.textContent = `Save failed: ${error.message}`; }
});

load().catch(error => {
  document.querySelector('#device-status').textContent = `Connection failed: ${error.message}`;
  summary.textContent = `Could not load settings: ${error.message}`;
});
