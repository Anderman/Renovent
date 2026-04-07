export function createDefinitionLookup(definitions) {
	return new Map(definitions.map((entry) => [entry.key, entry]));
}

function normalizeDisplayNumber(rawValue) {
	const compact = String(rawValue ?? "").replace(/\s+/g, "");
	const matches = compact.match(/[-0-9.]+/g);
	if (!matches?.length) {
		return "";
	}

	const token = matches[matches.length - 1];
	const negative = token.includes("-") || token.endsWith(".");
	let normalized = token.replace(/-/g, "");
	if (normalized.endsWith(".")) {
		normalized = normalized.slice(0, -1);
	}
	if (/^\.\d+$/.test(normalized)) {
		normalized = `0${normalized}`;
	}
	return negative ? `-${normalized}` : normalized;
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
				description: definition?.description ?? "",
				range: definition?.range ?? "",
				defaultValue: definition?.defaultValue ?? "",
				valueDisplay: normalizedDisplay || (entry.hasValue ? String(entry.value) : "--")
			};
		});
}

export function mapSensorEntries(payload) {
	const values = Array.isArray(payload?.values) ? payload.values : [];
	if (values.length) {
		return values.map((entry) => ({
			index: entry.index,
			description: entry.description ?? "",
			unit: entry.unit ?? "",
			remark: entry.remark ?? "",
			available: Boolean(entry.available),
			hasValue: Boolean(entry.hasValue),
			value: entry.value,
			valueDisplay: Boolean(entry.available) && Boolean(entry.hasValue) ? String(entry.value) : "--"
		}));
	}

	const entries = Array.isArray(payload?.entries) ? payload.entries : [];
	return entries.map((entry) => ({
		index: entry.step,
		description: entry.description ?? "",
		unit: "",
		remark: entry.remark ?? "",
		available: Boolean(entry.available),
		hasValue: Boolean(entry.hasValue),
		value: entry.value,
		valueDisplay: entry.available ? (entry.hasValue ? String(entry.value) : "--") : "--"
	}));
}

export function mapStatusSnapshot(status) {
	const coreDumpText = status.coreDumpPresent
		? `${status.coreDumpState ?? "present"} (${status.coreDumpSize ?? 0} B)`
		: (status.coreDumpState ?? "not-found");
	const activityText = status.settingWriterRunning
		? `Update setting ${status.settingWriterKey ?? ""}`.trim()
		: status.settingsMenuRunning
			? "Scan settings"
			: status.sensorsMenuRunning
				? "Scan sensors"
				: "Idle";
	const resetText = status.resetReason
		? `${status.resetReason}${status.resetRawReason !== undefined ? ` (${status.resetRawReason})` : ""}`
		: "--";
	const coreDumpBacktraceText = status.coreDumpReason
		? status.coreDumpReason
		: (status.coreDumpPresent ? "Niet beschikbaar in huidige firmware; seriele panic-uitvoer bevat de backtrace." : "--");
	const coreDumpAgeText = status.coreDumpPresent
		? "Onbekend; ESP-IDF geeft hier geen timestamp of leeftijd voor terug."
		: "--";

	return {
		displayText: status.displayText ?? "--",
		co2Text: status.co2Valid ? String(status.co2Ppm) : "--",
		temperatureText: status.co2Valid ? `${Number(status.co2TemperatureC ?? 0).toFixed(1)}` : "--",
		humidityText: status.co2Valid ? `${Number(status.co2HumidityPct ?? 0).toFixed(1)}` : "--",
		activityText,
		meta: [
			["Reset", resetText],
			["Reset detail", status.resetDetail ?? "--"],
			["Firmware", status.firmwareBuildId ?? "--"],
			["SPIFFS", status.spiffsBuildId ?? "--"],
			["RSSI", status.rssi !== undefined ? `${status.rssi} dBm` : "--"],
			["Actieve toets", status.activeKeys ?? "--"],
			["Laatste display", status.loggedDisplayText ?? "--"],
			["Auto-update", status.autoUpdateState ?? "--"],
			["Core dump", coreDumpText],
			["Core dump info", coreDumpBacktraceText],
			["Core dump leeftijd", coreDumpAgeText]
		]
	};
}
