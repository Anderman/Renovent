import {
	fetchKeyPressLogData,
	fetchLatestSensorsMenuData,
	fetchMqttConfigData,
	fetchParameterDefinitionsData,
	fetchSensorsMenuData,
	fetchSettingsMenuData,
	fetchStatusData,
	postMqttConfigData,
	postKeyPress,
	postSetValue
} from "./webui-api.js";
import { createAppShell, createMenu, icons } from "./webui-components.js";
import {
	mountPanels as mountPanelMarkup,
	renderLogs as renderLogPanel,
	renderSensors as renderSensorPanel,
	renderSettings as renderSettingsPanel,
	renderStatus as renderStatusPanel
} from "./webui-panels.js";
import { applyMqttFormDirtyState, mapMqttConfigResponse, readMqttForm, validateMqttForm } from "./webui-panel-mqtt.js";
import { formatClockTime, getInitialTab, getInitialTheme } from "./webui-utils.js";

const THEME_KEY = "renovent-theme";
const REFRESH_INTERVAL_MS = 4000;
const DEFAULT_LATCH_MS = 3200;
const menuItems = [
	{ id: "home", label: "Overzicht", icon: icons.home },
	{ id: "mqtt", label: "MQTT", icon: icons.mqtt },
	{ id: "debug", label: "Debug", icon: icons.debug },
	{ id: "logging", label: "Logging", icon: icons.log }
];

const state = {
	activeTab: getInitialTab(menuItems),
	theme: getInitialTheme(THEME_KEY),
	menuOpen: false,
	status: null,
	keyPressLog: null,
	keyPressLogLoading: false,
	keyPressLogError: "",
	mqttConfig: null,
	mqttConfigDirty: false,
	mqttConfigMessage: "",
	parameterDefinitions: [],
	sensorsPayload: null,
	latchedKeyMask: 0,
	latchedKeyTimerId: null,
	logs: [],
	refreshTimer: null,
	lastUpdated: null
};

const elements = {};
const app = document.getElementById("app");
app.innerHTML = createAppShell();

collectElements();
mountPanels();
renderMenu();
renderThemeButtons();
applyTheme();
bindEvents();
renderAll();
refreshAll();
loadMqttConfig();
loadParameterDefinitions();
state.refreshTimer = window.setInterval(refreshAll, REFRESH_INTERVAL_MS);

function collectElements() {
	elements.body = document.body;
	elements.sidebar = document.getElementById("sidebar");
	elements.sidebarBackdrop = document.getElementById("sidebar-backdrop");
	elements.menuNav = document.getElementById("menu-nav");
	elements.menuOpenButton = document.getElementById("menu-open-button");
	elements.menuCloseButton = document.getElementById("menu-close-button");
	elements.themeToggle = document.getElementById("theme-toggle");
	elements.mobileThemeToggle = document.getElementById("mobile-theme-toggle");
	elements.sidebarDisplay = document.getElementById("sidebar-display");
	elements.metaUptime = document.getElementById("meta-uptime");
	elements.metaIp = document.getElementById("meta-ip");
	elements.metaUpdated = document.getElementById("meta-updated");
	elements.homePanel = document.getElementById("panel-home");
	elements.mqttPanel = document.getElementById("panel-mqtt");
	elements.debugPanel = document.getElementById("panel-debug");
	elements.loggingPanel = document.getElementById("panel-logging");
}

function mountPanels() {
	mountPanelMarkup(elements);
}

function bindEvents() {
	elements.menuNav.addEventListener("click", (event) => {
		const button = event.target.closest("[data-tab]");
		if (!button) {
			return;
		}
		setActiveTab(button.dataset.tab);
	});

	elements.menuOpenButton.addEventListener("click", () => setMenuOpen(true));
	elements.menuCloseButton.addEventListener("click", () => setMenuOpen(false));
	elements.sidebarBackdrop.addEventListener("click", () => setMenuOpen(false));
	elements.themeToggle.addEventListener("click", toggleTheme);
	elements.mobileThemeToggle.addEventListener("click", toggleTheme);
	elements.mqttSaveButton.addEventListener("click", saveMqttConfig);
	elements.readSettingsMenu.addEventListener("click", readSettingsMenu);
	elements.startSensorsMenu.addEventListener("click", startSensorsMenu);
	elements.setValueButton.addEventListener("click", setValue);

	[
		elements.mqttNodeId,
		elements.mqttHost,
		elements.mqttPort,
		elements.mqttUser,
		elements.mqttPassword
	].forEach((input) => {
		input.addEventListener("input", () => {
			state.mqttConfigDirty = true;
			state.mqttConfigMessage = "Niet opgeslagen wijzigingen";
			applyMqttFormDirtyState(elements, true);
			renderStatus();
		});
	});

	document.addEventListener("click", (event) => {
		const button = event.target.closest(".key-button");
		if (!button) {
			return;
		}
		const latchMask = button.dataset.latchMask ? Number(button.dataset.latchMask) : undefined;
		if (latchMask !== undefined) {
			toggleLatchedKey(latchMask);
			return;
		}
		const keyMask = Number(button.dataset.keyMask);
		const durationMs = button.dataset.durationMs ? Number(button.dataset.durationMs) : undefined;
		pressKey(keyMask, durationMs);
	});

	window.addEventListener("hashchange", () => {
		setActiveTab(getInitialTab(menuItems), false);
	});
}

function renderAll() {
	renderPanels();
	renderStatus();
	renderSettings();
	renderSensors();
	renderLogs();
}

function renderPanels() {
	menuItems.forEach((item) => {
		const panel = document.getElementById(`panel-${item.id}`);
		panel.classList.toggle("active", item.id === state.activeTab);
	});
	renderMenu();
	setMenuOpen(false);
}

function renderMenu() {
	elements.menuNav.innerHTML = createMenu(menuItems, state.activeTab);
}

function renderThemeButtons() {
	const dark = state.theme === "dark";
	const label = dark ? "Licht" : "Donker";
	const icon = dark ? icons.sun() : icons.moon();
	elements.themeToggle.innerHTML = `${icon}<span>${label}</span>`;
	elements.mobileThemeToggle.innerHTML = dark ? icons.sun() : icons.moon();
}

function renderStatus() {
	renderStatusPanel(elements, state);
}

function renderSettings() {
	renderSettingsPanel(elements, state.status?.settingsMenuEntries ?? []);
}

function renderSensors() {
	renderSensorPanel(elements, state.sensorsPayload);
}

function renderLogs() {
	renderLogPanel(elements, state.logs);
}

async function refreshAll() {
	await fetchStatus();
	await syncSensorsSnapshot();
	if (state.activeTab === "logging") {
		await fetchKeyPressLog();
	}
	renderAll();
}

async function fetchStatus() {
	try {
		const status = await fetchStatusData();
		state.status = status;
		state.lastUpdated = Date.now();
		logAction("Status ververst");
	} catch (error) {
		logAction(`Status fout: ${error.message}`);
	}
}

async function syncSensorsSnapshot() {
	const completedMs = Number(state.status?.sensorsMenuLastCompletedMs ?? 0);
	const loadedMs = Number(state.sensorsPayload?.lastCompletedMs ?? 0);
	if (state.status?.sensorsMenuRunning) {
		return;
	}

	if (state.sensorsPayload !== null && (completedMs === 0 || completedMs === loadedMs)) {
		return;
	}

	try {
		state.sensorsPayload = await fetchLatestSensorsMenuData();
	} catch (error) {
		logAction(`Sensorcache fout: ${error.message}`);
	}
}

async function fetchKeyPressLog() {
	state.keyPressLogLoading = true;
	state.keyPressLogError = "";
	try {
		state.keyPressLog = await fetchKeyPressLogData();
	} catch (error) {
		state.keyPressLog = null;
		state.keyPressLogError = `Firmware keylog fout: ${error.message}`;
		logAction(`Firmware keylog fout: ${error.message}`);
	} finally {
		state.keyPressLogLoading = false;
	}
}

async function loadMqttConfig() {
	try {
		state.mqttConfig = mapMqttConfigResponse(await fetchMqttConfigData());
		state.mqttConfigDirty = false;
		state.mqttConfigMessage = "Configuratie geladen";
		applyMqttFormDirtyState(elements, false);
		renderStatus();
		logAction("MQTT configuratie geladen");
	} catch (error) {
		state.mqttConfigMessage = `MQTT fout: ${error.message}`;
		applyMqttFormDirtyState(elements, false);
		renderStatus();
		logAction(`MQTT configuratie fout: ${error.message}`);
	}
}

async function readSettingsMenu() {
	try {
		const result = await fetchSettingsMenuData(state.status?.settingsMenuLastCompletedMs ?? 0);
		state.status = result.status;
		logAction(`Instellingen gelezen (${result.entries.length})`);
		renderStatus();
		renderSettings();
	} catch (error) {
		logAction(`Instellingen fout: ${error.message}`);
		renderSettings();
	}
}

async function startSensorsMenu() {
	try {
		state.sensorsPayload = await fetchSensorsMenuData(state.status?.sensorsMenuLastCompletedMs ?? 0);
		logAction(`Sensoren gelezen (${state.sensorsPayload?.entries?.length ?? 0})`);
		await fetchStatus();
		renderStatus();
		renderSensors();
	} catch (error) {
		logAction(`Sensoren fout: ${error.message}`);
		renderSensors();
	}
}

async function setValue() {
	const id = String(elements.setValueKey.value ?? "").trim();
	const value = Number(elements.setValueValue.value);
	if (!id || !Number.isFinite(value)) {
		elements.setValueResult.textContent = "Geef een geldige key en waarde op.";
		return;
	}

	try {
		const result = await postSetValue(id, value);
		const displayText = result.displayText ? ` Huidige display: ${result.displayText}.` : "";
		elements.setValueResult.textContent = result.message
			?? (result.scheduled ? "Waarde verstuurd." : `Schrijfactie geweigerd.${displayText}`);
		if (result.scheduled) {
			logAction(`Waarde gezet: ${id}=${value}`);
		} else {
			logAction(`Set value geweigerd: ${result.reason ?? "unknown"}${displayText}`);
		}
	} catch (error) {
		elements.setValueResult.textContent = error.message;
		logAction(`Set value fout: ${error.message}`);
	}
}

async function saveMqttConfig() {
	const nextConfig = readMqttForm(elements);
	const validationError = validateMqttForm(nextConfig);
	if (validationError) {
		state.mqttConfigMessage = validationError;
		renderStatus();
		return;
	}

	try {
		applyMqttFormDirtyState(elements, true);
		const savedConfig = await postMqttConfigData(nextConfig);
		state.mqttConfig = mapMqttConfigResponse(savedConfig);
		state.mqttConfigDirty = false;
		state.mqttConfigMessage = "MQTT configuratie opgeslagen";
		applyMqttFormDirtyState(elements, false);
		renderStatus();
		logAction("MQTT configuratie opgeslagen");
	} catch (error) {
		state.mqttConfigMessage = `MQTT opslaan fout: ${error.message}`;
		applyMqttFormDirtyState(elements, false);
		renderStatus();
		logAction(`MQTT opslaan fout: ${error.message}`);
	}
}

async function pressKey(keyMask, durationMs) {
	try {
		if (!Number.isInteger(keyMask) || keyMask < 0 || keyMask > 15) {
			throw new Error("Ongeldige key mask");
		}
		if (state.latchedKeyMask !== 0) {
			await releaseLatchedKey();
		}
		await postKeyPress(keyMask, durationMs);
		logAction(`Toets gestuurd: 0x${keyMask.toString(16)}${durationMs ? ` (${durationMs} ms)` : ""}`);
	} catch (error) {
		logAction(`Toets fout: ${error.message}`);
	}
}

async function toggleLatchedKey(keyMask) {
	try {
		if (state.latchedKeyMask === keyMask) {
			await releaseLatchedKey();
			logAction(`Latch losgelaten: 0x${keyMask.toString(16)}`);
			return;
		}
		if (state.latchedKeyMask !== 0) {
			await releaseLatchedKey();
		}
		await postKeyPress(keyMask);
		state.latchedKeyMask = keyMask;
		scheduleLatchedAutoRelease(keyMask);
		renderStatus();
		logAction(`Latch actief: 0x${keyMask.toString(16)}`);
	} catch (error) {
		logAction(`Latch fout: ${error.message}`);
	}
}

async function releaseLatchedKey() {
	clearLatchedAutoRelease();
	await postKeyPress(0);
	state.latchedKeyMask = 0;
	renderStatus();
}

function scheduleLatchedAutoRelease(keyMask) {
	clearLatchedAutoRelease();
	state.latchedKeyTimerId = window.setTimeout(async () => {
		if (state.latchedKeyMask !== keyMask) {
			return;
		}
		try {
			await releaseLatchedKey();
			logAction(`Latch timeout: 0x${keyMask.toString(16)}`);
		} catch (error) {
			logAction(`Latch timeout fout: ${error.message}`);
		}
	}, DEFAULT_LATCH_MS);
}

function clearLatchedAutoRelease() {
	if (state.latchedKeyTimerId !== null) {
		window.clearTimeout(state.latchedKeyTimerId);
		state.latchedKeyTimerId = null;
	}
}

function setActiveTab(tab, updateHash = true) {
	if (!menuItems.some((item) => item.id === tab)) {
		tab = "home";
	}
	state.activeTab = tab;
	if (tab === "logging") {
		void fetchKeyPressLog().then(() => renderStatus());
	}
	if (updateHash) {
		window.location.hash = tab;
	}
	renderPanels();
}

function setMenuOpen(open) {
	state.menuOpen = open;
	elements.sidebar.classList.toggle("open", open);
	elements.sidebarBackdrop.classList.toggle("open", open);
	elements.body.classList.toggle("menu-open", open);
}

function toggleTheme() {
	state.theme = state.theme === "dark" ? "light" : "dark";
	window.localStorage.setItem(THEME_KEY, state.theme);
	applyTheme();
}

function applyTheme() {
	document.documentElement.dataset.theme = state.theme;
	renderThemeButtons();
}

function logAction(message) {
	state.logs.push({
		time: formatClockTime(Date.now()),
		message
	});
	if (state.logs.length > 400) {
		state.logs = state.logs.slice(-400);
	}
	renderLogs();
}

async function loadParameterDefinitions() {
	try {
		state.parameterDefinitions = await fetchParameterDefinitionsData();
		renderStatus();
		renderSensors();
		logAction(`Parameterdefinities geladen (${state.parameterDefinitions.length})`);
	} catch (error) {
		logAction(`Parameterdefinities fout: ${error.message}`);
	}
}
