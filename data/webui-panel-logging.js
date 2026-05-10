import { escapeHtml } from "./webui-utils.js";

export function buildLoggingPanel() {
	return `
		<div class="advanced-grid">
			<section class="panel wide">
				<header>
					<h2>UI textlog</h2>
					<span class="note">Laatste 400 regels uit de webinterface</span>
				</header>
				<div id="text-log-output" class="text-log-output"></div>
			</section>
			<section class="panel wide">
				<header>
					<h2>Firmware keylog</h2>
					<span class="note">Via aparte /api/key-press-log call</span>
				</header>
				<div id="firmware-log-output" class="kv"></div>
			</section>
		</div>`;
}

export function collectLoggingElements(elements) {
	elements.textLogOutput = document.getElementById("text-log-output");
	elements.firmwareLogOutput = document.getElementById("firmware-log-output");
}

export function renderLoggingPanel(elements, state) {
	renderTextLog(elements, state.logs);
	renderFirmwareLog(elements, state);
}

export function renderTextLog(elements, logs) {
	const textLogs = Array.isArray(logs) ? logs : [];
	elements.textLogOutput.innerHTML = textLogs.length
		? `
			<div class="note">${escapeHtml(`${textLogs.length} regels in UI textlog`)}</div>
			<div class="text-log-viewer">${textLogs.map((entry) => `
				<div class="text-log-line">
					<span class="text-log-time">${escapeHtml(entry.time ?? "--:--:--")}</span>
					<span class="text-log-message">${escapeHtml(entry.message ?? "")}</span>
				</div>`).join("")}
			</div>`
		: `<div class="note">Nog geen UI logregels beschikbaar.</div>`;
}

function renderFirmwareLog(elements, state) {
	if (state.keyPressLogLoading) {
		elements.firmwareLogOutput.innerHTML = `<div class="note">Firmware keylog laden...</div>`;
		return;
	}

	if (state.keyPressLogError) {
		elements.firmwareLogOutput.innerHTML = `<div class="note">${escapeHtml(state.keyPressLogError)}</div>`;
		return;
	}

	const firmwareLogs = Array.isArray(state.keyPressLog?.entries) ? state.keyPressLog.entries : [];
	const noteText = `${firmwareLogs.length} regels via /api/key-press-log`;

	elements.firmwareLogOutput.innerHTML = firmwareLogs.length
		? `
			<div class="note">${escapeHtml(noteText)}</div>
			<table class="scan-table firmware-log-table">
				<thead>
					<tr>
						<th>Event</th>
						<th>Toetsen</th>
						<th>Display</th>
						<th>Relatief</th>
						<th>Idle</th>
					</tr>
				</thead>
				<tbody>
					${firmwareLogs.map((entry) => {
						const eventName = String(entry.event ?? "").toLowerCase();
						const eventClass = getEventClass(eventName);
						return `
						<tr class="firmware-log-row ${eventClass}">
							<td>${escapeHtml(entry.event ?? "")}</td>
							<td>${escapeHtml(entry.keys ?? "")}</td>
							<td>${escapeHtml(entry.display ?? "")}</td>
							<td>${escapeHtml(`${entry.relativeMs ?? 0} ms`)}</td>
							<td>${escapeHtml(`${entry.idleBeforeMs ?? 0} ms`)}</td>
						</tr>`;
					}).join("")}
				</tbody>
			</table>`
		: `<div class="note">Nog geen firmware keylog beschikbaar.</div>`;
}

function getEventClass(eventName) {
	switch (eventName) {
		case "press":
		case "pressed":
			return "event-press";
		case "release":
		case "released":
			return "event-release";
		case "display":
			return "event-display";
		default:
			return "event-other";
	}
}
