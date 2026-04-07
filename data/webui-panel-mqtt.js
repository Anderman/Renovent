import { escapeHtml } from "./webui-utils.js";

export function buildMqttPanel() {
	return `
		<div class="advanced-grid">
			<section class="panel wide">
				<header>
					<h2>MQTT instellingen</h2>
					<span id="mqtt-status-note" class="note">Configuratie laden...</span>
				</header>
				<div class="mqtt-form-grid">
					<label>MQTT nodeId
						<input id="mqtt-node-id" type="number" min="1" step="1" placeholder="1" />
					</label>
					<label>MQTT host
						<input id="mqtt-host" type="text" placeholder="broker.local" />
					</label>
					<label>MQTT port
						<input id="mqtt-port" type="number" min="1" max="65535" step="1" placeholder="1883" />
					</label>
					<label>MQTT user
						<input id="mqtt-user" type="text" placeholder="gebruiker" />
					</label>
					<label>MQTT wachtwoord
						<input id="mqtt-password" type="password" placeholder="Leeg laten om te behouden" />
					</label>
				</div>
				<div class="mqtt-form-footer">
					<button id="mqtt-save-button" type="button">Opslaan</button>
				</div>
			</section>
		</div>`;
}

export function collectMqttElements(elements) {
	elements.mqttStatusNote = document.getElementById("mqtt-status-note");
	elements.mqttNodeId = document.getElementById("mqtt-node-id");
	elements.mqttHost = document.getElementById("mqtt-host");
	elements.mqttPort = document.getElementById("mqtt-port");
	elements.mqttUser = document.getElementById("mqtt-user");
	elements.mqttPassword = document.getElementById("mqtt-password");
	elements.mqttSaveButton = document.getElementById("mqtt-save-button");
}

export function renderMqttPanel(elements, state) {
	const config = state.mqttConfig;
	if (!config) {
		elements.mqttStatusNote.textContent = "Configuratie nog niet geladen.";
		applyMqttFormDirtyState(elements, false);
		return;
	}

	elements.mqttStatusNote.textContent = state.mqttConfigDirty
		? "Niet opgeslagen wijzigingen"
		: (state.mqttConfigMessage || "Configuratie geladen");
	applyMqttFormDirtyState(elements, state.mqttConfigDirty);

	if (state.mqttConfigDirty) {
		return;
	}

	elements.mqttNodeId.value = config.mqttNodeId ?? "1";
	elements.mqttHost.value = config.mqttHost ?? "";
	elements.mqttPort.value = String(config.mqttPort ?? 1883);
	elements.mqttUser.value = config.mqttUser ?? "";
	elements.mqttPassword.value = "";
}

export function readMqttForm(elements) {
	return {
		mqttNodeId: String(elements.mqttNodeId.value ?? "").trim(),
		mqttHost: String(elements.mqttHost.value ?? "").trim(),
		mqttPort: Number(elements.mqttPort.value || 1883),
		mqttUser: String(elements.mqttUser.value ?? "").trim(),
		mqttPassword: String(elements.mqttPassword.value ?? "")
	};
}

export function validateMqttForm(config) {
	if (!config.mqttNodeId) {
		return "MQTT nodeId is verplicht.";
	}
	if (!/^[0-9]+$/.test(config.mqttNodeId)) {
		return "MQTT nodeId moet een getal zijn.";
	}
	if (!config.mqttHost) {
		return "MQTT host is verplicht.";
	}
	if (!Number.isInteger(config.mqttPort) || config.mqttPort < 1 || config.mqttPort > 65535) {
		return "MQTT port moet tussen 1 en 65535 liggen.";
	}
	return "";
}

export function applyMqttFormDirtyState(elements, dirty) {
	elements.mqttSaveButton.disabled = false;
	elements.mqttSaveButton.textContent = dirty ? "Opslaan" : "Opgeslagen";
}

export function mapMqttConfigResponse(payload) {
	return {
		mqttNodeId: payload.mqttNodeId ?? "1",
		mqttHost: payload.mqttHost ?? "",
		mqttPort: payload.mqttPort ?? 1883,
		mqttUser: payload.mqttUser ?? ""
	};
}
