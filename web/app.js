/**
 * WerkPager web simulator — three virtual desks sharing an in-memory "radio".
 * Open via a local static server (ES modules need http:// not file://).
 */

import {
  MessageType,
  makeAck,
  makeCall,
  makeClear,
  makeDiscover,
  makeDiscoverReply,
  makeTttInvite,
  makeTttAccept,
  makeTttDecline,
  makeTttMove,
  makeTttForfeit,
  encode,
  decode,
} from "../protocol/messages.js";
import { installBoardGames } from "./board-games.js";
import { installMoreGames } from "./more-games.js";

const DEFAULT_EMOJIS = ["💅", "👑", "📢", "👀", "✨", "☕", "🆘", "🎉"];
const DEFAULT_CANNED = [
  "Got a sec?",
  "Come here",
  "Lunch?",
  "Urgent",
];
const MARK_GLYPH = { X: "✖", O: "○" };

const EMOJI_PALETTE = [
  "💅", "👑", "📢", "👀", "✨", "☕", "🆘", "🎉",
  "🔥", "💖", "🫡", "😈", "🫠", "🚀", "🎯", "😎",
  "🤡", "💀", "🌈", "⚡", "🏆", "🥹", "👋", "🪩",
  "😂", "🥰", "😍", "😜", "🤩", "😤", "😴", "🤔",
  "🙌", "👏", "💪", "🤝", "👀", "💬", "🔔", "⭐",
  "🍕", "🍩", "🍺", "🍾", "🎮", "🎲", "🧩", "🎵",
  "🐱", "🐶", "🦄", "🐉", "👽", "🤖", "👻", "🎃",
  "💯", "💢", "💥", "💫", "🌙", "☀️", "❄️", "🌊",
];

const THEMES = [
  { id: "eleganza", label: "Eleganza" },
  { id: "runway", label: "Runway" },
  { id: "ice", label: "Ice" },
  { id: "lemon", label: "Lemon" },
  { id: "matcha", label: "Matcha" },
];

const TIMEOUTS = [
  { id: "30", label: "30s", ms: 30_000 },
  { id: "60", label: "1m", ms: 60_000 },
  { id: "300", label: "5m", ms: 300_000 },
  { id: "off", label: "Off", ms: 0 },
];

const IDLE_MODES = [
  { id: "black", label: "Black" },
  { id: "clock", label: "Clock" },
];

/**
 * @typedef {{ fromId: string, fromName: string, emoji: string|null, message: string|null }} IncomingCall
 * @typedef {{ toId: string, toName: string, emoji: string|null, message: string|null }} OutgoingCall
 * @typedef {{ fromId: string, fromName: string }} TttInvite
 * @typedef {{
 *   opponentId: string,
 *   opponentName: string,
 *   mark: "X"|"O",
 *   board: string[],
 *   turn: "X"|"O",
 *   over: boolean,
 *   waiting: boolean,
 * }} TttGame
 * @typedef {{
 *   id: string,
 *   name: string,
 *   theme: string,
 *   timeoutId: string,
 *   idleMode: string,
 *   brightness: number,
 *   clockOffsetMs: number,
 *   emojis: string[],
 *   canned: string[],
 *   peers: Map<string, { id: string, name: string }>,
 *   incoming: IncomingCall|null,
 *   outgoing: OutgoingCall|null,
 *   tttInvite: TttInvite|null,
 *   tttGame: TttGame|null,
 * }} Desk
 */

/** @type {Record<string, Desk>} */
const desks = {
  tommy: makeDesk("mac-tommy", "Tommy"),
  will: makeDesk("mac-will", "Will"),
  alex: makeDesk("mac-alex", "Alex"),
};

function makeDesk(id, name) {
  const d = {
    id,
    name,
    theme: "eleganza",
    timeoutId: "60",
    idleMode: "clock",
    brightness: 100,
    clockOffsetMs: 0,
    wifiSsid: "",
    wifiConnected: false,
    emojis: [...DEFAULT_EMOJIS],
    canned: [...DEFAULT_CANNED],
    peers: new Map(),
    incoming: null,
    outgoing: null,
    tttInvite: null,
    tttGame: null,
  };
  hydrateDesk(d);
  return d;
}

function persistDesk(d = desk()) {
  try {
    localStorage.setItem(
      `werkpager:${d.id}`,
      JSON.stringify({
        name: d.name,
        theme: d.theme,
        timeoutId: d.timeoutId,
        idleMode: d.idleMode,
        brightness: d.brightness,
        clockOffsetMs: d.clockOffsetMs,
        wifiSsid: d.wifiSsid,
        wifiConnected: d.wifiConnected,
        emojis: d.emojis,
        canned: d.canned,
      })
    );
  } catch {
    /* ignore quota / private mode */
  }
}

function hydrateDesk(d) {
  try {
    const raw = localStorage.getItem(`werkpager:${d.id}`);
    if (!raw) return;
    const saved = JSON.parse(raw);
    if (typeof saved.name === "string" && saved.name) d.name = saved.name.slice(0, 12);
    if (typeof saved.theme === "string") d.theme = saved.theme;
    if (typeof saved.timeoutId === "string") d.timeoutId = saved.timeoutId;
    if (typeof saved.idleMode === "string") d.idleMode = saved.idleMode;
    if (typeof saved.brightness === "number") {
      d.brightness = Math.min(100, Math.max(10, Math.round(saved.brightness)));
    }
    if (typeof saved.clockOffsetMs === "number") d.clockOffsetMs = saved.clockOffsetMs;
    if (typeof saved.wifiSsid === "string") d.wifiSsid = saved.wifiSsid.slice(0, 32);
    if (typeof saved.wifiConnected === "boolean") d.wifiConnected = saved.wifiConnected;
    if (Array.isArray(saved.emojis) && saved.emojis.length === DEFAULT_EMOJIS.length) {
      d.emojis = saved.emojis;
    }
    if (Array.isArray(saved.canned) && saved.canned.length === DEFAULT_CANNED.length) {
      d.canned = saved.canned;
    }
  } catch {
    /* ignore */
  }
}

function applyBrightness() {
  const device = document.getElementById("device");
  const page =
    currentScreen === "incoming" ||
    currentScreen === "outgoing" ||
    device.classList.contains("is-ringing");
  const pct = page ? 100 : desk().brightness ?? 100;
  device.style.setProperty("--wp-brightness", String(pct / 100));
  device.classList.toggle("is-page-bright", page);
  device.classList.toggle("apply-dim", !page && pct < 100);
}

let activeDeskKey = "tommy";
const activeDeskKeyRef = {
  get current() {
    return activeDeskKey;
  },
};
/** @type {{ id: string, name: string } | null} */
let composePeer = null;
let composeEmoji = null;
let composeMessage = "";
let oskBuffer = "";
/** @type {"name"|"canned"|"compose"} */
let oskMode = "name";
let oskCannedIndex = 0;
let oskCaps = false;
let emojiEditIndex = 0;
let idleTimer = null;
let clockTimer = null;
/** @type {string} */
let currentScreen = "hub";
/** @type {string} */
let screenBeforeIdle = "hub";

const bus = new EventTarget();
/** @type {ReturnType<typeof installBoardGames>} */
let boardGames;
/** @type {ReturnType<typeof installMoreGames>} */
let moreGames;

function desk() {
  return desks[activeDeskKey];
}

function deskKeyById(id) {
  return Object.keys(desks).find((key) => desks[key].id === id) ?? null;
}

function toast(text) {
  const el = document.getElementById("toast");
  el.textContent = text;
  el.classList.add("show");
  clearTimeout(toast._t);
  toast._t = setTimeout(() => el.classList.remove("show"), 2200);
}

/** @type {null | (() => void)} */
let confirmYesHandler = null;

function hideConfirm() {
  confirmYesHandler = null;
  document.getElementById("confirmOverlay").hidden = true;
}

/** Confirm before forfeiting a game. */
function confirmForfeit(onYes) {
  confirmYesHandler = onYes;
  document.getElementById("confirmText").textContent =
    "Are you sure you want to forfeit?";
  document.getElementById("confirmOverlay").hidden = false;
}

function updateBrandSub(screen = currentScreen) {
  const sub = document.getElementById("brandSub");
  const meta = document.getElementById("brandMeta");
  if (!sub) return;
  const d = desk();
  let opponent = "";
  let metaHtml = "";

  if (screen === "tictactoe") {
    if (d.tttInvite) opponent = d.tttInvite.fromName;
    else if (d.tttGame) opponent = d.tttGame.opponentName;
  } else if (screen === "connect4") {
    if (d.c4Invite) opponent = d.c4Invite.fromName;
    else if (d.c4Game) opponent = d.c4Game.opponentName;
  } else if (screen === "battleship") {
    if (d.bsInvite) opponent = d.bsInvite.fromName;
    else if (d.bsGame) opponent = d.bsGame.opponentName;
  } else if (screen === "checkers") {
    if (d.ckInvite) opponent = d.ckInvite.fromName;
    else if (d.ckGame) {
      opponent = d.ckGame.opponentName;
      if (!d.ckGame.waiting) {
        const side = d.ckGame.side === "r" ? "Red" : "Black";
        const turn = d.ckGame.over
          ? "Game over"
          : d.ckGame.turn === d.ckGame.side
            ? "Your turn"
            : "Their turn";
        metaHtml = `${escapeHtml(turn)} <span class="ck-side">· you are <strong>${escapeHtml(side)}</strong></span>`;
      }
    }
  } else if (screen === "memory") {
    if (d.memInvite) opponent = d.memInvite.fromName;
    else if (d.memGame) {
      opponent = d.memGame.opponentName;
      if (!d.memGame.waiting) {
        const g = d.memGame;
        const turn = g.over
          ? "Game over"
          : g.lock
            ? "…"
            : g.myTurn
              ? "Your turn"
              : "Their turn";
        metaHtml = `${escapeHtml(turn)} <span class="ck-side">· you <strong>${g.myScore}</strong> · them <strong>${g.oppScore}</strong></span>`;
      }
    }
  } else if (screen === "doodle" && d.doodlePeerName) {
    opponent = d.doodlePeerName;
  }

  if (opponent) {
    sub.hidden = false;
    sub.innerHTML = `vs <strong>${escapeHtml(opponent)}</strong>`;
  } else {
    sub.hidden = true;
    sub.textContent = "";
  }

  if (meta) {
    if (metaHtml) {
      meta.hidden = false;
      meta.innerHTML = metaHtml;
    } else {
      meta.hidden = true;
      meta.textContent = "";
    }
  }
}

function applyTheme(themeId) {
  document.getElementById("device").dataset.theme = themeId;
}

function showScreen(name) {
  currentScreen = name;
  document.querySelectorAll(".screen").forEach((s) => {
    s.classList.toggle("active", s.dataset.screen === name);
  });
  const device = document.getElementById("device");
  device.classList.toggle("is-ringing", name === "incoming");
  device.classList.toggle("is-idle", name === "idle");
  applyBrightness();

  const brand = document.getElementById("brandTitle");
  const topbar = document.getElementById("topbar");
  if (name === "hub") {
    brand.textContent = "HOME";
    topbar.classList.add("topbar-minimal");
    updateHubClock();
  } else {
    topbar.classList.remove("topbar-minimal");
    if (name === "compose") brand.textContent = "WERK ROOM";
    else if (name === "tictactoe") brand.textContent = "TIC TAC TOE";
    else if (name === "connect4") brand.textContent = "CONNECT FOUR";
    else if (name === "battleship") brand.textContent = "BATTLESHIP";
    else if (name === "checkers") brand.textContent = "CHECKERS";
    else if (name === "memory") brand.textContent = "MEMORY";
    else if (name === "doodle") brand.textContent = "DOODLE";
    else if (name === "gamesfolder") brand.textContent = "GAMES";
    else if (name === "keyboard" && oskMode === "compose") brand.textContent = "WERK ROOM";
    else if (
      name === "settings" ||
      name === "keyboard" ||
      name === "emoji-picker" ||
      name === "wifi-scan" ||
      name === "ota-releases"
    ) {
      brand.textContent = "SETTINGS";
    } else brand.textContent = "WERKPAGER";
  }
  updateBrandSub(name);

  if (name !== "idle" && name !== "incoming" && name !== "outgoing") {
    resetIdleTimer();
  }
}

function nowForDesk(d = desk()) {
  return new Date(Date.now() + d.clockOffsetMs);
}

function formatTime(date) {
  return date.toLocaleTimeString([], { hour: "numeric", minute: "2-digit" });
}

function updateIdleClock() {
  const d = desk();
  const date = nowForDesk(d);
  document.getElementById("idleTime").textContent = formatTime(date);
  document.getElementById("idleDate").textContent = date.toLocaleDateString([], {
    weekday: "short",
    month: "short",
    day: "numeric",
  });
}

function updateHubClock() {
  const el = document.getElementById("hubClock");
  if (el) el.textContent = formatTime(nowForDesk());
}

function goIdle() {
  const d = desk();
  if (d.incoming || d.outgoing || d.tttInvite || d.tttGame || boardGames.busy(d) || moreGames?.busy(d)) return;
  if (currentScreen === "keyboard" || currentScreen === "emoji-picker") return;
  if (currentScreen === "idle") return;
  screenBeforeIdle = currentScreen;
  const idle = document.querySelector('[data-screen="idle"]');
  idle.classList.toggle("mode-clock", d.idleMode === "clock");
  updateIdleClock();
  showScreen("idle");
}

function wakeFromIdle() {
  if (currentScreen !== "idle") return;
  resumeScreen(screenBeforeIdle || "hub");
}

function resumeScreen(name) {
  if (name === "werk") {
    renderWerkPeers();
    showScreen("werk");
  } else if (name === "compose" && composePeer) {
    renderCompose();
    showScreen("compose");
  } else if (name === "compose") {
    renderWerkPeers();
    showScreen("werk");
  } else if (name === "settings") {
    renderSettings();
    showScreen("settings");
  } else if (name === "tictactoe") {
    renderTttScreen();
    showScreen("tictactoe");
  } else if (name === "connect4") {
    boardGames.renderC4Screen();
    showScreen("connect4");
  } else if (name === "battleship") {
    boardGames.renderBsScreen();
    showScreen("battleship");
  } else if (name === "checkers") {
    moreGames.renderCkScreen();
    showScreen("checkers");
  } else if (name === "memory") {
    moreGames.renderMemScreen();
    showScreen("memory");
  } else if (name === "doodle") {
    moreGames.renderDoodleScreen();
    showScreen("doodle");
  } else if (name === "gamesfolder") {
    showScreen("gamesfolder");
  } else if (name === "outgoing" || name === "incoming") {
    syncActiveDeskUi();
  } else {
    showScreen("hub");
  }
  resetIdleTimer();
}

function resetIdleTimer() {
  clearTimeout(idleTimer);
  const d = desk();
  const spec = TIMEOUTS.find((t) => t.id === d.timeoutId) ?? TIMEOUTS[1];
  if (!spec.ms) return;
  if (
    currentScreen === "idle" ||
    currentScreen === "incoming" ||
    currentScreen === "outgoing" ||
    d.tttInvite ||
    d.tttGame ||
    boardGames.busy(d) ||
    moreGames?.busy(d)
  ) {
    return;
  }
  idleTimer = setTimeout(goIdle, spec.ms);
}

/** Sync the visible device to the active desk's state. */
function syncActiveDeskUi() {
  const d = desk();
  applyTheme(d.theme);
  document.getElementById("meName").textContent = d.name;

  if (d.incoming) {
    document.getElementById("callFrom").textContent = d.incoming.fromName;
    document.getElementById("callEmoji").textContent = d.incoming.emoji || "📢";
    document.getElementById("callMessage").textContent =
      d.incoming.message || "is calling your desk";
    showScreen("incoming");
    return;
  }

  if (d.outgoing) {
    document.getElementById("outgoingTo").textContent = d.outgoing.toName;
    const bits = [d.outgoing.emoji, d.outgoing.message].filter(Boolean).join(" · ");
    document.getElementById("outgoingDetail").textContent = bits || "Waiting for them to notice…";
    showScreen("outgoing");
    return;
  }

  if (d.tttInvite || d.tttGame) {
    renderTttScreen();
    showScreen("tictactoe");
    return;
  }

  if (boardGames.busy(d)) {
    if (d.c4Invite || d.c4Game) {
      boardGames.renderC4Screen();
      showScreen("connect4");
    } else {
      boardGames.renderBsScreen();
      showScreen("battleship");
    }
    return;
  }

  if (moreGames?.busy(d)) {
    if (d.ckInvite || d.ckGame) {
      moreGames.renderCkScreen();
      showScreen("checkers");
    } else {
      moreGames.renderMemScreen();
      showScreen("memory");
    }
    return;
  }

  showScreen("hub");
}

function renderWerkPeers() {
  const d = desk();
  const list = document.getElementById("peerList");
  list.innerHTML = "";

  if (d.peers.size === 0) {
    list.innerHTML =
      '<p class="tagline">No peers saved yet. Scan or add from Settings.</p>';
    return;
  }

  for (const peer of d.peers.values()) {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "peer-btn";
    btn.innerHTML = `${escapeHtml(peer.name)}<small>compose a ping</small>`;
    btn.addEventListener("click", () => openCompose(peer));
    list.appendChild(btn);
  }
}

function openCompose(peer) {
  composePeer = peer;
  composeEmoji = null;
  composeMessage = "";
  document.getElementById("composeTo").textContent = peer.name;
  renderCompose();
  showScreen("compose");
}

function openComposeKeyboard() {
  oskMode = "compose";
  oskCaps = false;
  oskBuffer = composeMessage || "";
  document.getElementById("oskTitle").textContent = "Custom message";
  document.getElementById("oskValue").textContent = oskBuffer;
  buildOsk();
  showScreen("keyboard");
}

function renderCompose() {
  const d = desk();
  const emojiRow = document.getElementById("composeEmojis");
  emojiRow.innerHTML = "";
  for (const emoji of d.emojis) {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.textContent = emoji;
    btn.setAttribute("aria-pressed", composeEmoji === emoji ? "true" : "false");
    btn.addEventListener("click", () => {
      composeEmoji = composeEmoji === emoji ? null : emoji;
      renderCompose();
    });
    emojiRow.appendChild(btn);
  }

  const canned = document.getElementById("composeCanned");
  canned.innerHTML = "";

  const actions = document.createElement("div");
  actions.className = "compose-msg-actions";
  const clearBtn = document.createElement("button");
  clearBtn.type = "button";
  clearBtn.className = "compose-msg-action";
  clearBtn.innerHTML = `<span class="compose-msg-icon" aria-hidden="true">⌫</span> Clear Message`;
  clearBtn.addEventListener("click", () => {
    composeMessage = "";
    renderCompose();
  });
  const customBtn = document.createElement("button");
  customBtn.type = "button";
  customBtn.className = "compose-msg-action";
  customBtn.innerHTML = `<span class="compose-msg-icon" aria-hidden="true">✎</span> Custom message`;
  customBtn.addEventListener("click", openComposeKeyboard);
  actions.append(clearBtn, customBtn);
  canned.appendChild(actions);

  for (const text of d.canned) {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.textContent = text;
    btn.setAttribute("aria-pressed", composeMessage === text ? "true" : "false");
    btn.addEventListener("click", () => {
      composeMessage = text;
      renderCompose();
    });
    canned.appendChild(btn);
  }

  const preview = [composeEmoji, composeMessage].filter(Boolean).join("  ");
  document.getElementById("composePreview").textContent = preview || "No message";
}

function renderSettings() {
  const d = desk();
  document.getElementById("btnEditName").textContent = d.name;
  renderThemes();
  const bright = document.getElementById("brightnessSlider");
  const brightLbl = document.getElementById("brightnessValue");
  if (bright) {
    bright.value = String(d.brightness ?? 100);
    if (brightLbl) brightLbl.textContent = `${bright.value}%`;
  }

  const wifiStatus = document.getElementById("wifiStatus");
  const wifiScan = document.getElementById("btnWifiScan");
  const wifiDisc = document.getElementById("btnWifiDisconnect");
  if (wifiStatus) {
    if (d.wifiConnected && d.wifiSsid) {
      wifiStatus.textContent = `Connected - ${d.wifiSsid}`;
      wifiStatus.classList.add("is-on");
    } else {
      wifiStatus.textContent = "Not connected";
      wifiStatus.classList.remove("is-on");
    }
  }
  if (wifiScan) wifiScan.textContent = d.wifiConnected ? "Change" : "Scan Wi‑Fi";
  if (wifiDisc) wifiDisc.hidden = !d.wifiConnected;

  renderOptionRow("timeoutRow", TIMEOUTS, d.timeoutId, (id) => {
    d.timeoutId = id;
    persistDesk(d);
    resetIdleTimer();
    renderSettings();
  });
  renderOptionRow("idleModeRow", IDLE_MODES, d.idleMode, (id) => {
    d.idleMode = id;
    persistDesk(d);
    renderSettings();
  });

  const clockInput = document.getElementById("clockInput");
  const dateInput = document.getElementById("dateInput");
  const n = nowForDesk(d);
  clockInput.value = `${String(n.getHours()).padStart(2, "0")}:${String(n.getMinutes()).padStart(2, "0")}`;
  dateInput.value = `${n.getFullYear()}-${String(n.getMonth() + 1).padStart(2, "0")}-${String(n.getDate()).padStart(2, "0")}`;

  const emojiBox = document.getElementById("settingsEmojis");
  emojiBox.innerHTML = "";
  d.emojis.forEach((emoji, index) => {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.textContent = emoji;
    btn.addEventListener("click", () => openEmojiPicker(index));
    emojiBox.appendChild(btn);
  });

  const cannedBox = document.getElementById("settingsCanned");
  cannedBox.innerHTML = "";
  d.canned.forEach((text, index) => {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.textContent = text;
    btn.addEventListener("click", () => openCannedKeyboard(index));
    cannedBox.appendChild(btn);
  });

  const box = document.getElementById("nearbyList");
  box.innerHTML = "";
  for (const [key, other] of Object.entries(desks)) {
    if (key === activeDeskKey) continue;
    const saved = d.peers.has(other.id);
    const row = document.createElement("div");
    row.className = "nearby-row";
    row.innerHTML = `<span>${escapeHtml(other.name)}</span>`;
    const action = document.createElement("button");
    action.type = "button";
    action.className = saved ? "btn danger" : "btn primary";
    action.textContent = saved ? "Remove" : "Add";
    action.addEventListener("click", () => {
      if (saved) d.peers.delete(other.id);
      else d.peers.set(other.id, { id: other.id, name: other.name });
      toast(saved ? `Removed ${other.name}` : `Added ${other.name}`);
      renderSettings();
    });
    row.appendChild(action);
    box.appendChild(row);
  }
}

function renderOptionRow(elementId, options, selectedId, onPick) {
  const row = document.getElementById(elementId);
  row.innerHTML = "";
  for (const opt of options) {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "option-chip";
    btn.textContent = opt.label;
    btn.setAttribute("aria-pressed", opt.id === selectedId ? "true" : "false");
    btn.addEventListener("click", () => onPick(opt.id));
    row.appendChild(btn);
  }
}

function renderThemes() {
  const d = desk();
  const row = document.getElementById("themeRow");
  row.innerHTML = "";
  for (const theme of THEMES) {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "theme-swatch";
    btn.dataset.themeId = theme.id;
    btn.textContent = theme.label;
    btn.setAttribute("aria-pressed", theme.id === d.theme ? "true" : "false");
    btn.addEventListener("click", () => {
      d.theme = theme.id;
      persistDesk(d);
      applyTheme(theme.id);
      renderThemes();
    });
    row.appendChild(btn);
  }
}

function openKeyboard() {
  oskMode = "name";
  oskCaps = true;
  oskBuffer = desk().name;
  document.getElementById("oskTitle").textContent = "Enter name";
  document.getElementById("oskValue").textContent = oskBuffer;
  buildOsk();
  showScreen("keyboard");
}

function openCannedKeyboard(index) {
  oskMode = "canned";
  oskCaps = false;
  oskCannedIndex = index;
  oskBuffer = desk().canned[index] || "";
  document.getElementById("oskTitle").textContent = `Canned #${index + 1}`;
  document.getElementById("oskValue").textContent = oskBuffer;
  buildOsk();
  showScreen("keyboard");
}

function openEmojiPicker(index) {
  emojiEditIndex = index;
  const palette = document.getElementById("emojiPalette");
  palette.innerHTML = "";
  for (const emoji of EMOJI_PALETTE) {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.textContent = emoji;
    btn.addEventListener("click", () => {
      desk().emojis[emojiEditIndex] = emoji;
      persistDesk();
      renderSettings();
      showScreen("settings");
      toast("Emoji updated");
    });
    palette.appendChild(btn);
  }
  showScreen("emoji-picker");
}

function buildOsk() {
  const root = document.getElementById("osk");
  root.innerHTML = "";
  const messageMode = oskMode === "canned" || oskMode === "compose";

  const addRow = (keys, rowClass = "") => {
    const row = document.createElement("div");
    row.className = `osk-row${rowClass ? ` ${rowClass}` : ""}`;
    for (const spec of keys) {
      if (typeof spec === "string") {
        const letter = oskCaps ? spec : spec.toLowerCase();
        row.appendChild(oskKey(letter, () => oskType(letter)));
      } else {
        row.appendChild(oskKey(spec.label, spec.onClick, spec.className || ""));
      }
    }
    root.appendChild(row);
  };

  if (messageMode) {
    addRow(
      ["!", "?", "&", "#", "@", "/", "+", "-", "=", "'"].map((sym) => ({
        label: sym,
        onClick: () => oskType(sym),
      })),
      "osk-row-syms"
    );
  }

  addRow([... "QWERTYUIOP"]);
  addRow([... "ASDFGHJKL"], "osk-row-mid");
  addRow([... "ZXCVBNM"], "osk-row-bottom-letters");

  addRow(
    [
      {
        label: oskCaps ? "CAPS" : "caps",
        className: oskCaps ? "osk-caps osk-caps-on" : "osk-caps",
        onClick: () => {
          oskCaps = !oskCaps;
          buildOsk();
        },
      },
      {
        label: "space",
        className: "osk-space",
        onClick: () => oskType(" "),
      },
      {
        label: "⌫",
        className: "osk-back",
        onClick: oskBackspace,
      },
    ],
    "osk-row-actions"
  );
}

function oskKey(label, onClick, extraClass = "") {
  const btn = document.createElement("button");
  btn.type = "button";
  btn.textContent = label;
  if (extraClass) btn.className = extraClass;
  btn.addEventListener("click", onClick);
  return btn;
}

function oskType(ch) {
  const max = oskMode === "name" ? 12 : 22;
  if (oskBuffer.length >= max) return;
  oskBuffer += ch;
  document.getElementById("oskValue").textContent = oskBuffer;
}

function oskBackspace() {
  oskBuffer = oskBuffer.slice(0, -1);
  document.getElementById("oskValue").textContent = oskBuffer;
}

function escapeHtml(s) {
  return String(s)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;");
}

function broadcast(fromDeskKey, msg) {
  bus.dispatchEvent(
    new CustomEvent("radio", {
      detail: { fromDeskKey, msg: decode(encode(msg)) },
    })
  );
}

function sendComposedCall() {
  const d = desk();
  if (!composePeer || d.outgoing || d.incoming) return;
  const msg = makeCall(d.id, d.name, composePeer.id, {
    emoji: composeEmoji,
    message: composeMessage,
  });
  d.outgoing = {
    toId: composePeer.id,
    toName: composePeer.name,
    emoji: composeEmoji,
    message: composeMessage,
  };
  broadcast(activeDeskKey, msg);
  composePeer = null;
  syncActiveDeskUi();
}

function deliverIncoming(targetDeskKey, msg) {
  const target = desks[targetDeskKey];
  target.incoming = {
    fromId: msg.fromId,
    fromName: msg.fromName,
    emoji: msg.emoji ?? null,
    message: msg.message ?? null,
  };
  if (targetDeskKey === activeDeskKey) syncActiveDeskUi();
}

/* —— Tic Tac Toe (multiplayer over radio) —— */

function emptyBoard() {
  return Array(9).fill("");
}

function tttGetWinner(board) {
  const lines = [
    [0, 1, 2],
    [3, 4, 5],
    [6, 7, 8],
    [0, 3, 6],
    [1, 4, 7],
    [2, 5, 8],
    [0, 4, 8],
    [2, 4, 6],
  ];
  for (const [a, b, c] of lines) {
    if (board[a] && board[a] === board[b] && board[a] === board[c]) return board[a];
  }
  return null;
}

function showTttPanel(id) {
  for (const panel of ["tttPick", "tttWaiting", "tttInvite", "tttPlay"]) {
    document.getElementById(panel).hidden = panel !== id;
  }
  const result = document.getElementById("tttResult");
  if (result && id !== "tttPlay") result.hidden = true;
}

function setTttDock(buttons) {
  const dock = document.getElementById("tttDock");
  dock.innerHTML = "";
  for (const spec of buttons) {
    const btn = document.createElement("button");
    btn.type = "button";
    if (spec.primary) btn.className = "primary";
    if (spec.danger) btn.className = "danger";
    btn.textContent = spec.label;
    btn.addEventListener("click", spec.onClick);
    dock.appendChild(btn);
  }
}

function renderTttScreen() {
  const d = desk();

  if (d.tttInvite) {
    showTttPanel("tttInvite");
    document.getElementById("tttInviteName").textContent = d.tttInvite.fromName;
    setTttDock([
      {
        label: "Accept",
        primary: true,
        onClick: () => {
          const inv = d.tttInvite;
          broadcast(activeDeskKey, makeTttAccept(d.id, d.name, inv.fromId));
          d.tttInvite = null;
          d.tttGame = {
            opponentId: inv.fromId,
            opponentName: inv.fromName,
            mark: "O",
            board: emptyBoard(),
            turn: "X",
            over: false,
            waiting: false,
            resultDismissed: false,
          };
          renderTttScreen();
          showScreen("tictactoe");
        },
      },
      {
        label: "Decline",
        danger: true,
        onClick: () => {
          broadcast(
            activeDeskKey,
            makeTttDecline(d.id, d.name, d.tttInvite.fromId)
          );
          d.tttInvite = null;
          syncActiveDeskUi();
        },
      },
    ]);
    return;
  }

  if (d.tttGame?.waiting) {
    showTttPanel("tttWaiting");
    document.getElementById("tttWaitingName").textContent = d.tttGame.opponentName;
    setTttDock([
      {
        label: "Cancel",
        danger: true,
        onClick: () => {
          broadcast(
            activeDeskKey,
            makeTttForfeit(d.id, d.name, d.tttGame.opponentId)
          );
          d.tttGame = null;
          syncActiveDeskUi();
        },
      },
    ]);
    return;
  }

  if (d.tttGame) {
    showTttPanel("tttPlay");
    renderTttBoard();
    if (d.tttGame.over && d.tttGame.resultDismissed) {
      setTttDock([
        {
          label: "Play again",
          primary: true,
          onClick: () => {
            const peer = {
              id: d.tttGame.opponentId,
              name: d.tttGame.opponentName,
            };
            d.tttGame = null;
            startTttChallenge(peer);
          },
        },
        {
          label: "Home",
          onClick: () => {
            d.tttGame = null;
            showScreen("gamesfolder");
          },
        },
      ]);
    } else if (!d.tttGame.over) {
      setTttDock([
        {
          label: "Forfeit",
          danger: true,
          onClick: () => {
            confirmForfeit(() => {
              broadcast(
                activeDeskKey,
                makeTttForfeit(d.id, d.name, d.tttGame.opponentId)
              );
              d.tttGame = null;
              syncActiveDeskUi();
            });
          },
        },
      ]);
    } else {
      setTttDock([]);
    }
    return;
  }

  // Pick a peer to challenge
  showTttPanel("tttPick");
  const list = document.getElementById("tttPeerList");
  list.innerHTML = "";
  if (d.peers.size === 0) {
    list.innerHTML = '<p class="tagline">Add a peer in Settings first.</p>';
  } else {
    for (const peer of d.peers.values()) {
      const btn = document.createElement("button");
      btn.type = "button";
      btn.className = "peer-btn";
      btn.innerHTML = `${escapeHtml(peer.name)}<small>challenge</small>`;
      btn.addEventListener("click", () => startTttChallenge(peer));
      list.appendChild(btn);
    }
  }
  setTttDock([{ label: "Back", onClick: () => showScreen("gamesfolder") }]);
}

function startTttChallenge(peer) {
  const d = desk();
  if (d.tttGame || d.tttInvite) return;
  d.tttGame = {
    opponentId: peer.id,
    opponentName: peer.name,
    mark: "X",
    board: emptyBoard(),
    turn: "X",
    over: false,
    waiting: true,
    resultDismissed: false,
  };
  broadcast(activeDeskKey, makeTttInvite(d.id, d.name, peer.id));
  renderTttScreen();
  showScreen("tictactoe");
}

function renderTttBoard() {
  const d = desk();
  const game = d.tttGame;
  if (!game) return;
  const boardEl = document.getElementById("tttBoard");
  const resultEl = document.getElementById("tttResult");
  boardEl.innerHTML = "";
  const myTurn = !game.over && !game.waiting && game.turn === game.mark;
  const winner = tttGetWinner(game.board);

  game.board.forEach((cell, i) => {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "ttt-cell";
    if (cell === "X") btn.classList.add("mark-x");
    if (cell === "O") btn.classList.add("mark-o");
    btn.textContent = cell ? MARK_GLYPH[cell] : "";
    btn.disabled = Boolean(cell) || !myTurn;
    btn.addEventListener("click", () => tttPlayCell(i));
    boardEl.appendChild(btn);
  });

  const markEl = document.getElementById("tttMyMark");
  markEl.textContent = MARK_GLYPH[game.mark];
  markEl.className = `ttt-mark-badge mark-${game.mark.toLowerCase()}`;

  if (game.over) {
    document.getElementById("tttStatus").textContent = game.resultDismissed
      ? "Play again?"
      : "Game over";
    if (!game.resultDismissed) {
      resultEl.hidden = false;
      if (winner) {
        const iWon = winner === game.mark;
        resultEl.className = `ttt-result ${iWon ? "win" : "lose"}`;
        document.getElementById("tttResultEmoji").textContent = iWon ? "🎉" : "😢";
        document.getElementById("tttResultText").textContent = iWon
          ? "Condragulations!"
          : "Sashay away…";
      } else {
        resultEl.className = "ttt-result";
        document.getElementById("tttResultEmoji").textContent = "🤝";
        document.getElementById("tttResultText").textContent = "It's a draw";
      }
      resultEl.onclick = () => {
        game.resultDismissed = true;
        resultEl.hidden = true;
        renderTttScreen();
      };
    } else {
      resultEl.hidden = true;
    }
  } else {
    resultEl.hidden = true;
    if (myTurn) {
      document.getElementById("tttStatus").textContent = "Your turn — serve!";
    } else {
      document.getElementById("tttStatus").textContent = `Waiting for ${game.opponentName}…`;
    }
  }
}

function tttPlayCell(i) {
  const d = desk();
  const game = d.tttGame;
  if (!game || game.over || game.waiting || game.turn !== game.mark || game.board[i]) return;

  game.board[i] = game.mark;
  const winner = tttGetWinner(game.board);
  if (winner || game.board.every(Boolean)) {
    game.over = true;
    game.resultDismissed = false;
  } else game.turn = game.mark === "X" ? "O" : "X";

  broadcast(activeDeskKey, makeTttMove(d.id, game.opponentId, i, game.mark));
  renderTttScreen();
}

function applyTttMove(targetKey, msg) {
  const d = desks[targetKey];
  const game = d.tttGame;
  if (!game || game.opponentId !== msg.fromId) return;
  if (game.board[msg.cell]) return;

  game.board[msg.cell] = msg.mark;
  const winner = tttGetWinner(game.board);
  if (winner || game.board.every(Boolean)) {
    game.over = true;
    game.resultDismissed = false;
  } else game.turn = msg.mark === "X" ? "O" : "X";

  if (targetKey === activeDeskKey) {
    renderTttScreen();
    showScreen("tictactoe");
  }
}

bus.addEventListener("radio", (ev) => {
  const { fromDeskKey, msg } = ev.detail;

  if (boardGames?.onRadio(msg, fromDeskKey)) return;
  if (moreGames?.onRadio(msg, fromDeskKey)) return;

  if (msg.type === MessageType.DISCOVER) {
    for (const [key, other] of Object.entries(desks)) {
      if (key === fromDeskKey) continue;
      broadcast(key, makeDiscoverReply(other.id, other.name));
    }
    return;
  }

  if (msg.type === MessageType.CALL) {
    const targetKey = deskKeyById(msg.toId);
    if (!targetKey || targetKey === fromDeskKey) return;
    deliverIncoming(targetKey, msg);
    return;
  }

  if (msg.type === MessageType.ACK) {
    const callerKey = deskKeyById(msg.forCallFromId);
    if (!callerKey) return;
    const caller = desks[callerKey];
    if (caller.outgoing) {
      caller.outgoing = null;
      if (callerKey === activeDeskKey) {
        syncActiveDeskUi();
        toast(`${msg.fromName} shantayed`);
      }
    }
    return;
  }

  if (msg.type === MessageType.CLEAR) {
    for (const [key, d] of Object.entries(desks)) {
      if (d.incoming && d.incoming.fromId === msg.fromId) {
        d.incoming = null;
        if (key === activeDeskKey) {
          syncActiveDeskUi();
          toast(`${msg.fromName} cancelled`);
        }
      }
    }
    return;
  }

  if (msg.type === MessageType.TTT_INVITE) {
    const targetKey = deskKeyById(msg.toId);
    if (!targetKey || targetKey === fromDeskKey) return;
    const target = desks[targetKey];
    if (target.tttGame || target.tttInvite) return; // busy
    target.tttInvite = { fromId: msg.fromId, fromName: msg.fromName };
    if (targetKey === activeDeskKey) {
      renderTttScreen();
      showScreen("tictactoe");
      toast(`${msg.fromName} challenged you`);
    }
    return;
  }

  if (msg.type === MessageType.TTT_ACCEPT) {
    const challengerKey = deskKeyById(msg.toId);
    if (!challengerKey) return;
    const challenger = desks[challengerKey];
    if (!challenger.tttGame?.waiting || challenger.tttGame.opponentId !== msg.fromId) return;
    challenger.tttGame.waiting = false;
    challenger.tttGame.opponentName = msg.fromName;
    if (challengerKey === activeDeskKey) {
      renderTttScreen();
      showScreen("tictactoe");
      toast(`${msg.fromName} accepted`);
    }
    return;
  }

  if (msg.type === MessageType.TTT_DECLINE) {
    const challengerKey = deskKeyById(msg.toId);
    if (!challengerKey) return;
    const challenger = desks[challengerKey];
    if (!challenger.tttGame?.waiting || challenger.tttGame.opponentId !== msg.fromId) return;
    challenger.tttGame = null;
    if (challengerKey === activeDeskKey) {
      toast(`${msg.fromName} declined`);
      syncActiveDeskUi();
    }
    return;
  }

  if (msg.type === MessageType.TTT_MOVE) {
    const targetKey = deskKeyById(msg.toId);
    if (!targetKey || targetKey === fromDeskKey) return;
    applyTttMove(targetKey, msg);
    return;
  }

  if (msg.type === MessageType.TTT_FORFEIT) {
    const targetKey = deskKeyById(msg.toId);
    if (!targetKey) return;
    const target = desks[targetKey];
    if (target.tttInvite && target.tttInvite.fromId === msg.fromId) {
      target.tttInvite = null;
    }
    if (target.tttGame && target.tttGame.opponentId === msg.fromId) {
      target.tttGame = null;
    }
    // Also clear waiting challenger if they cancelled before accept
    if (target.tttGame?.waiting && target.tttGame.opponentId === msg.fromId) {
      target.tttGame = null;
    }
    if (targetKey === activeDeskKey) {
      toast(`${msg.fromName} left the game`);
      syncActiveDeskUi();
    }
  }
});

document.querySelectorAll("[data-go]").forEach((btn) => {
  btn.addEventListener("click", () => {
    const screen = btn.getAttribute("data-go");
    if (screen === "werk") {
      renderWerkPeers();
      showScreen("werk");
    } else if (screen === "settings") {
      renderSettings();
      showScreen("settings");
    } else if (screen === "tictactoe") {
      renderTttScreen();
      showScreen("tictactoe");
    } else if (screen === "connect4") {
      boardGames.renderC4Screen();
      showScreen("connect4");
    } else if (screen === "battleship") {
      boardGames.renderBsScreen();
      showScreen("battleship");
    } else if (screen === "checkers") {
      moreGames.renderCkScreen();
      showScreen("checkers");
    } else if (screen === "memory") {
      moreGames.renderMemScreen();
      showScreen("memory");
    } else if (screen === "doodle") {
      moreGames.renderDoodleScreen();
      showScreen("doodle");
    } else if (screen === "gamesfolder" || screen === "hub") {
      showScreen(screen);
    }
  });
});

document.getElementById("btnComposeBack").addEventListener("click", () => {
  composePeer = null;
  renderWerkPeers();
  showScreen("werk");
});

document.getElementById("btnComposeSend").addEventListener("click", sendComposedCall);

document.getElementById("btnEditName").addEventListener("click", openKeyboard);

document.getElementById("btnOskCancel").addEventListener("click", () => {
  if (oskMode === "compose") {
    renderCompose();
    showScreen("compose");
    return;
  }
  renderSettings();
  showScreen("settings");
});

document.getElementById("btnOskDone").addEventListener("click", () => {
  if (oskMode === "compose") {
    composeMessage = oskBuffer.slice(0, 22);
    renderCompose();
    showScreen("compose");
    return;
  }
  if (oskMode === "canned") {
    const text = oskBuffer.trim() || `Message ${oskCannedIndex + 1}`;
    desk().canned[oskCannedIndex] = text.slice(0, 22);
    toast("Message saved");
  } else {
    const name = oskBuffer.trim() || "Queen";
    desk().name = name.slice(0, 12);
    document.getElementById("meName").textContent = desk().name;
    toast("Name saved");
  }
  persistDesk();
  renderSettings();
  showScreen("settings");
});

document.getElementById("btnEmojiCancel").addEventListener("click", () => {
  renderSettings();
  showScreen("settings");
});

document.getElementById("btnSetClock").addEventListener("click", () => {
  const timeValue = document.getElementById("clockInput").value;
  const dateValue = document.getElementById("dateInput").value;
  if (!timeValue || !dateValue) {
    toast("Set both date and time");
    return;
  }
  const [hh, mm] = timeValue.split(":").map(Number);
  const [yyyy, mo, dd] = dateValue.split("-").map(Number);
  const target = new Date(yyyy, mo - 1, dd, hh, mm, 0, 0);
  desk().clockOffsetMs = target.getTime() - Date.now();
  persistDesk();
  toast("Date & time set");
  updateHubClock();
  if (currentScreen === "idle") updateIdleClock();
  renderSettings();
});

document.getElementById("btnDiscover").addEventListener("click", () => {
  broadcast(activeDeskKey, makeDiscover(desk().id, desk().name));
  toast("Scanning…");
  renderSettings();
  showScreen("settings");
});

document.getElementById("btnAck").addEventListener("click", () => {
  const d = desk();
  if (!d.incoming) return;
  broadcast(activeDeskKey, makeAck(d.id, d.name, d.incoming.fromId));
  d.incoming = null;
  syncActiveDeskUi();
  toast("Shantay — they know");
});

document.getElementById("btnClear").addEventListener("click", () => {
  desk().incoming = null;
  syncActiveDeskUi();
});

document.getElementById("btnCancelPing").addEventListener("click", () => {
  const d = desk();
  if (!d.outgoing) return;
  broadcast(activeDeskKey, makeClear(d.id, d.name));
  d.outgoing = null;
  syncActiveDeskUi();
  toast("Ping cancelled");
});

document.querySelectorAll("[data-desk]").forEach((btn) => {
  btn.addEventListener("click", () => {
    activeDeskKey = btn.getAttribute("data-desk");
    composePeer = null;
    syncActiveDeskUi();
    toast(`Now controlling ${desk().name}'s desk`);
  });
});

document.getElementById("btnSimIncoming").addEventListener("click", () => {
  deliverIncoming(activeDeskKey, {
    fromId: "mac-sim",
    fromName: "Ru",
    emoji: "👑",
    message: "Lipsync for your life",
    type: MessageType.CALL,
    toId: desk().id,
  });
});

document.getElementById("btnForceIdle").addEventListener("click", goIdle);

document.getElementById("device").addEventListener("pointerdown", () => {
  if (currentScreen === "idle") wakeFromIdle();
  else resetIdleTimer();
});

desks.tommy.peers.set(desks.will.id, { id: desks.will.id, name: desks.will.name });
desks.will.peers.set(desks.tommy.id, { id: desks.tommy.id, name: desks.tommy.name });

boardGames = installBoardGames({
  desks,
  desk,
  activeDeskKeyRef,
  broadcast,
  toast,
  showScreen,
  syncActiveDeskUi,
  escapeHtml,
  deskKeyById,
  confirmForfeit,
});

moreGames = installMoreGames({
  desks,
  desk,
  activeDeskKeyRef,
  broadcast,
  toast,
  showScreen,
  syncActiveDeskUi,
  escapeHtml,
  deskKeyById,
  updateBrandSub,
  confirmForfeit,
});

document.getElementById("confirmNo").addEventListener("click", hideConfirm);
document.getElementById("confirmYes").addEventListener("click", () => {
  const fn = confirmYesHandler;
  hideConfirm();
  if (fn) fn();
});

clockTimer = setInterval(() => {
  updateHubClock();
  if (currentScreen === "idle" && desk().idleMode === "clock") updateIdleClock();
}, 1000);

const MOCK_WIFI_APS = [
  { ssid: "WerkOffice", open: false },
  { ssid: "Studio-5G", open: false },
  { ssid: "Guest", open: true },
  { ssid: "ATT-WiFi-8821", open: false },
  { ssid: "xfinitywifi", open: true },
  { ssid: "PIXELATE", open: false },
];

function openWifiScan() {
  const list = document.getElementById("wifiScanList");
  list.innerHTML = "";
  for (const ap of MOCK_WIFI_APS) {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "wifi-ap";
    btn.innerHTML = `<span>${escapeHtml(ap.ssid)}</span><small>${ap.open ? "Open" : "Secured"}</small>`;
    btn.addEventListener("click", () => {
      if (ap.open) {
        desk().wifiSsid = ap.ssid;
        desk().wifiConnected = true;
        persistDesk();
        toast(`Connected to ${ap.ssid}`);
        showScreen("settings");
        renderSettings();
        return;
      }
      const pass = window.prompt(`Password for ${ap.ssid}`, "");
      if (pass == null) return;
      desk().wifiSsid = ap.ssid;
      desk().wifiConnected = true;
      persistDesk();
      toast(`Connected to ${ap.ssid}`);
      showScreen("settings");
      renderSettings();
    });
    list.appendChild(btn);
  }
  showScreen("wifi-scan");
}

document.getElementById("btnWifiScan")?.addEventListener("click", openWifiScan);

document.getElementById("btnWifiDisconnect")?.addEventListener("click", () => {
  desk().wifiConnected = false;
  desk().wifiSsid = "";
  persistDesk();
  toast("Wi‑Fi disconnected");
  renderSettings();
});

document.getElementById("btnWifiScanBack")?.addEventListener("click", () => {
  renderSettings();
  showScreen("settings");
});

document.getElementById("btnWifiRescan")?.addEventListener("click", () => {
  toast("Scanning…");
  openWifiScan();
});

document.getElementById("btnWifiSyncTime")?.addEventListener("click", () => {
  if (!desk().wifiConnected) {
    toast("Connect to Wi‑Fi first");
    return;
  }
  desk().clockOffsetMs = 0;
  persistDesk();
  updateHubClock();
  toast("Time synced");
});

const FIRMWARE_VERSION = "0.1.0-sim";
const MOCK_OTA_RELEASES = [
  { tag: "v0.2.0", note: "latest" },
  { tag: "v0.1.0-sim", note: "this build" },
  { tag: "v0.1.0", note: "stable" },
  { tag: "v0.0.9", note: "older" },
];

function openOtaReleases() {
  document.getElementById("otaCurrentVersion").textContent = `Running ${FIRMWARE_VERSION}`;
  const list = document.getElementById("otaReleaseList");
  list.innerHTML = "";
  for (const r of MOCK_OTA_RELEASES) {
    const body = r.tag.startsWith("v") ? r.tag.slice(1) : r.tag;
    const isCurrent = body === FIRMWARE_VERSION;
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "wifi-ap";
    if (isCurrent) btn.classList.add("is-current");
    btn.innerHTML = `<span><strong>${escapeHtml(r.tag)}</strong><br><small>${
      isCurrent ? "current" : escapeHtml(r.note)
    }</small></span><small>${isCurrent ? "OK" : "Install"}</small>`;
    btn.addEventListener("click", () => {
      if (isCurrent) {
        toast("Already on this version");
        return;
      }
      toast(`Would install ${r.tag}`);
      showScreen("settings");
      renderSettings();
    });
    list.appendChild(btn);
  }
  showScreen("ota-releases");
}

document.getElementById("btnWifiUpdates")?.addEventListener("click", () => {
  if (!desk().wifiConnected) {
    toast("Connect to Wi‑Fi first");
    return;
  }
  openOtaReleases();
});

document.getElementById("btnOtaBack")?.addEventListener("click", () => {
  renderSettings();
  showScreen("settings");
});

document.getElementById("btnOtaRefresh")?.addEventListener("click", () => {
  toast("Fetching releases…");
  openOtaReleases();
});

document.getElementById("brightnessSlider")?.addEventListener("input", (e) => {
  const v = Number(e.target.value);
  desk().brightness = Math.min(100, Math.max(10, v));
  const lbl = document.getElementById("brightnessValue");
  if (lbl) lbl.textContent = `${desk().brightness}%`;
  persistDesk();
  applyBrightness();
});

function runBootSplash() {
  const splash = document.getElementById("bootSplash");
  if (!splash) {
    syncActiveDeskUi();
    resetIdleTimer();
    return;
  }
  document.getElementById("bootSplashName").textContent = desk().name;
  splash.hidden = false;
  const finish = () => {
    if (splash.hidden) return;
    splash.hidden = true;
    syncActiveDeskUi();
    resetIdleTimer();
  };
  splash.addEventListener("click", finish, { once: true });
  setTimeout(finish, 1400);
}

runBootSplash();
