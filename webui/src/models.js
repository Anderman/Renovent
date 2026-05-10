export function createDefinitionLookup(definitions) {
  return new Map(definitions.map((entry) => [entry.key, entry]));
}

function normalizeDisplayNumber(rawValue) {
  const compact = String(rawValue ?? '').replace(/\s+/g, '');
  const matches = compact.match(/[-0-9.]+/g);
  if (!matches?.length) {
    return '';
  }

  const token = matches[matches.length - 1];
  const negative = token.includes('-') || token.endsWith('.');
  let normalized = token.replace(/-/g, '');
  if (normalized.endsWith('.')) {
    normalized = normalized.slice(0, -1);
  }
  if (/^\.\d+$/.test(normalized)) {
    normalized = `0${normalized}`;
  }

  return negative ? `-${normalized}` : normalized;
}

function formatScaledValue(value, displayPrecision) {
  const numericValue = Number(value);
  const precision = Number(displayPrecision ?? 0);
  if (!Number.isFinite(numericValue) || !Number.isInteger(precision) || precision <= 0) {
    return String(value);
  }

  return (numericValue / (10 ** precision)).toFixed(precision);
}

export function mapSettingsEntries(entries, definitions) {
  const lookup = createDefinitionLookup(definitions);
  return entries
    .filter((entry) => entry && entry.available)
    .map((entry) => {
      const definition = lookup.get(entry.key) ?? null;
      const normalizedDisplay = normalizeDisplayNumber(entry.rawValue);
      return {
        key: entry.key,
        label: definition?.title ?? entry.key,
        description: definition?.description ?? '',
        range: definition?.range ?? '',
        defaultValue: definition?.defaultValue ?? '',
        valueDisplay: normalizedDisplay || (entry.hasValue ? String(entry.value) : '--')
      };
    });
}

export function mapSensorEntries(payload) {
  const values = Array.isArray(payload?.values) ? payload.values : [];
  if (values.length) {
    const mappedValues = values.map((entry) => ({
      index: entry.index,
      key: entry.key ?? '',
      description: entry.description ?? '',
      unit: entry.unit ?? '',
      remark: entry.remark ?? '',
      available: Boolean(entry.available),
      hasValue: Boolean(entry.hasValue),
      value: entry.value,
      valueDisplay: Boolean(entry.available) && Boolean(entry.hasValue)
        ? formatScaledValue(entry.value, entry.displayPrecision)
        : '--'
    }));

    const unknownEntries = Array.isArray(payload?.unknownEntries) ? payload.unknownEntries : [];
    const mappedUnknownEntries = unknownEntries.map((entry) => ({
      index: `? ${entry.key ?? ''}`.trim(),
      key: entry.key ?? '',
      description: `Onbekende sensor ${entry.key ?? ''}`.trim(),
      unit: '',
      remark: entry.rawValue ?? '',
      available: true,
      hasValue: Boolean(entry.hasValue),
      value: entry.value,
      valueDisplay: Boolean(entry.hasValue) ? String(entry.value) : (entry.rawValue ?? '--')
    }));

    return [...mappedValues, ...mappedUnknownEntries];
  }

  const entries = Array.isArray(payload?.entries) ? payload.entries : [];
  return entries.map((entry) => ({
    index: entry.step,
    key: entry.key ?? '',
    description: entry.description ?? '',
    unit: '',
    remark: entry.remark ?? '',
    available: Boolean(entry.available),
    hasValue: Boolean(entry.hasValue),
    value: entry.value,
    valueDisplay: entry.available ? (entry.hasValue ? String(entry.value) : '--') : '--'
  }));
}

export function mapStatusSnapshot(status) {
  const uptimeMs = Number(status.uptimeMs ?? 0);
  const lastCheckMs = Number(status.autoUpdateLastCheckMs ?? 0);
  const lastCheckDate = lastCheckMs > 0 && uptimeMs >= lastCheckMs
    ? new Date(Date.now() - (uptimeMs - lastCheckMs))
    : null;
  const lastCheckText = lastCheckDate
    ? lastCheckDate.toLocaleTimeString('nl-NL', { hour: '2-digit', minute: '2-digit', second: '2-digit' })
    : '--';
  const coreDumpText = status.coreDumpPresent
    ? `${status.coreDumpState ?? 'present'} (${status.coreDumpSize ?? 0} B)`
    : (status.coreDumpState ?? 'not-found');
  const activityText = status.settingWriterRunning
    ? `Update setting ${status.settingWriterKey ?? ''}`.trim()
    : status.settingsMenuRunning
      ? 'Scan settings'
      : status.sensorsMenuRunning
        ? 'Scan sensors'
        : 'Idle';
  const resetText = status.resetReason
    ? `${status.resetReason}${status.resetRawReason !== undefined ? ` (${status.resetRawReason})` : ''}`
    : '--';
  const backtraceSuffix = status.coreDumpBacktraceCorrupted ? ' (mogelijk incompleet)' : '';
  const coreDumpBacktraceText = status.coreDumpBacktrace
    ? `${status.coreDumpBacktrace}${backtraceSuffix}`
    : (status.coreDumpReason
      ? status.coreDumpReason
      : (status.coreDumpPresent ? 'Niet beschikbaar in huidige firmware; decodeer coredump host-side met de build ELF.' : '--'));

  return {
    displayText: status.displayText ?? '--',
    co2Text: status.co2Valid ? String(status.co2Ppm) : '--',
    temperatureText: status.co2Valid ? `${Number(status.co2TemperatureC ?? 0).toFixed(1)}` : '--',
    humidityText: status.co2Valid ? `${Number(status.co2HumidityPct ?? 0).toFixed(1)}` : '--',
    absoluteHumidityText: status.co2Valid ? `${Number(status.co2AbsoluteHumidityGm3 ?? 0).toFixed(1)}` : '--',
    activityText,
    meta: [
      ['Reset', resetText],
      ['Reset detail', status.resetDetail ?? '--'],
      ['Firmware', status.firmwareBuildId ?? '--'],
      ['SPIFFS', status.spiffsBuildId ?? '--'],
      ['RSSI', status.rssi !== undefined ? `${status.rssi} dBm` : '--'],
      ['Laatste OTA check', lastCheckText],
      ['OTA fout', status.autoUpdateLastError || '--'],
      ['Core dump', coreDumpText],
      ['Backtrace', coreDumpBacktraceText]
    ]
  };
}

export function formatUptime(value) {
  const totalMs = Number(value);
  if (!Number.isFinite(totalMs) || totalMs <= 0) {
    return '--';
  }

  const totalSeconds = Math.floor(totalMs / 1000);
  const days = Math.floor(totalSeconds / 86400);
  const hours = Math.floor((totalSeconds % 86400) / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  if (days > 0) {
    return `${days}d ${hours}u ${minutes}m`;
  }
  if (hours > 0) {
    return `${hours}u ${minutes}m`;
  }

  return `${minutes}m`;
}

export function formatTimestampMs(timestampMs) {
  const totalSeconds = Math.floor(Number(timestampMs ?? 0) / 1000);
  const hours = Math.floor(totalSeconds / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  const seconds = totalSeconds % 60;
  return [hours, minutes, seconds].map((value) => String(value).padStart(2, '0')).join(':');
}

export function mapMqttConfigResponse(payload) {
  return {
    mqttNodeId: payload.mqttNodeId ?? '1',
    mqttHost: payload.mqttHost ?? '',
    mqttPort: payload.mqttPort ?? 1883,
    mqttUser: payload.mqttUser ?? ''
  };
}

export function validateMqttForm(config) {
  if (!config.mqttNodeId) {
    return 'MQTT nodeId is verplicht.';
  }
  if (!/^[0-9]+$/.test(config.mqttNodeId)) {
    return 'MQTT nodeId moet een getal zijn.';
  }
  if (!config.mqttHost) {
    return 'MQTT host is verplicht.';
  }
  if (!Number.isInteger(config.mqttPort) || config.mqttPort < 1 || config.mqttPort > 65535) {
    return 'MQTT port moet tussen 1 en 65535 liggen.';
  }

  return '';
}

export function buildAirflowRows(sensorsPayload) {
  const values = new Map((Array.isArray(sensorsPayload?.values) ? sensorsPayload.values : []).map((entry) => [entry.key, entry]));
  const readValue = (key) => {
    const entry = values.get(key);
    return entry && entry.available && entry.hasValue ? String(entry.value) : '--';
  };
  const withUnit = (key, unit) => {
    const value = readValue(key);
    return value === '--' ? '--' : `${value} ${unit}`;
  };

  return [
    {
      label: 'Toevoer',
      temperatureFrom: withUnit('outside_temperature', '°C'),
      temperatureTo: withUnit('incoming_temperature', '°C'),
      volume: withUnit('supply_flow', 'm3/h'),
      pressure: withUnit('supply_pressure', 'Pa')
    },
    {
      label: 'Afvoer',
      temperatureFrom: withUnit('inside_temperature', '°C'),
      temperatureTo: withUnit('outgoing_temperature', '°C'),
      volume: withUnit('exhaust_flow', 'm3/h'),
      pressure: withUnit('exhaust_pressure', 'Pa')
    }
  ];
}