/**
 * Connect Four + Battleship multiplayer helpers for the WerkPager web sim.
 */

import {
  MessageType,
  makeC4Invite,
  makeC4Accept,
  makeC4Decline,
  makeC4Drop,
  makeC4Forfeit,
  makeBsInvite,
  makeBsAccept,
  makeBsDecline,
  makeBsReady,
  makeBsFire,
  makeBsResult,
  makeBsForfeit,
} from "../protocol/messages.js";

const C4_COLS = 7;
const C4_ROWS = 6;
const C4_COLORS = [
  { id: "pink", label: "Pink" },
  { id: "gold", label: "Gold" },
  { id: "mint", label: "Mint" },
  { id: "sky", label: "Sky" },
  { id: "violet", label: "Violet" },
  { id: "coral", label: "Coral" },
];

function normalizeC4Color(id) {
  return C4_COLORS.some((c) => c.id === id) ? id : "pink";
}

function otherDefaultC4Color(taken) {
  return C4_COLORS.find((c) => c.id !== taken)?.id || "gold";
}

const BS_SHIPS = [
  { id: "carrier", name: "Carrier", len: 5 },
  { id: "battleship", name: "Battleship", len: 4 },
  { id: "cruiser", name: "Cruiser", len: 3 },
  { id: "sub", name: "Submarine", len: 3 },
  { id: "destroyer", name: "Destroyer", len: 2 },
];

export function emptyC4Board() {
  return Array.from({ length: C4_ROWS }, () => Array(C4_COLS).fill(""));
}

export function c4Drop(board, col, mark) {
  for (let r = C4_ROWS - 1; r >= 0; r--) {
    if (!board[r][col]) {
      board[r][col] = mark;
      return r;
    }
  }
  return -1;
}

export function c4Winner(board) {
  const dirs = [
    [0, 1],
    [1, 0],
    [1, 1],
    [1, -1],
  ];
  for (let r = 0; r < C4_ROWS; r++) {
    for (let c = 0; c < C4_COLS; c++) {
      const cell = board[r][c];
      if (!cell) continue;
      for (const [dr, dc] of dirs) {
        let ok = true;
        for (let i = 1; i < 4; i++) {
          const rr = r + dr * i;
          const cc = c + dc * i;
          if (rr < 0 || rr >= C4_ROWS || cc < 0 || cc >= C4_COLS || board[rr][cc] !== cell) {
            ok = false;
            break;
          }
        }
        if (ok) return cell;
      }
    }
  }
  if (board.every((row) => row.every(Boolean))) return "draw";
  return null;
}

function emptySea() {
  return Array.from({ length: 10 }, () => Array(10).fill(null));
}

function canPlace(grid, x, y, len, horiz) {
  for (let i = 0; i < len; i++) {
    const xx = horiz ? x + i : x;
    const yy = horiz ? y : y + i;
    if (xx < 0 || yy < 0 || xx > 9 || yy > 9) return false;
    if (grid[yy][xx]) return false;
  }
  return true;
}

function placeShip(grid, shipId, x, y, len, horiz) {
  for (let i = 0; i < len; i++) {
    const xx = horiz ? x + i : x;
    const yy = horiz ? y : y + i;
    grid[yy][xx] = shipId;
  }
}

export function randomFleet() {
  const grid = emptySea();
  const placements = [];
  for (const ship of BS_SHIPS) {
    let placed = false;
    for (let attempt = 0; attempt < 80 && !placed; attempt++) {
      const horiz = Math.random() < 0.5;
      const x = Math.floor(Math.random() * 10);
      const y = Math.floor(Math.random() * 10);
      if (!canPlace(grid, x, y, ship.len, horiz)) continue;
      placeShip(grid, ship.id, x, y, ship.len, horiz);
      placements.push({ ...ship, x, y, horiz, hits: 0 });
      placed = true;
    }
    if (!placed) return randomFleet();
  }
  return { grid, ships: placements };
}

export function emptyFleet() {
  return { grid: emptySea(), ships: [] };
}

/** Apply bow / mid / stern / bridge classes for top-down ship look. */
export function styleShipCells(gridEl, ships) {
  for (const ship of ships) {
    const bridgeIdx = Math.floor((ship.len - 1) / 2);
    for (let i = 0; i < ship.len; i++) {
      const x = ship.horiz ? ship.x + i : ship.x;
      const y = ship.horiz ? ship.y : ship.y + i;
      const cell = gridEl.querySelector(`[data-x="${x}"][data-y="${y}"]`);
      if (!cell) continue;
      cell.classList.add("ship", ship.horiz ? "ship-h" : "ship-v");
      if (i === 0) cell.classList.add("ship-bow");
      else if (i === ship.len - 1) cell.classList.add("ship-stern");
      else cell.classList.add("ship-mid", "ship-deck");
      if (i === bridgeIdx) cell.classList.add("ship-bridge");
    }
  }
}

/**
 * @param {object} api
 */
export function installBoardGames(api) {
  const {
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
  } = api;

  /** @type {string} */
  let c4PickColor = "pink";
  /** @type {string} */
  let c4InvitePickColor = "gold";

  function activeKey() {
    return activeDeskKeyRef.current;
  }

  function ensureGameFields(d) {
    if (!("c4Invite" in d)) d.c4Invite = null;
    if (!("c4Game" in d)) d.c4Game = null;
    if (!("bsInvite" in d)) d.bsInvite = null;
    if (!("bsGame" in d)) d.bsGame = null;
  }

  Object.values(desks).forEach(ensureGameFields);

  function fillC4ColorPick(containerId, selectedId, takenId, onPick) {
    const box = document.getElementById(containerId);
    if (!box) return;
    box.innerHTML = "";
    for (const color of C4_COLORS) {
      const btn = document.createElement("button");
      btn.type = "button";
      btn.className = `c4-color-btn disc-${color.id}`;
      btn.textContent = color.label;
      btn.disabled = Boolean(takenId && color.id === takenId);
      btn.setAttribute("aria-pressed", color.id === selectedId ? "true" : "false");
      btn.addEventListener("click", () => {
        if (btn.disabled) return;
        onPick(color.id);
        fillC4ColorPick(containerId, color.id, takenId, onPick);
      });
      box.appendChild(btn);
    }
  }

  /* —— Connect Four UI —— */
  function showC4Panel(id) {
    for (const p of ["c4Pick", "c4Waiting", "c4Invite", "c4Play"]) {
      document.getElementById(p).hidden = p !== id;
    }
    const result = document.getElementById("c4Result");
    if (result && id !== "c4Play") result.hidden = true;
  }

  function setDock(elId, buttons) {
    const dock = document.getElementById(elId);
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

  function renderC4Screen() {
    const d = desk();
    ensureGameFields(d);

    if (d.c4Invite) {
      showC4Panel("c4Invite");
      document.getElementById("c4InviteName").textContent = d.c4Invite.fromName;
      const taken = d.c4Invite.color;
      document.getElementById("c4InviteSub").textContent = `wants to play · they chose ${taken}`;
      if (!c4InvitePickColor || c4InvitePickColor === taken) {
        c4InvitePickColor = otherDefaultC4Color(taken);
      }
      fillC4ColorPick("c4InviteColors", c4InvitePickColor, taken, (id) => {
        c4InvitePickColor = id;
      });
      setDock("c4Dock", [
        {
          label: "Accept",
          primary: true,
          onClick: () => {
            const inv = d.c4Invite;
            const myColor = c4InvitePickColor === inv.color
              ? otherDefaultC4Color(inv.color)
              : c4InvitePickColor;
            broadcast(activeKey(), makeC4Accept(d.id, d.name, inv.fromId, myColor));
            d.c4Invite = null;
            d.c4Game = {
              opponentId: inv.fromId,
              opponentName: inv.fromName,
              myColor,
              oppColor: inv.color,
              mark: myColor,
              board: emptyC4Board(),
              turn: inv.color,
              over: false,
              waiting: false,
              resultDismissed: false,
            };
            renderC4Screen();
            showScreen("connect4");
          },
        },
        {
          label: "Decline",
          danger: true,
          onClick: () => {
            broadcast(activeKey(), makeC4Decline(d.id, d.name, d.c4Invite.fromId));
            d.c4Invite = null;
            syncActiveDeskUi();
          },
        },
      ]);
      return;
    }

    if (d.c4Game?.waiting) {
      showC4Panel("c4Waiting");
      document.getElementById("c4WaitingName").textContent = d.c4Game.opponentName;
      setDock("c4Dock", [
        {
          label: "Cancel",
          danger: true,
          onClick: () => {
            broadcast(activeKey(), makeC4Forfeit(d.id, d.name, d.c4Game.opponentId));
            d.c4Game = null;
            syncActiveDeskUi();
          },
        },
      ]);
      return;
    }

    if (d.c4Game) {
      showC4Panel("c4Play");
      renderC4Board();
      const buttons = [];
      if (d.c4Game.over && d.c4Game.resultDismissed) {
        buttons.push({
          label: "Play again",
          primary: true,
          onClick: () => rematchC4(),
        });
        buttons.push({
          label: "Home",
          onClick: () => {
            d.c4Game = null;
            showScreen("gamesfolder");
          },
        });
      } else if (!d.c4Game.over) {
        buttons.push({
          label: "Forfeit",
          danger: true,
          onClick: () => {
            confirmForfeit(() => {
              broadcast(activeKey(), makeC4Forfeit(d.id, d.name, d.c4Game.opponentId));
              d.c4Game = null;
              syncActiveDeskUi();
            });
          },
        });
      } else {
        buttons.push({ label: "…", onClick: () => {} });
      }
      setDock("c4Dock", buttons);
      return;
    }

    showC4Panel("c4Pick");
    fillC4ColorPick("c4ColorPick", c4PickColor, null, (id) => {
      c4PickColor = id;
    });
    const list = document.getElementById("c4PeerList");
    list.innerHTML = "";
    if (d.peers.size === 0) {
      list.innerHTML = '<p class="tagline">Add a peer in Settings first.</p>';
    } else {
      for (const peer of d.peers.values()) {
        const btn = document.createElement("button");
        btn.type = "button";
        btn.className = "peer-btn";
        btn.innerHTML = `${escapeHtml(peer.name)}<small>challenge</small>`;
        btn.addEventListener("click", () => startC4(peer));
        list.appendChild(btn);
      }
    }
    setDock("c4Dock", [{ label: "Back", onClick: () => showScreen("gamesfolder") }]);
  }

  function startC4(peer) {
    const d = desk();
    const color = normalizeC4Color(c4PickColor);
    d.c4Game = {
      opponentId: peer.id,
      opponentName: peer.name,
      myColor: color,
      oppColor: null,
      mark: color,
      board: emptyC4Board(),
      turn: color,
      over: false,
      waiting: true,
      resultDismissed: false,
    };
    broadcast(activeKey(), makeC4Invite(d.id, d.name, peer.id, color));
    renderC4Screen();
    showScreen("connect4");
  }

  function rematchC4() {
    const d = desk();
    const peer = { id: d.c4Game.opponentId, name: d.c4Game.opponentName };
    d.c4Game = null;
    startC4(peer);
  }

  function renderC4Board() {
    const d = desk();
    const g = d.c4Game;
    const boardEl = document.getElementById("c4Board");
    const resultEl = document.getElementById("c4Result");
    boardEl.innerHTML = "";
    const myTurn = !g.over && !g.waiting && g.turn === g.mark;
    const drop = g.lastDrop;

    for (let c = 0; c < C4_COLS; c++) {
      const colBtn = document.createElement("button");
      colBtn.type = "button";
      colBtn.className = "c4-col";
      colBtn.disabled = !myTurn || g.board[0][c];
      colBtn.addEventListener("click", () => dropC4(c));
      for (let r = 0; r < C4_ROWS; r++) {
        const cell = document.createElement("div");
        cell.className = "c4-cell";
        const v = g.board[r][c];
        if (v) cell.classList.add(`disc-${v}`);
        if (drop && drop.r === r && drop.c === c && v) {
          cell.classList.add("c4-dropping");
          cell.style.setProperty("--drop-rows", String(r + 1));
          cell.addEventListener(
            "animationend",
            () => {
              cell.classList.remove("c4-dropping");
              cell.style.removeProperty("--drop-rows");
              if (g.lastDrop?.r === r && g.lastDrop?.c === c) g.lastDrop = null;
            },
            { once: true }
          );
        }
        colBtn.appendChild(cell);
      }
      boardEl.appendChild(colBtn);
    }

    const status = document.getElementById("c4Status");
    if (g.over) {
      const w = c4Winner(g.board);
      if (!g.resultDismissed) {
        resultEl.hidden = false;
        if (w === "draw") {
          resultEl.className = "ttt-result";
          document.getElementById("c4ResultEmoji").textContent = "🤝";
          document.getElementById("c4ResultText").textContent = "Draw";
        } else {
          const win = w === g.mark;
          resultEl.className = `ttt-result ${win ? "win" : "lose"}`;
          document.getElementById("c4ResultEmoji").textContent = win ? "🎉" : "😢";
          document.getElementById("c4ResultText").textContent = win
            ? "Condragulations!"
            : "Sashay away…";
        }
        resultEl.onclick = () => {
          g.resultDismissed = true;
          resultEl.hidden = true;
          renderC4Screen();
        };
      } else {
        resultEl.hidden = true;
      }
      status.textContent = "";
    } else {
      resultEl.hidden = true;
      status.textContent = myTurn ? "Your turn — tap a column" : `Waiting for ${g.opponentName}…`;
    }
  }

  function dropC4(col) {
    const d = desk();
    const g = d.c4Game;
    if (!g || g.over || g.waiting || g.turn !== g.mark) return;
    const row = c4Drop(g.board, col, g.mark);
    if (row < 0) return;
    g.lastDrop = { r: row, c: col };
    const w = c4Winner(g.board);
    if (w) {
      g.over = true;
      g.resultDismissed = false;
    } else g.turn = g.oppColor;
    broadcast(activeKey(), makeC4Drop(d.id, g.opponentId, col, g.mark));
    renderC4Screen();
  }

  /* —— Battleship —— */
  let bsModeFallback = "offense"; // only used before a game exists

  function getBsMode(g) {
    return g?.bsMode || bsModeFallback;
  }

  function setBsMode(g, mode) {
    if (g) g.bsMode = mode;
    else bsModeFallback = mode;
  }

  function nextShipIndex(ships) {
    const placed = new Set(ships.map((s) => s.id));
    const idx = BS_SHIPS.findIndex((s) => !placed.has(s.id));
    return idx < 0 ? BS_SHIPS.length : idx;
  }

  /** Valid placements of length `len` that use (ax,ay) as an endpoint (bow or stern). */
  function placementOptionsFromAnchor(grid, ax, ay, len) {
    if (grid[ay][ax]) return [];
    const candidates = [
      { x: ax, y: ay, horiz: true }, // extends right
      { x: ax - len + 1, y: ay, horiz: true }, // extends left
      { x: ax, y: ay, horiz: false }, // extends down
      { x: ax, y: ay - len + 1, horiz: false }, // extends up
    ];
    const seen = new Set();
    const out = [];
    for (const c of candidates) {
      if (!canPlace(grid, c.x, c.y, len, c.horiz)) continue;
      // must include anchor
      let covers = false;
      for (let i = 0; i < len; i++) {
        const xx = c.horiz ? c.x + i : c.x;
        const yy = c.horiz ? c.y : c.y + i;
        if (xx === ax && yy === ay) covers = true;
      }
      if (!covers) continue;
      const key = `${c.x},${c.y},${c.horiz ? "h" : "v"}`;
      if (seen.has(key)) continue;
      seen.add(key);
      out.push(c);
    }
    return out;
  }

  function cellsForPlacement(p, len) {
    const cells = [];
    for (let i = 0; i < len; i++) {
      cells.push({
        x: p.horiz ? p.x + i : p.x,
        y: p.horiz ? p.y : p.y + i,
      });
    }
    return cells;
  }

  function removeShipFromFleet(fleet, shipId) {
    const ship = fleet.ships.find((s) => s.id === shipId);
    if (!ship) return;
    for (let i = 0; i < ship.len; i++) {
      const x = ship.horiz ? ship.x + i : ship.x;
      const y = ship.horiz ? ship.y : ship.y + i;
      if (fleet.grid[y][x] === shipId) fleet.grid[y][x] = "";
    }
    fleet.ships = fleet.ships.filter((s) => s.id !== shipId);
  }

  function shipAtCell(fleet, x, y) {
    const id = fleet.grid[y][x];
    if (!id || id === "hit") return null;
    return fleet.ships.find((s) => s.id === id) || null;
  }

  function showBsPanel(id) {
    for (const p of ["bsPick", "bsWaiting", "bsInvite", "bsSetup", "bsPlay"]) {
      document.getElementById(p).hidden = p !== id;
    }
    const result = document.getElementById("bsResult");
    if (result && id !== "bsPlay") result.hidden = true;
  }

  function renderBsScreen() {
    const d = desk();
    ensureGameFields(d);

    if (d.bsInvite) {
      showBsPanel("bsInvite");
      document.getElementById("bsInviteName").textContent = d.bsInvite.fromName;
      setDock("bsDock", [
        {
          label: "Accept",
          primary: true,
          onClick: () => {
            const inv = d.bsInvite;
            broadcast(activeKey(), makeBsAccept(d.id, d.name, inv.fromId));
            d.bsInvite = null;
            const fleet = emptyFleet();
            d.bsGame = {
              opponentId: inv.fromId,
              opponentName: inv.fromName,
              phase: "setup",
              waiting: false,
              iAmFirst: false,
              myTurn: false,
              fleet,
              placingIndex: 0,
              placeAnchor: null,
              selectedShipId: null,
              tracking: emptySea(),
              oppReady: false,
              meReady: false,
              over: false,
              resultDismissed: false,
              lastMsg: "Place your fleet",
            };
            renderBsScreen();
            showScreen("battleship");
          },
        },
        {
          label: "Decline",
          danger: true,
          onClick: () => {
            broadcast(activeKey(), makeBsDecline(d.id, d.name, d.bsInvite.fromId));
            d.bsInvite = null;
            syncActiveDeskUi();
          },
        },
      ]);
      return;
    }

    if (d.bsGame?.waiting) {
      showBsPanel("bsWaiting");
      document.getElementById("bsWaitingName").textContent = d.bsGame.opponentName;
      setDock("bsDock", [
        {
          label: "Cancel",
          danger: true,
          onClick: () => {
            broadcast(activeKey(), makeBsForfeit(d.id, d.name, d.bsGame.opponentId));
            d.bsGame = null;
            syncActiveDeskUi();
          },
        },
      ]);
      return;
    }

    if (d.bsGame?.phase === "setup") {
      showBsPanel("bsSetup");
      renderBsSetup();
      const allPlaced = d.bsGame.fleet.ships.length >= BS_SHIPS.length;
      setDock("bsDock", [
        {
          label: "Forfeit",
          danger: true,
          onClick: () => {
            confirmForfeit(() => {
              broadcast(activeKey(), makeBsForfeit(d.id, d.name, d.bsGame.opponentId));
              d.bsGame = null;
              syncActiveDeskUi();
            });
          },
        },
        {
          label: "Randomize",
          onClick: () => {
            d.bsGame.fleet = randomFleet();
            d.bsGame.meReady = false;
            d.bsGame.placingIndex = BS_SHIPS.length;
            d.bsGame.placeAnchor = null;
            d.bsGame.selectedShipId = null;
            renderBsScreen();
          },
        },
        {
          label: d.bsGame.meReady ? "Waiting…" : "Ready",
          primary: true,
          onClick: () => {
            if (!allPlaced) {
              toast("Place all ships first");
              return;
            }
            if (d.bsGame.meReady) return;
            d.bsGame.meReady = true;
            d.bsGame.placeAnchor = null;
            d.bsGame.selectedShipId = null;
            broadcast(activeKey(), makeBsReady(d.id, d.name, d.bsGame.opponentId));
            maybeStartBsBattle(d);
            renderBsScreen();
          },
        },
      ]);
      return;
    }

    if (d.bsGame?.phase === "battle") {
      showBsPanel("bsPlay");
      renderBsBattle();
      const buttons = [];
      if (d.bsGame.over && d.bsGame.resultDismissed) {
        buttons.push({
          label: "Play again",
          primary: true,
          onClick: () => rematchBs(),
        });
        buttons.push({
          label: "Home",
          onClick: () => {
            d.bsGame = null;
            showScreen("gamesfolder");
          },
        });
      } else if (!d.bsGame.over) {
        buttons.push({
          label: "Forfeit",
          danger: true,
          onClick: () => {
            confirmForfeit(() => {
              broadcast(activeKey(), makeBsForfeit(d.id, d.name, d.bsGame.opponentId));
              d.bsGame = null;
              syncActiveDeskUi();
            });
          },
        });
      }
      setDock("bsDock", buttons);
      return;
    }

    showBsPanel("bsPick");
    const list = document.getElementById("bsPeerList");
    list.innerHTML = "";
    if (d.peers.size === 0) {
      list.innerHTML = '<p class="tagline">Add a peer in Settings first.</p>';
    } else {
      for (const peer of d.peers.values()) {
        const btn = document.createElement("button");
        btn.type = "button";
        btn.className = "peer-btn";
        btn.innerHTML = `${escapeHtml(peer.name)}<small>challenge</small>`;
        btn.addEventListener("click", () => startBs(peer));
        list.appendChild(btn);
      }
    }
    setDock("bsDock", [{ label: "Back", onClick: () => showScreen("gamesfolder") }]);
  }

  function startBs(peer) {
    const d = desk();
    d.bsGame = {
      opponentId: peer.id,
      opponentName: peer.name,
      phase: "setup",
      waiting: true,
      iAmFirst: true,
      myTurn: true,
      fleet: emptyFleet(),
      placingIndex: 0,
      placeAnchor: null,
      selectedShipId: null,
      tracking: emptySea(),
      oppReady: false,
      meReady: false,
      over: false,
      resultDismissed: false,
      lastMsg: "Waiting for accept…",
    };
    broadcast(activeKey(), makeBsInvite(d.id, d.name, peer.id));
    renderBsScreen();
    showScreen("battleship");
  }

  function rematchBs() {
    const d = desk();
    const peer = { id: d.bsGame.opponentId, name: d.bsGame.opponentName };
    d.bsGame = null;
    startBs(peer);
  }

  function maybeStartBsBattle(d) {
    if (d.bsGame.meReady && d.bsGame.oppReady) {
      d.bsGame.phase = "battle";
      d.bsGame.waiting = false;
      d.bsGame.myTurn = d.bsGame.iAmFirst;
      d.bsGame.lastMsg = d.bsGame.myTurn ? "Your turn — tap to fire!" : "Enemy turn…";
      setBsMode(d.bsGame, d.bsGame.myTurn ? "offense" : "defense");
    } else if (d.bsGame.meReady) {
      d.bsGame.lastMsg = "Waiting for opponent fleet…";
    }
  }

  function renderBsSetup() {
    const d = desk();
    const g = d.bsGame;
    g.placingIndex = nextShipIndex(g.fleet.ships);
    if (!("placeAnchor" in g)) g.placeAnchor = null;
    if (!("selectedShipId" in g)) g.selectedShipId = null;

    const ship = BS_SHIPS[Math.min(g.placingIndex, BS_SHIPS.length - 1)];
    const hint = document.getElementById("bsSetupHint");
    const allPlaced = g.fleet.ships.length >= BS_SHIPS.length;
    const removeBtn = document.getElementById("bsRemoveBtn");
    if (removeBtn) removeBtn.disabled = !g.selectedShipId || Boolean(g.meReady);

    if (g.meReady) {
      hint.textContent = "Ready — waiting on opponent…";
    } else if (allPlaced) {
      hint.textContent = "Fleet set — Ready, or tap a ship to remove";
    } else if (g.placeAnchor) {
      hint.textContent = `Tap a highlighted square to place ${ship.name} (${ship.len})`;
    } else {
      hint.textContent = `Tap a square to start ${ship.name} (${ship.len})`;
    }

    const options =
      !allPlaced && !g.meReady && g.placeAnchor
        ? placementOptionsFromAnchor(g.fleet.grid, g.placeAnchor.x, g.placeAnchor.y, ship.len)
        : [];

    /** @type {Map<string, object[]>} */
    const cellOptions = new Map();
    for (const opt of options) {
      for (const c of cellsForPlacement(opt, ship.len)) {
        const key = `${c.x},${c.y}`;
        if (!cellOptions.has(key)) cellOptions.set(key, []);
        cellOptions.get(key).push(opt);
      }
    }

    const grid = document.getElementById("bsSetupGrid");
    grid.innerHTML = "";
    for (let y = 0; y < 10; y++) {
      for (let x = 0; x < 10; x++) {
        const cell = document.createElement("button");
        cell.type = "button";
        cell.className = "bs-cell";
        cell.dataset.x = String(x);
        cell.dataset.y = String(y);
        const key = `${x},${y}`;
        const optsHere = cellOptions.get(key);
        if (g.placeAnchor?.x === x && g.placeAnchor?.y === y) cell.classList.add("anchor");
        if (optsHere?.length) cell.classList.add("ghost");

        cell.addEventListener("click", () => {
          if (g.meReady) return;

          const existing = shipAtCell(g.fleet, x, y);
          if (existing) {
            g.placeAnchor = null;
            g.selectedShipId = g.selectedShipId === existing.id ? null : existing.id;
            renderBsScreen();
            return;
          }

          g.selectedShipId = null;

          if (allPlaced) {
            toast("Tap a ship to remove, or Ready");
            return;
          }

          if (g.placeAnchor && optsHere?.length) {
            const isAnchor = g.placeAnchor.x === x && g.placeAnchor.y === y;
            if (isAnchor && optsHere.length > 1) {
              g.placeAnchor = null;
              renderBsScreen();
              return;
            }
            let chosen = optsHere[0];
            if (optsHere.length > 1) {
              chosen =
                optsHere.find((opt) => {
                  const cells = cellsForPlacement(opt, ship.len);
                  const tip = cells[cells.length - 1];
                  const start = cells[0];
                  return (tip.x === x && tip.y === y) || (start.x === x && start.y === y);
                }) || optsHere[0];
            }
            placeShip(g.fleet.grid, ship.id, chosen.x, chosen.y, ship.len, chosen.horiz);
            g.fleet.ships.push({
              ...ship,
              x: chosen.x,
              y: chosen.y,
              horiz: chosen.horiz,
              hits: 0,
            });
            g.placeAnchor = null;
            g.placingIndex = nextShipIndex(g.fleet.ships);
            renderBsScreen();
            return;
          }

          if (g.placeAnchor?.x === x && g.placeAnchor?.y === y) {
            g.placeAnchor = null;
          } else {
            g.placeAnchor = { x, y };
          }
          renderBsScreen();
        });
        grid.appendChild(cell);
      }
    }
    styleShipCells(grid, g.fleet.ships);
    if (g.selectedShipId) {
      const sel = g.fleet.ships.find((s) => s.id === g.selectedShipId);
      if (sel) {
        for (const c of cellsForPlacement(sel, sel.len)) {
          grid
            .querySelector(`[data-x="${c.x}"][data-y="${c.y}"]`)
            ?.classList.add("ship-selected");
        }
      }
    }
  }

  function renderBsBattle() {
    const d = desk();
    const g = d.bsGame;
    document.getElementById("bsStatus").textContent = g.lastMsg;
    const mode = getBsMode(g);
    document.querySelectorAll("[data-bs-mode]").forEach((btn) => {
      btn.setAttribute("aria-pressed", btn.dataset.bsMode === mode ? "true" : "false");
    });

    const grid = document.getElementById("bsBattleGrid");
    grid.innerHTML = "";
    const resultEl = document.getElementById("bsResult");

    if (mode === "offense") {
      for (let y = 0; y < 10; y++) {
        for (let x = 0; x < 10; x++) {
          const cell = document.createElement("button");
          cell.type = "button";
          cell.className = "bs-cell";
          cell.dataset.x = String(x);
          cell.dataset.y = String(y);
          const mark = g.tracking[y][x];
          if (mark === "hit") cell.classList.add("hit");
          if (mark === "miss") cell.classList.add("miss");
          cell.disabled = !g.myTurn || g.over || Boolean(mark);
          cell.addEventListener("click", () => fireBs(x, y));
          grid.appendChild(cell);
        }
      }
    } else {
      for (let y = 0; y < 10; y++) {
        for (let x = 0; x < 10; x++) {
          const cell = document.createElement("button");
          cell.type = "button";
          cell.className = "bs-cell";
          cell.dataset.x = String(x);
          cell.dataset.y = String(y);
          cell.disabled = true;
          if (g.fleet.grid[y][x] === "hit") cell.classList.add("hit");
          else if (g.fleetMiss?.[y]?.[x]) cell.classList.add("miss");
          grid.appendChild(cell);
        }
      }
      styleShipCells(
        grid,
        g.fleet.ships.filter((s) => {
          // still show ship body; hits overlay via hit class on cells
          return true;
        })
      );
      // Re-apply hits on top of ship styling
      for (let y = 0; y < 10; y++) {
        for (let x = 0; x < 10; x++) {
          if (g.fleet.grid[y][x] === "hit") {
            const cell = grid.querySelector(`[data-x="${x}"][data-y="${y}"]`);
            if (cell) cell.classList.add("hit");
          }
        }
      }
    }

    if (g.over && !g.resultDismissed) {
      resultEl.hidden = false;
      const win = g.iWon;
      resultEl.className = `ttt-result ${win ? "win" : "lose"}`;
      document.getElementById("bsResultEmoji").textContent = win ? "🎉" : "😢";
      document.getElementById("bsResultText").textContent = win
        ? "Condragulations!"
        : "Sashay away…";
      resultEl.onclick = () => {
        g.resultDismissed = true;
        resultEl.hidden = true;
        renderBsScreen();
      };
    } else {
      resultEl.hidden = true;
    }
  }

  function fireBs(x, y) {
    const d = desk();
    const g = d.bsGame;
    if (!g?.myTurn || g.over) return;
    broadcast(activeKey(), makeBsFire(d.id, g.opponentId, x, y));
    g.myTurn = false;
    g.lastMsg = "Shot away…";
    setBsMode(g, "defense");
    renderBsScreen();
  }

  function handleIncomingFire(targetKey, msg) {
    const d = desks[targetKey];
    const g = d.bsGame;
    if (!g || g.phase !== "battle") return;
    if (!g.fleetMiss) g.fleetMiss = emptySea();
    const shipId = g.fleet.grid[msg.y][msg.x];
    let hit = false;
    let sunk = false;
    let gameOver = false;
    if (shipId && shipId !== "hit") {
      hit = true;
      g.fleet.grid[msg.y][msg.x] = "hit";
      const ship = g.fleet.ships.find((s) => s.id === shipId);
      if (ship) {
        ship.hits++;
        if (ship.hits >= ship.len) sunk = true;
      }
      gameOver = g.fleet.ships.every((s) => s.hits >= s.len);
    } else {
      g.fleetMiss[msg.y][msg.x] = true;
    }
    broadcast(targetKey, makeBsResult(d.id, msg.fromId, msg.x, msg.y, hit, sunk, gameOver));
    if (gameOver) {
      g.over = true;
      g.iWon = false;
      g.lastMsg = "Fleet destroyed";
    } else {
      g.myTurn = true;
      g.lastMsg = hit ? "They hit you! Your turn." : "Missed you. Your turn.";
      setBsMode(g, "offense");
    }
    if (targetKey === activeKey()) {
      renderBsScreen();
      showScreen("battleship");
    }
  }

  function handleBsResult(targetKey, msg) {
    const d = desks[targetKey];
    const g = d.bsGame;
    if (!g) return;
    g.tracking[msg.y][msg.x] = msg.hit ? "hit" : "miss";
    if (msg.gameOver) {
      g.over = true;
      g.iWon = true;
      g.lastMsg = "You sank their fleet!";
    } else {
      g.lastMsg = msg.hit
        ? msg.sunk
          ? "Hit — sunk!"
          : "Hit!"
        : "Miss";
      g.myTurn = false;
      setBsMode(g, "defense");
    }
    if (targetKey === activeKey()) {
      renderBsScreen();
      showScreen("battleship");
    }
  }

  function onRadio(msg, fromDeskKey) {
    if (msg.type === MessageType.C4_INVITE) {
      const targetKey = deskKeyById(msg.toId);
      if (!targetKey || targetKey === fromDeskKey) return true;
      const t = desks[targetKey];
      ensureGameFields(t);
      if (t.c4Game || t.c4Invite) return true;
      t.c4Invite = {
        fromId: msg.fromId,
        fromName: msg.fromName,
        color: normalizeC4Color(msg.color),
      };
      if (targetKey === activeKey()) {
        renderC4Screen();
        showScreen("connect4");
        toast(`${msg.fromName} challenged you · Connect Four`);
      }
      return true;
    }
    if (msg.type === MessageType.C4_ACCEPT) {
      const key = deskKeyById(msg.toId);
      if (!key) return true;
      const d = desks[key];
      if (!d.c4Game?.waiting || d.c4Game.opponentId !== msg.fromId) return true;
      d.c4Game.waiting = false;
      d.c4Game.opponentName = msg.fromName;
      d.c4Game.oppColor = normalizeC4Color(msg.color);
      if (key === activeKey()) {
        renderC4Screen();
        showScreen("connect4");
      }
      return true;
    }
    if (msg.type === MessageType.C4_DECLINE) {
      const key = deskKeyById(msg.toId);
      if (!key) return true;
      const d = desks[key];
      if (d.c4Game?.waiting && d.c4Game.opponentId === msg.fromId) {
        d.c4Game = null;
        if (key === activeKey()) {
          toast(`${msg.fromName} declined`);
          syncActiveDeskUi();
        }
      }
      return true;
    }
    if (msg.type === MessageType.C4_DROP) {
      const key = deskKeyById(msg.toId);
      if (!key || key === fromDeskKey) return true;
      const d = desks[key];
      const g = d.c4Game;
      if (!g || g.opponentId !== msg.fromId) return true;
      const color = normalizeC4Color(msg.color || g.oppColor);
      const row = c4Drop(g.board, msg.col, color);
      if (row >= 0) g.lastDrop = { r: row, c: msg.col };
      const w = c4Winner(g.board);
      if (w) {
        g.over = true;
        g.resultDismissed = false;
      } else g.turn = g.mark;
      if (key === activeKey()) {
        renderC4Screen();
        showScreen("connect4");
      }
      return true;
    }
    if (msg.type === MessageType.C4_FORFEIT) {
      const key = deskKeyById(msg.toId);
      if (!key) return true;
      const d = desks[key];
      if (d.c4Invite?.fromId === msg.fromId) d.c4Invite = null;
      if (d.c4Game?.opponentId === msg.fromId) d.c4Game = null;
      if (key === activeKey()) {
        toast(`${msg.fromName} left Connect Four`);
        syncActiveDeskUi();
      }
      return true;
    }

    if (msg.type === MessageType.BS_INVITE) {
      const targetKey = deskKeyById(msg.toId);
      if (!targetKey || targetKey === fromDeskKey) return true;
      const t = desks[targetKey];
      ensureGameFields(t);
      if (t.bsGame || t.bsInvite) return true;
      t.bsInvite = { fromId: msg.fromId, fromName: msg.fromName };
      if (targetKey === activeKey()) {
        renderBsScreen();
        showScreen("battleship");
        toast(`${msg.fromName} challenged you · Battleship`);
      }
      return true;
    }
    if (msg.type === MessageType.BS_ACCEPT) {
      const key = deskKeyById(msg.toId);
      if (!key) return true;
      const d = desks[key];
      if (!d.bsGame?.waiting || d.bsGame.opponentId !== msg.fromId) return true;
      d.bsGame.waiting = false;
      d.bsGame.phase = "setup";
      d.bsGame.opponentName = msg.fromName;
      d.bsGame.lastMsg = "Place your fleet";
      d.bsGame.fleet = emptyFleet();
      d.bsGame.placingIndex = 0;
      d.bsGame.meReady = false;
      d.bsGame.placeAnchor = null;
      d.bsGame.selectedShipId = null;
      if (key === activeKey()) {
        renderBsScreen();
        showScreen("battleship");
      }
      return true;
    }
    if (msg.type === MessageType.BS_DECLINE) {
      const key = deskKeyById(msg.toId);
      if (!key) return true;
      const d = desks[key];
      if (d.bsGame?.waiting && d.bsGame.opponentId === msg.fromId) {
        d.bsGame = null;
        if (key === activeKey()) {
          toast(`${msg.fromName} declined`);
          syncActiveDeskUi();
        }
      }
      return true;
    }
    if (msg.type === MessageType.BS_READY) {
      const key = deskKeyById(msg.toId);
      if (!key) return true;
      const d = desks[key];
      if (!d.bsGame || d.bsGame.opponentId !== msg.fromId) return true;
      d.bsGame.oppReady = true;
      maybeStartBsBattle(d);
      if (key === activeKey()) {
        renderBsScreen();
        showScreen("battleship");
      }
      return true;
    }
    if (msg.type === MessageType.BS_FIRE) {
      const key = deskKeyById(msg.toId);
      if (!key || key === fromDeskKey) return true;
      handleIncomingFire(key, msg);
      return true;
    }
    if (msg.type === MessageType.BS_RESULT) {
      const key = deskKeyById(msg.toId);
      if (!key || key === fromDeskKey) return true;
      handleBsResult(key, msg);
      return true;
    }
    if (msg.type === MessageType.BS_FORFEIT) {
      const key = deskKeyById(msg.toId);
      if (!key) return true;
      const d = desks[key];
      if (d.bsInvite?.fromId === msg.fromId) d.bsInvite = null;
      if (d.bsGame?.opponentId === msg.fromId) d.bsGame = null;
      if (key === activeKey()) {
        toast(`${msg.fromName} left Battleship`);
        syncActiveDeskUi();
      }
      return true;
    }
    return false;
  }

  document.querySelectorAll("[data-bs-mode]").forEach((btn) => {
    btn.addEventListener("click", () => {
      const g = desk().bsGame;
      if (g?.phase === "battle") {
        setBsMode(g, btn.dataset.bsMode);
        renderBsBattle();
      }
    });
  });

  document.getElementById("bsRemoveBtn")?.addEventListener("click", () => {
    const d = desk();
    if (d.bsGame?.phase !== "setup" || !d.bsGame.selectedShipId || d.bsGame.meReady) return;
    removeShipFromFleet(d.bsGame.fleet, d.bsGame.selectedShipId);
    d.bsGame.selectedShipId = null;
    d.bsGame.placeAnchor = null;
    d.bsGame.placingIndex = nextShipIndex(d.bsGame.fleet.ships);
    d.bsGame.meReady = false;
    renderBsScreen();
  });

  document.getElementById("bsClearBtn")?.addEventListener("click", () => {
    const d = desk();
    if (d.bsGame?.phase !== "setup" || d.bsGame.meReady) return;
    d.bsGame.fleet = emptyFleet();
    d.bsGame.meReady = false;
    d.bsGame.placingIndex = 0;
    d.bsGame.placeAnchor = null;
    d.bsGame.selectedShipId = null;
    renderBsScreen();
  });

  return {
    renderC4Screen,
    renderBsScreen,
    onRadio,
    busy(d) {
      ensureGameFields(d);
      return Boolean(d.c4Invite || d.c4Game || d.bsInvite || d.bsGame);
    },
  };
}
