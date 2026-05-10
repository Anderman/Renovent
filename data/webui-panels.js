import { buildDebugPanel, collectDebugElements, renderDebugPanel } from "./webui-panel-debug.js";
import { buildHomePanel, collectHomeElements, renderHomePanel } from "./webui-panel-home.js";
import { buildLoggingPanel, collectLoggingElements, renderLoggingPanel, renderTextLog } from "./webui-panel-logging.js";
import { buildMqttPanel, collectMqttElements, renderMqttPanel } from "./webui-panel-mqtt.js";

export function mountPanels(elements) {
	elements.homePanel.innerHTML = buildHomePanel();
	elements.mqttPanel.innerHTML = buildMqttPanel();
	elements.debugPanel.innerHTML = buildDebugPanel();
	elements.loggingPanel.innerHTML = buildLoggingPanel();
	collectHomeElements(elements);
	collectMqttElements(elements);
	collectDebugElements(elements);
	collectLoggingElements(elements);
}

export function renderStatus(elements, state) {
	renderHomePanel(elements, state);
	renderMqttPanel(elements, state);
	renderDebugPanel(elements, state);
	renderLoggingPanel(elements, state);
}

export function renderSettings(elements, settings) {
	void elements;
	void settings;
}

export function renderSensors(elements, sensors) {
	void elements;
	void sensors;
}

export function renderLogs(elements, logs) {
	renderTextLog(elements, logs);
}