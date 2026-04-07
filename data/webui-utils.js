export function escapeHtml(value) {
	return String(value ?? "")
		.replace(/&/g, "&amp;")
		.replace(/</g, "&lt;")
		.replace(/>/g, "&gt;")
		.replace(/\"/g, "&quot;")
		.replace(/'/g, "&#39;");
}

export function extractMenuItems(payload) {
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

export function normalizeMenuItem(item, index = 0) {
	return {
		index: item.index ?? item.id ?? index,
		label: item.label ?? item.name ?? item.title ?? `Item ${index}`,
		valueRaw: item.valueRaw ?? item.raw ?? item.value ?? "--",
		valueDisplay: item.valueDisplay ?? item.display ?? item.formatted ?? item.value ?? item.raw ?? "--",
		unit: item.unit ?? ""
	};
}

export function formatReading(value, unit) {
	if (value === undefined || value === null || value === "") {
		return { value: "--", subtext: unit };
	}
	return { value: String(value), subtext: unit };
}

export function formatUptime(value) {
	const totalMs = Number(value);
	if (!Number.isFinite(totalMs) || totalMs <= 0) {
		return "--";
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

export function formatClockTime(value) {
	const date = new Date(value);
	return date.toLocaleTimeString("nl-NL", {
		hour: "2-digit",
		minute: "2-digit",
		second: "2-digit"
	});
}

export function getInitialTab(menuItems) {
	const tab = window.location.hash.replace("#", "");
	return menuItems.some((item) => item.id === tab) ? tab : "home";
}

export function getInitialTheme(themeKey) {
	const saved = window.localStorage.getItem(themeKey);
	if (saved === "light" || saved === "dark") {
		return saved;
	}
	return window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light";
}