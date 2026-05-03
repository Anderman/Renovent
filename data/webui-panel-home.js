import { metricCard } from "./webui-components.js";
import { formatUptime } from "./webui-utils.js";
import { mapStatusSnapshot } from "./webui-models.js";

export function buildHomePanel() {
	return `
		<div class="metrics">
			${metricCard("Display", "--", "Actuele display", "display-card")}
			${metricCard("CO2", "--", "ppm", "")}
			${metricCard("Temperatuur", "--", "°C", "")}
			${metricCard("RV", "--", "%", "")}
			${metricCard("Abs. vocht", "--", "g/m3", "")}
		</div>
		<article class="panel home-airflow-panel">
			<div class="panel-header">
				<div>
					<h2>Luchtstromen</h2>
					<p class="panel-subtitle">Toevoer en afvoer uit de laatste sensorscan</p>
				</div>
				<div id="home-airflow-status" class="home-airflow-status">Geen scan</div>
			</div>
			<div id="home-airflow-table"></div>
		</article>`;
}

export function collectHomeElements(elements) {
	elements.metricCards = Array.from(elements.homePanel.querySelectorAll(".metric-card"));
	elements.homeAirflowStatus = document.getElementById("home-airflow-status");
	elements.homeAirflowTable = document.getElementById("home-airflow-table");
}

export function renderHomePanel(elements, state) {
	const status = state.status ?? {};
	const snapshot = mapStatusSnapshot(status);
	updateMetricCard(elements, 0, snapshot.displayText, "Actuele display");
	updateMetricCard(elements, 1, snapshot.co2Text, "ppm");
	updateMetricCard(elements, 2, snapshot.temperatureText, "°C");
	updateMetricCard(elements, 3, snapshot.humidityText, "%");
	updateMetricCard(elements, 4, snapshot.absoluteHumidityText, "g/m3");
	renderAirflowTable(elements, state.sensorsPayload);

	elements.sidebarDisplay.textContent = snapshot.displayText;
	elements.metaUptime.textContent = formatUptime(status.uptimeMs ?? 0);
	elements.metaIp.textContent = status.rssi !== undefined ? `${status.rssi} dBm` : "--";
	elements.metaUpdated.textContent = snapshot.activityText;
}

function renderAirflowTable(elements, sensorsPayload) {
	if (!elements.homeAirflowTable || !elements.homeAirflowStatus) {
		return;
	}

	const values = createSensorValueLookup(sensorsPayload);
	const rows = [
		createAirflowRow(
			"Toevoer",
			formatSensorValue(values, "outside_temperature", "°C"),
			formatSensorValue(values, "incoming_temperature", "°C"),
			formatSensorValue(values, "supply_flow", "m3/h"),
			formatSensorValue(values, "supply_pressure", "Pa")
		),
		createAirflowRow(
			"Afvoer",
			formatSensorValue(values, "inside_temperature", "°C"),
			formatSensorValue(values, "outgoing_temperature", "°C"),
			formatSensorValue(values, "exhaust_flow", "m3/h"),
			formatSensorValue(values, "exhaust_pressure", "Pa")
		)
	];

	const hasAnyValue = rows.some((row) => row.temperatureFrom !== "--" || row.temperatureTo !== "--" || row.volume !== "--" || row.pressure !== "--");
	const completedMs = Number(sensorsPayload?.lastCompletedMs ?? 0);
	elements.homeAirflowStatus.textContent = completedMs > 0 ? `Scan ${new Date(completedMs).toLocaleTimeString("nl-NL", { hour: "2-digit", minute: "2-digit" })}` : "Geen scan";

	if (!hasAnyValue) {
		elements.homeAirflowTable.innerHTML = `<div class="note">Nog geen sensorgegevens beschikbaar. Start een sensorscan in Debug.</div>`;
		return;
	}

	const htmlRows = rows.map((row) => `
		<tr>
			<th scope="row">${row.label}</th>
			<td>${row.temperatureFrom}</td>
			<td>${row.temperatureTo}</td>
			<td>${row.volume}</td>
			<td>${row.pressure}</td>
		</tr>`).join("");

	elements.homeAirflowTable.innerHTML = `
		<table class="scan-table home-airflow-table">
			<thead>
				<tr>
					<th>Stroom</th>
					<th>Temperatuur van</th>
					<th>Temperatuur naar</th>
					<th>Volume</th>
					<th>Druk</th>
				</tr>
			</thead>
			<tbody>${htmlRows}</tbody>
		</table>`;
}

function createSensorValueLookup(sensorsPayload) {
	const values = Array.isArray(sensorsPayload?.values) ? sensorsPayload.values : [];
	return new Map(values.map((entry) => [entry.key, entry]));
}

function readSensorValue(values, key) {
	const entry = values.get(key);
	return entry && entry.available && entry.hasValue ? String(entry.value) : "--";
}

function formatSensorValue(values, key, unit) {
	const value = readSensorValue(values, key);
	return value === "--" ? value : `${value} ${unit}`;
}

function createAirflowRow(label, temperatureFrom, temperatureTo, volume, pressure) {
	return {
		label,
		temperatureFrom,
		temperatureTo,
		volume,
		pressure
	};
}

function updateMetricCard(elements, index, value, subtext) {
	const card = elements.metricCards[index];
	if (!card) {
		return;
	}
	const valueEl = card.querySelector(".metric-value");
	const subtextEl = card.querySelector(".metric-subtext");
	if (valueEl) {
		valueEl.textContent = value;
	}
	if (subtextEl) {
		subtextEl.textContent = subtext;
	}
}
