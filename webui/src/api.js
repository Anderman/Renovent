function extractMenuItems(payload) {
  if (Array.isArray(payload)) {
    return payload;
  }

  if (Array.isArray(payload?.items)) {
    return payload.items;
  }

  if (Array.isArray(payload?.values)) {
    return payload.values;
  }

  if (Array.isArray(payload?.entries)) {
    return payload.entries;
  }

  return [];
}

export async function getJson(url, options = {}) {
  const response = await fetch(url, options);
  if (!response.ok) {
    throw new Error(`${response.status} ${response.statusText}`);
  }

  const text = await response.text();
  if (!text) {
    return {};
  }

  try {
    return JSON.parse(text);
  } catch {
    return { message: text };
  }
}

export function fetchStatusData() {
  return getJson('/api/status');
}

export function openDisplayStream(onDisplayText) {
  const url = `${window.location.protocol}//${window.location.hostname}:81/display-stream`;
  const source = new EventSource(url);
  source.addEventListener('display', (event) => {
    onDisplayText?.(event.data ?? '');
  });
  return source;
}

export function fetchKeyPressLogData() {
  return getJson('/api/key-press-log');
}

export function fetchTextLogData() {
  return getJson('/api/text-log');
}

export function fetchMqttConfigData() {
  return getJson('/api/mqtt/config');
}

export async function fetchParameterDefinitionsData() {
  const result = await getJson('/api/parameter-definitions');
  return extractMenuItems(result);
}

export async function fetchSettingsMenuData(previousCompletedMs = 0) {
  const schedule = await postSchedule('/api/settings-menu/read');
  if (!schedule.scheduled) {
    throw new Error('Instellingenmenu is al bezig');
  }

  const status = await poll(
    async () => fetchStatusData(),
    (result) => !result.settingsMenuRunning && Number(result.settingsMenuLastCompletedMs ?? 0) !== Number(previousCompletedMs ?? 0),
    20000,
    500
  );

  return {
    status,
    entries: Array.isArray(status.settingsMenuEntries) ? status.settingsMenuEntries : []
  };
}

export async function fetchSensorsMenuData(previousCompletedMs = 0) {
  const schedule = await postSchedule('/api/sensors-menu/start');
  if (!schedule.scheduled) {
    throw new Error('Sensormenu is al bezig');
  }

  return poll(
    async () => getJson('/api/sensors-menu'),
    (result) => !result.running && Number(result.lastCompletedMs ?? 0) !== Number(previousCompletedMs ?? 0),
    20000,
    500
  );
}

export function fetchLatestSensorsMenuData() {
  return getJson('/api/sensors-menu');
}

export async function postSetValue(id, value) {
  const response = await fetch('/api/set-value', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ key: id, value })
  });

  const text = await response.text();
  const payload = text ? JSON.parse(text) : {};
  if (!response.ok && response.status !== 409) {
    throw new Error(`${response.status} ${response.statusText}`);
  }

  return payload;
}

export function postKeyPress(keyMask, durationMs) {
  return getJson('/api/keys/press', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(durationMs ? { mask: keyMask, durationMs } : { mask: keyMask })
  });
}

export function postMqttConfigData(config) {
  return getJson('/api/mqtt/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(config)
  });
}

async function postSchedule(url) {
  const response = await fetch(url, { method: 'POST' });
  const text = await response.text();
  const payload = text ? JSON.parse(text) : {};
  if (!response.ok && response.status !== 409) {
    throw new Error(`${response.status} ${response.statusText}`);
  }

  return payload;
}

async function poll(fetcher, predicate, timeoutMs, intervalMs) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const result = await fetcher();
    if (predicate(result)) {
      return result;
    }

    await new Promise((resolve) => window.setTimeout(resolve, intervalMs));
  }

  throw new Error('Timeout tijdens wachten op apparaat');
}