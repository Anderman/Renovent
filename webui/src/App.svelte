<script>
  import { onMount } from 'svelte';
  import {
    fetchKeyPressLogData,
    fetchLatestSensorsMenuData,
    fetchMqttConfigData,
    fetchParameterDefinitionsData,
    fetchSensorsMenuData,
    fetchSettingsMenuData,
    fetchStatusData,
    fetchTextLogData,
    openDisplayStream,
    postKeyPress,
    postMqttConfigData,
    postSetValue
  } from './api.js';
  import {
    buildAirflowRows,
    formatTimestampMs,
    formatUptime,
    mapMqttConfigResponse,
    mapSensorEntries,
    mapSettingsEntries,
    mapStatusSnapshot,
    validateMqttForm
  } from './models.js';

  const THEME_KEY = 'renovent-theme';
  const REFRESH_INTERVAL_MS = 4000;
  const DEFAULT_LATCH_MS = 3200;
  const tabs = [
    { id: 'home', label: 'Overzicht' },
    { id: 'mqtt', label: 'MQTT' },
    { id: 'debug', label: 'Debug' },
    { id: 'logging', label: 'Logging' }
  ];
  const remoteButtons = [
    { label: 'ON', keyMask: 3, durationMs: 800 },
    { label: '+', keyMask: 2, durationMs: 200 },
    { label: 'SET', keyMask: 6, durationMs: 500 },
    { label: 'OK', keyMask: 1, durationMs: 500 },
    { label: 'F+OK', latchMask: 5 },
    { label: 'F', latchMask: 4 },
    { label: 'OFF', keyMask: 9, durationMs: 800 },
    { label: '-', keyMask: 8, durationMs: 200 },
    { label: 'RESET', keyMask: 12, durationMs: 500 }
  ];

  let activeTab = getInitialTab();
  let theme = getInitialTheme();
  let menuOpen = false;

  let status = null;
  let parameterDefinitions = [];
  let sensorsPayload = null;
  let textLog = null;
  let textLogLoading = false;
  let textLogError = '';
  let keyPressLog = null;
  let keyPressLogLoading = false;
  let keyPressLogError = '';

  let mqttNodeId = '1';
  let mqttHost = '';
  let mqttPort = 1883;
  let mqttUser = '';
  let mqttPassword = '';
  let mqttConfigDirty = false;
  let mqttConfigMessage = 'Configuratie laden...';

  let latchedKeyMask = 0;
  let latchedKeyTimerId = null;
  let setValueKey = '';
  let setValueValue = '';
  let setValueResult = '';

  $: snapshot = mapStatusSnapshot(status ?? {});
  $: settingsEntries = mapSettingsEntries(status?.settingsMenuEntries ?? [], parameterDefinitions);
  $: sensorEntries = mapSensorEntries(sensorsPayload);
  $: airflowRows = buildAirflowRows(sensorsPayload);
  $: airflowHasAnyValue = airflowRows.some((row) => row.temperatureFrom !== '--' || row.temperatureTo !== '--' || row.volume !== '--' || row.pressure !== '--');
  $: airflowCompletedText = Number(sensorsPayload?.lastCompletedMs ?? 0) > 0
    ? `Scan ${new Date(Number(sensorsPayload.lastCompletedMs)).toLocaleTimeString('nl-NL', { hour: '2-digit', minute: '2-digit' })}`
    : 'Geen scan';
  $: textLogEntries = Array.isArray(textLog?.entries) ? [...textLog.entries].reverse() : [];
  $: keyPressEntries = Array.isArray(keyPressLog?.entries) ? keyPressLog.entries : [];

  onMount(() => {
    applyTheme();
    void refreshAll();
    void loadMqttConfig();
    void loadParameterDefinitions();

    const refreshTimer = window.setInterval(refreshAll, REFRESH_INTERVAL_MS);
    const displayStream = openDisplayStream((displayText) => {
      status = status === null ? { displayText } : { ...status, displayText };
    });
    const handleHashChange = () => {
      setActiveTab(getInitialTab(), false);
    };

    window.addEventListener('hashchange', handleHashChange);

    return () => {
      window.clearInterval(refreshTimer);
      displayStream.close();
      window.removeEventListener('hashchange', handleHashChange);
      clearLatchedAutoRelease();
    };
  });

  function getInitialTab() {
    const hash = window.location.hash.replace('#', '');
    return tabs.some((tab) => tab.id === hash) ? hash : 'home';
  }

  function getInitialTheme() {
    const saved = window.localStorage.getItem(THEME_KEY);
    if (saved === 'light' || saved === 'dark') {
      return saved;
    }

    return window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
  }

  function applyTheme() {
    document.documentElement.classList.toggle('dark', theme === 'dark');
    document.documentElement.style.colorScheme = theme === 'dark' ? 'dark' : 'light';
    window.localStorage.setItem(THEME_KEY, theme);
  }

  function toggleTheme() {
    theme = theme === 'dark' ? 'light' : 'dark';
    applyTheme();
  }

  function setActiveTab(tab, updateHash = true) {
    activeTab = tabs.some((entry) => entry.id === tab) ? tab : 'home';
    if (updateHash) {
      window.location.hash = activeTab;
    }
    menuOpen = false;
    if (activeTab === 'logging') {
      void Promise.all([fetchTextLog(), fetchKeyPressLog()]);
    }
  }

  async function refreshAll() {
    await fetchStatus();
    await syncSensorsSnapshot();
    if (activeTab === 'logging') {
      await Promise.all([fetchTextLog(), fetchKeyPressLog()]);
    }
  }

  async function fetchStatus() {
    try {
      status = await fetchStatusData();
    } catch {
    }
  }

  async function syncSensorsSnapshot() {
    const completedMs = Number(status?.sensorsMenuLastCompletedMs ?? 0);
    const loadedMs = Number(sensorsPayload?.lastCompletedMs ?? 0);
    if (status?.sensorsMenuRunning) {
      return;
    }
    if (sensorsPayload !== null && (completedMs === 0 || completedMs === loadedMs)) {
      return;
    }

    try {
      sensorsPayload = await fetchLatestSensorsMenuData();
    } catch {
    }
  }

  async function fetchTextLog() {
    const isInitialLoad = textLog === null;
    textLogLoading = isInitialLoad;
    textLogError = '';
    try {
      textLog = await fetchTextLogData();
    } catch (error) {
      if (isInitialLoad) {
        textLog = null;
      }
      textLogError = `Firmware textlog fout: ${error.message}`;
    } finally {
      textLogLoading = false;
    }
  }

  async function fetchKeyPressLog() {
    const isInitialLoad = keyPressLog === null;
    keyPressLogLoading = isInitialLoad;
    keyPressLogError = '';
    try {
      keyPressLog = await fetchKeyPressLogData();
    } catch (error) {
      if (isInitialLoad) {
        keyPressLog = null;
      }
      keyPressLogError = `Firmware keylog fout: ${error.message}`;
    } finally {
      keyPressLogLoading = false;
    }
  }

  async function loadMqttConfig() {
    try {
      const config = mapMqttConfigResponse(await fetchMqttConfigData());
      mqttNodeId = config.mqttNodeId;
      mqttHost = config.mqttHost;
      mqttPort = config.mqttPort;
      mqttUser = config.mqttUser;
      mqttPassword = '';
      mqttConfigDirty = false;
      mqttConfigMessage = 'Configuratie geladen';
    } catch (error) {
      mqttConfigMessage = `MQTT fout: ${error.message}`;
    }
  }

  async function loadParameterDefinitions() {
    try {
      parameterDefinitions = await fetchParameterDefinitionsData();
    } catch {
    }
  }

  function markMqttDirty() {
    mqttConfigDirty = true;
    mqttConfigMessage = 'Niet opgeslagen wijzigingen';
  }

  async function saveMqttConfig() {
    const nextConfig = {
      mqttNodeId: String(mqttNodeId ?? '').trim(),
      mqttHost: String(mqttHost ?? '').trim(),
      mqttPort: Number(mqttPort || 1883),
      mqttUser: String(mqttUser ?? '').trim(),
      mqttPassword: String(mqttPassword ?? '')
    };

    const validationError = validateMqttForm(nextConfig);
    if (validationError) {
      mqttConfigMessage = validationError;
      return;
    }

    try {
      const savedConfig = mapMqttConfigResponse(await postMqttConfigData(nextConfig));
      mqttNodeId = savedConfig.mqttNodeId;
      mqttHost = savedConfig.mqttHost;
      mqttPort = savedConfig.mqttPort;
      mqttUser = savedConfig.mqttUser;
      mqttPassword = '';
      mqttConfigDirty = false;
      mqttConfigMessage = 'MQTT configuratie opgeslagen';
    } catch (error) {
      mqttConfigMessage = `MQTT opslaan fout: ${error.message}`;
    }
  }

  async function readSettingsMenu() {
    try {
      const result = await fetchSettingsMenuData(status?.settingsMenuLastCompletedMs ?? 0);
      status = result.status;
    } catch {
    }
  }

  async function startSensorsMenu() {
    try {
      sensorsPayload = await fetchSensorsMenuData(status?.sensorsMenuLastCompletedMs ?? 0);
      await fetchStatus();
    } catch {
    }
  }

  async function setValue() {
    const id = String(setValueKey ?? '').trim();
    const value = Number(setValueValue);
    if (!id || !Number.isFinite(value)) {
      setValueResult = 'Geef een geldige key en waarde op.';
      return;
    }

    try {
      const result = await postSetValue(id, value);
      const displayText = result.displayText ? ` Huidige display: ${result.displayText}.` : '';
      setValueResult = result.message ?? (result.scheduled ? 'Waarde verstuurd.' : `Schrijfactie geweigerd.${displayText}`);
    } catch (error) {
      setValueResult = error.message;
    }
  }

  async function pressKey(keyMask, durationMs) {
    try {
      if (!Number.isInteger(keyMask) || keyMask < 0 || keyMask > 15) {
        throw new Error('Ongeldige key mask');
      }
      if (latchedKeyMask !== 0) {
        await releaseLatchedKey();
      }
      await postKeyPress(keyMask, durationMs);
    } catch {
    }
  }

  async function toggleLatchedKey(keyMask) {
    try {
      if (latchedKeyMask === keyMask) {
        await releaseLatchedKey();
        return;
      }
      if (latchedKeyMask !== 0) {
        await releaseLatchedKey();
      }
      await postKeyPress(keyMask);
      latchedKeyMask = keyMask;
      scheduleLatchedAutoRelease(keyMask);
    } catch {
    }
  }

  async function releaseLatchedKey() {
    clearLatchedAutoRelease();
    await postKeyPress(0);
    latchedKeyMask = 0;
  }

  function scheduleLatchedAutoRelease(keyMask) {
    clearLatchedAutoRelease();
    latchedKeyTimerId = window.setTimeout(async () => {
      if (latchedKeyMask !== keyMask) {
        return;
      }
      try {
        await releaseLatchedKey();
      } catch {
      }
    }, DEFAULT_LATCH_MS);
  }

  function clearLatchedAutoRelease() {
    if (latchedKeyTimerId !== null) {
      window.clearTimeout(latchedKeyTimerId);
      latchedKeyTimerId = null;
    }
  }
</script>

<svelte:head>
  <title>Renovent dashboard</title>
</svelte:head>

<div class="min-h-screen bg-transparent text-slate-900 dark:text-slate-100">
  <div class="mx-auto flex min-h-screen max-w-[1600px] flex-col lg:flex-row">
    <div class={`fixed inset-0 z-30 bg-slate-950/50 backdrop-blur-sm transition ${menuOpen ? 'opacity-100' : 'pointer-events-none opacity-0'} lg:hidden`} role="presentation" on:click={() => (menuOpen = false)}></div>

    <aside class={`fixed left-0 top-0 z-40 h-full w-72 border-r border-slate-200/70 bg-white/90 p-5 backdrop-blur-xl transition dark:border-slate-800 dark:bg-slate-950/90 lg:static lg:block lg:h-auto lg:w-72 lg:translate-x-0 ${menuOpen ? 'translate-x-0' : '-translate-x-full'}`}>
      <div class="flex h-full flex-col gap-6 rounded-[2rem] border border-slate-200/70 bg-white/80 p-5 shadow-2xl shadow-slate-900/10 dark:border-slate-800 dark:bg-slate-900/70 dark:shadow-black/30">
        <div class="flex items-center justify-between gap-4">
          <div>
            <div class="text-2xl font-black uppercase tracking-[0.18em] text-slate-900 dark:text-white">Renovent</div>
            <div class="mt-1 text-sm text-slate-500 dark:text-slate-400">Svelte WebUI</div>
          </div>
          <button class="btn btn-secondary h-11 w-11 rounded-2xl p-0 lg:hidden" type="button" on:click={() => (menuOpen = false)}>×</button>
        </div>

        <nav class="flex flex-col gap-2">
          {#each tabs as tab}
            <button class={`flex items-center rounded-2xl px-4 py-3 text-left text-sm font-semibold transition ${activeTab === tab.id ? 'bg-emerald-600 text-white shadow-lg shadow-emerald-700/20' : 'text-slate-600 hover:bg-slate-100 dark:text-slate-300 dark:hover:bg-slate-800'}`} type="button" on:click={() => setActiveTab(tab.id)}>
              {tab.label}
            </button>
          {/each}
        </nav>

        <div class="mt-auto space-y-3">
          <button class="btn btn-secondary w-full" type="button" on:click={toggleTheme}>{theme === 'dark' ? 'Licht thema' : 'Donker thema'}</button>
          <div class="rounded-3xl border border-slate-200 bg-white/80 p-4 dark:border-slate-800 dark:bg-slate-950/80">
            <div class="text-xs font-semibold uppercase tracking-[0.18em] text-slate-500 dark:text-slate-400">Display</div>
            <div class="mt-2 font-mono text-2xl text-emerald-600 dark:text-emerald-400">{snapshot.displayText}</div>
          </div>
        </div>
      </div>
    </aside>

    <main class="flex-1 px-4 pb-8 pt-4 lg:px-8 lg:pb-10 lg:pt-6">
      <div class="mx-auto max-w-6xl space-y-6">
        <div class="flex items-center justify-between gap-3 sm:gap-4">
          <button class="btn btn-secondary h-11 w-11 shrink-0 rounded-2xl px-0 text-xl lg:hidden" type="button" aria-label="Menu openen" on:click={() => (menuOpen = true)}>☰</button>
          <div class="panel flex min-w-0 flex-1 items-center gap-3 overflow-hidden !px-3 !py-3 text-[13px] sm:hidden">
            <span class="shrink-0 whitespace-nowrap">📶 {status?.rssi !== undefined ? `${status.rssi} dBm` : '--'}</span>
            <span class="shrink-0 whitespace-nowrap">⏱ {formatUptime(status?.uptimeMs ?? 0)}</span>
            <span class="min-w-0 truncate">✳ {snapshot.activityText}</span>
          </div>
          <div class="hidden flex-1 gap-3 sm:grid sm:grid-cols-3">
            <div class="panel !p-4">
              <div class="muted">Uptime</div>
              <div class="mt-2 text-lg font-semibold">{formatUptime(status?.uptimeMs ?? 0)}</div>
            </div>
            <div class="panel !p-4">
              <div class="muted">Netwerk</div>
              <div class="mt-2 text-lg font-semibold">{status?.rssi !== undefined ? `${status.rssi} dBm` : '--'}</div>
            </div>
            <div class="panel !p-4">
              <div class="muted">Status</div>
              <div class="mt-2 text-lg font-semibold">{snapshot.activityText}</div>
            </div>
          </div>
        </div>

        {#if activeTab === 'home'}
          <section class="space-y-6">
            <div class="grid gap-4 md:grid-cols-2 xl:grid-cols-5">
              <div class="panel xl:col-span-1">
                <div class="muted">Display</div>
                <div class="mt-3 font-mono text-4xl text-emerald-600 dark:text-emerald-400">{snapshot.displayText}</div>
              </div>
              <div class="panel">
                <div class="muted">CO2</div>
                <div class="mt-3 text-4xl font-bold">{snapshot.co2Text}</div>
                <div class="mt-1 text-sm text-slate-500 dark:text-slate-400">ppm</div>
              </div>
              <div class="panel">
                <div class="muted">Temperatuur</div>
                <div class="mt-3 text-4xl font-bold">{snapshot.temperatureText}</div>
                <div class="mt-1 text-sm text-slate-500 dark:text-slate-400">°C</div>
              </div>
              <div class="panel">
                <div class="muted">RV</div>
                <div class="mt-3 text-4xl font-bold">{snapshot.humidityText}</div>
                <div class="mt-1 text-sm text-slate-500 dark:text-slate-400">%</div>
              </div>
              <div class="panel">
                <div class="muted">Abs. vocht</div>
                <div class="mt-3 text-4xl font-bold">{snapshot.absoluteHumidityText}</div>
                <div class="mt-1 text-sm text-slate-500 dark:text-slate-400">g/m3</div>
              </div>
            </div>

            <div class="panel">
              <div class="flex flex-col gap-3 sm:flex-row sm:items-end sm:justify-between">
                <div>
                  <h2 class="panel-title">Luchtstromen</h2>
                  <p class="muted mt-1">Toevoer en afvoer uit de laatste sensorscan</p>
                </div>
                <div class="chip">{airflowCompletedText}</div>
              </div>

              {#if airflowHasAnyValue}
                <div class="mt-5 overflow-x-auto">
                  <table class="min-w-full text-left text-sm">
                    <thead class="text-slate-500 dark:text-slate-400">
                      <tr class="border-b border-slate-200 dark:border-slate-800">
                        <th class="pb-3 pr-4">Stroom</th>
                        <th class="pb-3 pr-4">Temp van</th>
                        <th class="pb-3 pr-4">Temp naar</th>
                        <th class="pb-3 pr-4">Vol</th>
                        <th class="pb-3">Druk</th>
                      </tr>
                    </thead>
                    <tbody>
                      {#each airflowRows as row}
                        <tr class="border-b border-slate-100 dark:border-slate-900">
                          <th class="py-3 pr-4 font-semibold">{row.label}</th>
                          <td class="py-3 pr-4">{row.temperatureFrom}</td>
                          <td class="py-3 pr-4">{row.temperatureTo}</td>
                          <td class="py-3 pr-4">{row.volume}</td>
                          <td class="py-3">{row.pressure}</td>
                        </tr>
                      {/each}
                    </tbody>
                  </table>
                </div>
              {:else}
                <div class="mt-5 rounded-2xl border border-dashed border-slate-300 p-5 text-sm text-slate-500 dark:border-slate-700 dark:text-slate-400">Nog geen sensorgegevens beschikbaar. Start een sensorscan in Debug.</div>
              {/if}
            </div>
          </section>
        {/if}

        {#if activeTab === 'mqtt'}
          <section class="panel space-y-5">
            <div>
              <h2 class="panel-title">MQTT instellingen</h2>
              <p class="muted mt-1">{mqttConfigDirty ? 'Niet opgeslagen wijzigingen' : mqttConfigMessage}</p>
            </div>
            <div class="grid gap-4 md:grid-cols-2 xl:grid-cols-3">
              <label class="space-y-2 text-sm font-medium">
                <span>MQTT nodeId</span>
                <input class="input" type="number" min="1" step="1" bind:value={mqttNodeId} on:input={markMqttDirty} />
              </label>
              <label class="space-y-2 text-sm font-medium">
                <span>MQTT host</span>
                <input class="input" type="text" bind:value={mqttHost} on:input={markMqttDirty} />
              </label>
              <label class="space-y-2 text-sm font-medium">
                <span>MQTT port</span>
                <input class="input" type="number" min="1" max="65535" step="1" bind:value={mqttPort} on:input={markMqttDirty} />
              </label>
              <label class="space-y-2 text-sm font-medium">
                <span>MQTT user</span>
                <input class="input" type="text" bind:value={mqttUser} on:input={markMqttDirty} />
              </label>
              <label class="space-y-2 text-sm font-medium md:col-span-2 xl:col-span-2">
                <span>MQTT wachtwoord</span>
                <input class="input" type="password" bind:value={mqttPassword} on:input={markMqttDirty} />
              </label>
            </div>
            <button class="btn" type="button" on:click={saveMqttConfig}>{mqttConfigDirty ? 'Opslaan' : 'Opgeslagen'}</button>
          </section>
        {/if}

        {#if activeTab === 'debug'}
          <section class="grid min-w-0 gap-6 xl:grid-cols-[1.1fr_1fr]">
            <div class="min-w-0 space-y-6">
              <div class="panel">
                <h2 class="panel-title">Display</h2>
                <div class="mt-4 overflow-x-auto rounded-3xl bg-slate-950 px-4 py-6 font-mono text-2xl tracking-[0.18em] text-emerald-300 shadow-inner sm:px-6 sm:py-8 sm:text-4xl sm:tracking-[0.25em]">{snapshot.displayText}</div>
              </div>

              <div class="panel">
                <div class="flex flex-col gap-3 sm:flex-row sm:items-center sm:justify-between">
                  <h2 class="panel-title">Toetsen</h2>
                  <div class="chip">Latch {latchedKeyMask === 0 ? 'uit' : `0x${latchedKeyMask.toString(16)}`}</div>
                </div>
                <div class="mt-5 grid grid-cols-2 gap-3 sm:grid-cols-3">
                  {#each remoteButtons as button}
                    <button
                      class={`min-w-0 rounded-2xl px-3 py-4 text-sm font-bold transition sm:px-4 ${button.latchMask && latchedKeyMask === button.latchMask ? 'bg-amber-500 text-slate-950' : 'bg-slate-100 text-slate-900 hover:bg-slate-200 dark:bg-slate-800 dark:text-slate-50 dark:hover:bg-slate-700'}`}
                      type="button"
                      on:click={() => (button.latchMask ? toggleLatchedKey(button.latchMask) : pressKey(button.keyMask, button.durationMs))}
                    >
                      {button.label}
                    </button>
                  {/each}
                </div>
              </div>

              <div class="panel space-y-4">
                <div class="flex items-center justify-between gap-4">
                  <div>
                    <h2 class="panel-title">Waarde schrijven</h2>
                    <p class="muted mt-1">Gebruik firmware-keys zoals U1 of I2</p>
                  </div>
                </div>
                <div class="grid gap-4 md:grid-cols-[1fr_1fr_auto]">
                  <label class="space-y-2 text-sm font-medium">
                    <span>Key</span>
                    <input class="input" type="text" list="parameter-key-list" bind:value={setValueKey} placeholder="Bijv. U1" />
                    <datalist id="parameter-key-list">
                      {#each parameterDefinitions as entry}
                        <option value={entry.key}>{entry.title ?? entry.key}</option>
                      {/each}
                    </datalist>
                  </label>
                  <label class="space-y-2 text-sm font-medium">
                    <span>Waarde</span>
                    <input class="input" type="number" step="1" bind:value={setValueValue} placeholder="Bijv. 3" />
                  </label>
                  <div class="flex items-end">
                    <button class="btn w-full md:w-auto" type="button" on:click={setValue}>Verstuur</button>
                  </div>
                </div>
                <div class="muted">{setValueResult}</div>
              </div>
            </div>

            <div class="min-w-0 space-y-6">
              <div class="panel">
                <div class="flex flex-col gap-3 sm:flex-row sm:items-end sm:justify-between">
                  <div>
                    <h2 class="panel-title">Status</h2>
                    <p class="muted mt-1">{snapshot.activityText}</p>
                  </div>
                </div>
                <div class="mt-5 grid gap-3 text-sm md:grid-cols-[auto_1fr]">
                  {#each snapshot.meta as [label, value]}
                    <div class="font-semibold text-slate-500 dark:text-slate-400">{label}</div>
                    <div class="break-all">{value}</div>
                  {/each}
                </div>
              </div>

              <div class="panel space-y-4">
                <div class="flex flex-col gap-3 sm:flex-row sm:items-center sm:justify-between">
                  <div>
                    <h2 class="panel-title">Sensor scan</h2>
                    <p class="muted mt-1">Start een nieuwe scan en lees de actuele waardes uit.</p>
                  </div>
                  <button class="btn w-full sm:w-auto" type="button" on:click={startSensorsMenu}>Start scan</button>
                </div>
                <div class="min-w-0 overflow-x-auto">
                  {#if sensorEntries.length}
                    <table class="min-w-[42rem] text-left text-sm sm:min-w-full">
                      <thead class="text-slate-500 dark:text-slate-400">
                        <tr class="border-b border-slate-200 dark:border-slate-800">
                          <th class="pb-3 pr-4">#</th>
                          <th class="pb-3 pr-4">Omschrijving</th>
                          <th class="pb-3 pr-4">Waarde</th>
                          <th class="pb-3 pr-4">Unit</th>
                          <th class="pb-3">Detail</th>
                        </tr>
                      </thead>
                      <tbody>
                        {#each sensorEntries as entry}
                          <tr class="border-b border-slate-100 dark:border-slate-900">
                            <td class="py-3 pr-4">{entry.index}</td>
                            <td class="py-3 pr-4">{entry.description}</td>
                            <td class="py-3 pr-4">{entry.valueDisplay}</td>
                            <td class="py-3 pr-4">{entry.unit}</td>
                            <td class="py-3">{entry.remark}</td>
                          </tr>
                        {/each}
                      </tbody>
                    </table>
                  {:else}
                    <div class="rounded-2xl border border-dashed border-slate-300 p-5 text-sm text-slate-500 dark:border-slate-700 dark:text-slate-400">Nog geen sensorgegevens beschikbaar.</div>
                  {/if}
                </div>
              </div>

              <div class="panel space-y-4">
                <div class="flex flex-col gap-3 sm:flex-row sm:items-center sm:justify-between">
                  <div>
                    <h2 class="panel-title">Instellingenmenu</h2>
                    <p class="muted mt-1">Lees het instellingenmenu van het apparaat opnieuw uit.</p>
                  </div>
                  <button class="btn w-full sm:w-auto" type="button" on:click={readSettingsMenu}>Lees instellingen</button>
                </div>
                {#if settingsEntries.length}
                  <div class="grid gap-4 sm:grid-cols-2">
                    {#each settingsEntries as entry}
                      <article class="rounded-3xl border border-slate-200 bg-slate-50 p-4 dark:border-slate-800 dark:bg-slate-950/70">
                        <div class="text-xs font-semibold uppercase tracking-[0.18em] text-slate-500 dark:text-slate-400">{entry.key}</div>
                        <h3 class="mt-2 text-base font-semibold">{entry.label}</h3>
                        <div class="mt-3 text-3xl font-bold text-emerald-600 dark:text-emerald-400">{entry.valueDisplay}</div>
                        {#if entry.description}
                          <p class="mt-3 text-sm text-slate-500 dark:text-slate-400">{entry.description}</p>
                        {/if}
                        <div class="mt-4 flex flex-wrap gap-2 text-xs">
                          {#if entry.range}<span class="chip">{entry.range}</span>{/if}
                          {#if entry.defaultValue}<span class="chip">def {entry.defaultValue}</span>{/if}
                        </div>
                      </article>
                    {/each}
                  </div>
                {:else}
                  <div class="rounded-2xl border border-dashed border-slate-300 p-5 text-sm text-slate-500 dark:border-slate-700 dark:text-slate-400">Nog geen instellingen geladen.</div>
                {/if}
              </div>
            </div>
          </section>
        {/if}

        {#if activeTab === 'logging'}
          <section class="grid gap-6 xl:grid-cols-2">
            <div class="panel">
              <div class="flex items-end justify-between gap-4">
                <div>
                  <h2 class="panel-title">Firmware textlog</h2>
                  <p class="muted mt-1">400 regels uit de ESP32 backend</p>
                </div>
                <button class="btn btn-secondary" type="button" on:click={() => Promise.all([fetchTextLog(), fetchKeyPressLog()])}>Ververs</button>
              </div>
              {#if textLogLoading}
                <div class="mt-5 muted">Firmware textlog laden...</div>
              {:else if textLogError}
                <div class="mt-5 rounded-2xl border border-rose-400/40 bg-rose-50 p-4 text-sm text-rose-700 dark:bg-rose-950/30 dark:text-rose-300">{textLogError}</div>
              {:else if textLogEntries.length}
                <div class="mt-5">
                  <div class="muted mb-3">{textLogEntries.length} regels via /api/text-log</div>
                  <div class="max-h-[34rem] overflow-auto rounded-3xl border border-slate-200 bg-slate-50 dark:border-slate-800 dark:bg-slate-950/80">
                    {#each textLogEntries as entry}
                      <div class="grid grid-cols-[6rem_1fr] gap-4 border-b border-slate-200 px-4 py-3 font-mono text-sm dark:border-slate-800">
                        <div class="text-slate-500 dark:text-slate-400">{formatTimestampMs(entry.timestampMs)}</div>
                        <div class="break-all">{entry.message}</div>
                      </div>
                    {/each}
                  </div>
                </div>
              {:else}
                <div class="mt-5 rounded-2xl border border-dashed border-slate-300 p-5 text-sm text-slate-500 dark:border-slate-700 dark:text-slate-400">Nog geen firmware textlog beschikbaar.</div>
              {/if}
            </div>

            <div class="panel">
              <div>
                <h2 class="panel-title">Firmware keylog</h2>
                <p class="muted mt-1">Via /api/key-press-log</p>
              </div>
              {#if keyPressLogLoading}
                <div class="mt-5 muted">Firmware keylog laden...</div>
              {:else if keyPressLogError}
                <div class="mt-5 rounded-2xl border border-rose-400/40 bg-rose-50 p-4 text-sm text-rose-700 dark:bg-rose-950/30 dark:text-rose-300">{keyPressLogError}</div>
              {:else if keyPressEntries.length}
                <div class="mt-5 overflow-auto">
                  <div class="muted mb-3">{keyPressEntries.length} regels via /api/key-press-log</div>
                  <table class="min-w-full text-left text-sm">
                    <thead class="text-slate-500 dark:text-slate-400">
                      <tr class="border-b border-slate-200 dark:border-slate-800">
                        <th class="pb-3 pr-4">Event</th>
                        <th class="pb-3 pr-4">Toetsen</th>
                        <th class="pb-3 pr-4">Display</th>
                        <th class="pb-3 pr-4">Relatief</th>
                        <th class="pb-3">Idle</th>
                      </tr>
                    </thead>
                    <tbody>
                      {#each keyPressEntries as entry}
                        <tr class="border-b border-slate-100 dark:border-slate-900">
                          <td class="py-3 pr-4">{entry.event ?? ''}</td>
                          <td class="py-3 pr-4">{entry.keys ?? ''}</td>
                          <td class="py-3 pr-4">{entry.display ?? ''}</td>
                          <td class="py-3 pr-4">{entry.relativeMs ?? 0} ms</td>
                          <td class="py-3">{entry.idleBeforeMs ?? 0} ms</td>
                        </tr>
                      {/each}
                    </tbody>
                  </table>
                </div>
              {:else}
                <div class="mt-5 rounded-2xl border border-dashed border-slate-300 p-5 text-sm text-slate-500 dark:border-slate-700 dark:text-slate-400">Nog geen firmware keylog beschikbaar.</div>
              {/if}
            </div>
          </section>
        {/if}
      </div>
    </main>
  </div>
</div>