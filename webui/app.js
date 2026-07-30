const settings = document.querySelector('#settings');
const template = document.querySelector('#setting-template');
const status = document.querySelector('#save-status');

function row(setting = {key: '', value: ''}) {
  const fragment = template.content.cloneNode(true);
  fragment.querySelector('.key').value = setting.key;
  fragment.querySelector('.value').value = setting.value;
  fragment.querySelector('.remove').addEventListener('click', event => event.target.closest('.setting-row').remove());
  settings.append(fragment);
}

async function load() {
  const [firmware, configuration, wifi] = await Promise.all([
    fetch('/api/firmware/info').then(response => response.json()),
    fetch('/api/config').then(response => response.json()),
    fetch('/api/wifi/diagnostics').then(response => response.json()),
  ]);
  document.querySelector('#device-status').textContent = `Firmware ${firmware.version} · ${wifi.ipv4_address || 'no network address'}`;
  settings.replaceChildren();
  configuration.settings.forEach(row);
  if (!configuration.settings.length) row();
  const diagnostics = document.querySelector('#wifi');
  diagnostics.replaceChildren();
  for (const [label, value] of [['Connected', wifi.connected ? 'Yes' : 'No'], ['RSSI', `${wifi.rssi_dbm} dBm`], ['Channel', wifi.channel], ['Security', wifi.authentication]]) {
    const group = document.createElement('div');
    const term = document.createElement('dt'); term.textContent = label;
    const detail = document.createElement('dd'); detail.textContent = value;
    group.append(term, detail); diagnostics.append(group);
  }
}

document.querySelector('#add-setting').addEventListener('click', () => row());
document.querySelector('#settings-form').addEventListener('submit', async event => {
  event.preventDefault();
  status.textContent = 'Saving…';
  const records = [...settings.querySelectorAll('.setting-row')].map(item => ({key:item.querySelector('.key').value,value:item.querySelector('.value').value}));
  try {
    for (const record of records) {
      const response = await fetch('/api/config', {method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(record)});
      if (!response.ok) throw new Error(await response.text());
    }
    status.textContent = 'Saved';
    await load();
  } catch (error) { status.textContent = `Save failed: ${error.message}`; }
});

load().catch(error => { document.querySelector('#device-status').textContent = `Connection failed: ${error.message}`; });
