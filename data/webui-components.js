import { escapeHtml } from "./webui-utils.js";

export const icons = {
	menu: () => `
		<svg viewBox="0 0 24 24" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linecap="round">
			<path d="M4 7h16M4 12h16M4 17h16"></path>
		</svg>`,
	x: () => `
		<svg viewBox="0 0 24 24" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linecap="round">
			<path d="M6 6l12 12M18 6L6 18"></path>
		</svg>`,
	sun: () => `
		<svg viewBox="0 0 24 24" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round">
			<circle cx="12" cy="12" r="4"></circle>
			<path d="M12 2.5v2.2M12 19.3v2.2M4.7 4.7l1.6 1.6M17.7 17.7l1.6 1.6M2.5 12h2.2M19.3 12h2.2M4.7 19.3l1.6-1.6M17.7 6.3l1.6-1.6"></path>
		</svg>`,
	moon: () => `
		<svg viewBox="0 0 24 24" aria-hidden="true" fill="currentColor">
			<path d="M14.8 2.4a8.8 8.8 0 1 0 6.8 14.1A9.6 9.6 0 0 1 14.8 2.4z"></path>
		</svg>`,
	home: () => `
		<svg viewBox="0 0 24 24" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
			<path d="M4 10.5L12 4l8 6.5"></path>
			<path d="M6.5 9.8V20h11V9.8"></path>
		</svg>`,
	debug: () => `
		<svg viewBox="0 0 24 24" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
			<path d="M9 3h6"></path>
			<path d="M10 7h4"></path>
			<rect x="6" y="7" width="12" height="12" rx="4"></rect>
			<path d="M9.5 12h.01M14.5 12h.01M9 16c.9-.8 1.9-1.2 3-1.2s2.1.4 3 1.2"></path>
		</svg>`,
	log: () => `
		<svg viewBox="0 0 24 24" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round">
			<path d="M7 4h8l4 4v12H7z"></path>
			<path d="M15 4v4h4"></path>
			<path d="M10 12h6M10 16h6"></path>
		</svg>`,
	mqtt: () => `
		<svg viewBox="0 0 24 24" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
			<path d="M4 8h16"></path>
			<path d="M7 8V5h10v3"></path>
			<rect x="4" y="8" width="16" height="10" rx="3"></rect>
			<path d="M8 18v2M16 18v2M9 12h.01M12 12h.01M15 12h.01"></path>
		</svg>`,
	wifi: () => `
		<svg viewBox="0 0 24 24" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round">
			<path d="M3.5 9.5A14.6 14.6 0 0 1 12 6.7a14.6 14.6 0 0 1 8.5 2.8"></path>
			<path d="M6.8 13.2A9.1 9.1 0 0 1 12 11.6a9.1 9.1 0 0 1 5.2 1.6"></path>
			<path d="M10 16.7a3.8 3.8 0 0 1 4 0"></path>
			<circle cx="12" cy="20" r="1"></circle>
		</svg>`,
	clock: () => `
		<svg viewBox="0 0 24 24" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
			<circle cx="12" cy="12" r="8"></circle>
			<path d="M12 7v5l3 2"></path>
		</svg>`,
	refresh: () => `
		<svg viewBox="0 0 24 24" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
			<path d="M20 5v5h-5"></path>
			<path d="M4 19v-5h5"></path>
			<path d="M19 10a7 7 0 0 0-12-2.3L4 10"></path>
			<path d="M5 14a7 7 0 0 0 12 2.3L20 14"></path>
		</svg>`,
	dot: () => `
		<svg viewBox="0 0 24 24" aria-hidden="true" fill="currentColor">
			<circle cx="12" cy="12" r="5"></circle>
		</svg>`
};

export function createAppShell() {
	return `
		<div class="app-shell">
			<div class="mobile-topbar">
				<div class="mobile-topbar-row">
					<button id="menu-open-button" class="icon-button" type="button" aria-label="Open menu">${icons.menu()}</button>
					<strong>Renovent</strong>
					<button id="mobile-theme-toggle" class="icon-button" type="button" aria-label="Wissel thema"></button>
				</div>
			</div>
			<div id="sidebar-backdrop" class="sidebar-backdrop"></div>
			<div class="app-layout">
				<aside id="sidebar" class="sidebar">
					<div class="sidebar-panel">
						<div class="sidebar-header">
							<div class="sidebar-brand">
								<strong>Renovent</strong>
								<span>WebUI</span>
							</div>
							<button id="menu-close-button" class="icon-button" type="button" aria-label="Sluit menu">${icons.x()}</button>
						</div>
						<nav id="menu-nav" class="sidebar-nav"></nav>
						<div class="sidebar-footer">
							<button id="theme-toggle" class="theme-toggle" type="button"></button>
							<div class="sidebar-status">
								<strong>Display</strong>
								<span id="sidebar-display">--</span>
							</div>
						</div>
					</div>
				</aside>
				<main class="content-shell">
					<div class="content-inner">
						<div class="content-top">
							<div class="header-meta">
								<div class="header-meta-item" title="Uptime">
									${icons.clock()}
									<span class="header-meta-copy"><span id="meta-uptime">--</span></span>
								</div>
								<div class="header-meta-item" title="Netwerk">
									${icons.wifi()}
									<span class="header-meta-copy"><span id="meta-ip">--</span></span>
								</div>
								<div class="header-meta-item header-meta-item-status" title="Status">
									${icons.refresh()}
									<span class="header-meta-copy"><span id="meta-updated">--</span></span>
								</div>
							</div>
						</div>
						<section id="panel-home" class="tab-panel active"></section>
						<section id="panel-mqtt" class="tab-panel"></section>
						<section id="panel-debug" class="tab-panel"></section>
						<section id="panel-logging" class="tab-panel"></section>
					</div>
				</main>
			</div>
		</div>`;
}

export function createMenu(items, activeTab) {
	return items.map((item) => `
		<button class="menu-link${item.id === activeTab ? " active" : ""}" data-tab="${item.id}" type="button">
			${item.icon()}
			<span>${escapeHtml(item.label)}</span>
		</button>`).join("");
}

export function metricCard(title, value, subtext = "", className = "") {
	return `
		<article class="metric-card ${escapeHtml(className)}">
			<strong>${escapeHtml(title)}</strong>
			<div class="metric-value">${escapeHtml(value)}</div>
			<div class="metric-subtext">${escapeHtml(subtext)}</div>
		</article>`;
}

export function keyButton(label, mask, durationMs = "") {
	const durationAttr = durationMs !== "" ? ` data-duration-ms="${escapeHtml(durationMs)}"` : "";
	return `<button class="key-button" type="button" data-key-mask="${escapeHtml(mask)}"${durationAttr}>${escapeHtml(label)}</button>`;
}

export function settingCard(item) {
	const label = item.label ?? item.name ?? `Index ${item.index}`;
	const key = item.key ?? "";
	const description = item.description ?? "";
	const display = item.display ?? item.valueDisplay ?? "--";
	const meta = [];
	if (Array.isArray(item.meta)) {
		meta.push(...item.meta.filter(Boolean));
	} else if (item.meta) {
		meta.push(item.meta);
	} else {
		if (item.unit) {
			meta.push(item.unit);
		}
		if (item.index !== undefined) {
			meta.push(`Index ${item.index}`);
		}
	}
	const metaMarkup = meta.length
		? `<div class="setting-card-meta">${meta.map((entry) => `<span>${escapeHtml(entry)}</span>`).join("")}</div>`
		: "";
	return `
		<div class="setting-card">
			<div class="setting-card-head">
				${key ? `<span class="setting-card-key">${escapeHtml(key)}</span>` : ""}
			</div>
			<div class="setting-card-title">${escapeHtml(label)}</div>
			<div class="value">${escapeHtml(display)}</div>
			${description ? `<div class="description">${escapeHtml(description)}</div>` : ""}
			${metaMarkup}
		</div>`;
}

export function scanTable(items) {
	if (!items.length) {
		return `<div class="note">Nog geen sensorgegevens beschikbaar.</div>`;
	}

	const rows = items.map((item) => `
		<tr>
			<td>${escapeHtml(item.index ?? "")}</td>
			<td>${escapeHtml(item.label ?? item.name ?? "")}</td>
			<td>${escapeHtml(item.valueDisplay ?? item.display ?? item.valueRaw ?? item.raw ?? "")}</td>
			<td>${escapeHtml(item.unit ?? "")}</td>
		</tr>`).join("");

	return `
		<table class="scan-table">
			<thead>
				<tr>
					<th>#</th>
					<th>Naam</th>
					<th>Waarde</th>
					<th>Unit</th>
				</tr>
			</thead>
			<tbody>${rows}</tbody>
		</table>`;
}
