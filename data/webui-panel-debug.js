import { settingCard } from "./webui-components.js";
import { escapeHtml } from "./webui-utils.js";
import { mapSensorEntries, mapSettingsEntries, mapStatusSnapshot } from "./webui-models.js";

export function buildDebugPanel() {
	return `
		<div class="advanced-grid">
			<section class="panel">
				<header>
					<h2>Display</h2>
				</header>
				<div class="display-box" id="display-value">--</div>
			</section>
			<section class="panel">
				<header>
					<h2>Toetsen</h2>
				</header>
				<div class="remote-pad">
					<div class="remote-orbit">
						<div class="remote-ring"></div>
						<button class="key-button remote-button remote-pos-top" type="button" data-key-mask="2" data-duration-ms="200">+</button>
						<button class="key-button remote-button remote-pos-upper-left" type="button" data-key-mask="3" data-duration-ms="800">ON</button>
						<button class="key-button remote-button remote-pos-left" type="button" data-key-mask="1" data-duration-ms="500">OK</button>
						<button class="key-button remote-button remote-pos-lower-left" type="button" data-key-mask="9" data-duration-ms="800">OFF</button>
						<button class="key-button remote-button remote-pos-bottom" type="button" data-key-mask="8" data-duration-ms="200">-</button>
						<button id="reset-button" class="key-button remote-button remote-pos-lower-right" type="button" data-key-mask="12" data-duration-ms="500">RESET</button>
						<button id="f-latch-button" class="key-button remote-button remote-pos-right remote-button-latch" type="button" data-latch-mask="4">F</button>
						<button class="key-button remote-button remote-pos-upper-right" type="button" data-key-mask="6" data-duration-ms="500">SET</button>
						<button id="f-ok-latch-button" class="key-button remote-button remote-pos-center remote-button-latch" type="button" data-latch-mask="5">F+OK</button>
					</div>
				</div>
			</section>
			<section class="panel">
				<header>
					<h2>Status</h2>
					<span id="status-note" class="note">Wachten op status...</span>
				</header>
				<div id="status-kv" class="kv"></div>
			</section>
			<section class="panel full">
				<header>
					<h2>Waarde schrijven</h2>
					<span class="note">Gebruik firmware-keys zoals U1 of I2</span>
				</header>
				<div class="input-grid">
					<label>Key
						<input id="set-value-key" type="text" list="parameter-key-list" placeholder="Bijv. U1" />
						<datalist id="parameter-key-list"></datalist>
					</label>
					<label>Waarde
						<input id="set-value-value" type="number" step="1" placeholder="Bijv. 3" />
					</label>
					<button id="set-value-button" type="button">Verstuur</button>
				</div>
				<div id="set-value-result" class="note"></div>
			</section>
			<section class="panel full">
				<header>
					<h2>Sensor scan</h2>
					<button id="start-sensors-menu" type="button">Start scan</button>
				</header>
				<div id="sensors-output"></div>
			</section>
			<section class="panel full">
				<header>
					<h2>Instellingenmenu</h2>
					<button id="read-settings-menu" type="button" class="secondary">Lees instellingen</button>
				</header>
				<div id="settings-grid" class="setting-grid"></div>
			</section>
		</div>`;
}

export function collectDebugElements(elements) {
	elements.statusKv = document.getElementById("status-kv");
	elements.statusNote = document.getElementById("status-note");
	elements.settingsGrid = document.getElementById("settings-grid");
	elements.readSettingsMenu = document.getElementById("read-settings-menu");
	elements.displayValue = document.getElementById("display-value");
	elements.sensorsOutput = document.getElementById("sensors-output");
	elements.startSensorsMenu = document.getElementById("start-sensors-menu");
	elements.setValueKey = document.getElementById("set-value-key");
	elements.setValueValue = document.getElementById("set-value-value");
	elements.setValueButton = document.getElementById("set-value-button");
	elements.setValueResult = document.getElementById("set-value-result");
	elements.parameterKeyList = document.getElementById("parameter-key-list");
	elements.latchedButtons = Array.from(document.querySelectorAll("[data-latch-mask]"));
}

export function renderDebugPanel(elements, state) {
	const status = state.status ?? {};
	const snapshot = mapStatusSnapshot(status);
	elements.statusNote.textContent = snapshot.activityText;
	elements.statusKv.innerHTML = snapshot.meta.map(([label, value]) => `<div>${escapeHtml(label)}</div><div>${escapeHtml(value)}</div>`).join("");

	const mappedSettings = mapSettingsEntries(status.settingsMenuEntries ?? [], state.parameterDefinitions ?? []);
	elements.settingsGrid.innerHTML = mappedSettings.length
		? mappedSettings.map((item) => settingCard({
			label: item.label,
			description: item.description,
			valueDisplay: item.valueDisplay,
			meta: [
				item.key,
				item.range,
				item.defaultValue ? `def ${item.defaultValue}` : ""
			]
		})).join("")
		: `<div class="note">Nog geen instellingen geladen.</div>`;

	elements.displayValue.textContent = state.status?.displayText ?? "--";
	elements.parameterKeyList.innerHTML = (state.parameterDefinitions ?? []).map((entry) =>
		`<option value="${escapeHtml(entry.key)}">${escapeHtml(entry.title ?? entry.key)}</option>`
	).join("");

	const rows = mapSensorEntries(state.sensorsPayload).map((entry) => `
		<tr>
			<td>${escapeHtml(entry.index)}</td>
			<td>${escapeHtml(entry.description)}</td>
			<td>${escapeHtml(entry.valueDisplay ?? "--")}</td>
			<td>${escapeHtml(entry.unit || "")}</td>
			<td>${escapeHtml(entry.remark || "")}</td>
		</tr>`).join("");

	elements.sensorsOutput.innerHTML = rows
		? `<table class="scan-table"><thead><tr><th>#</th><th>Omschrijving</th><th>Waarde</th><th>Unit</th><th>Detail</th></tr></thead><tbody>${rows}</tbody></table>`
		: `<div class="note">Nog geen sensorgegevens beschikbaar.</div>`;

	for (const button of elements.latchedButtons ?? []) {
		const latchMask = Number(button.dataset.latchMask);
		const isActive = state.latchedKeyMask === latchMask;
		button.classList.toggle("active", isActive);
		button.setAttribute("aria-pressed", isActive ? "true" : "false");
	}
}
