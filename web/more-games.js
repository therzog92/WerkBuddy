/**
 * Checkers, Memory (Drag Race icons), and Doodle stroke sync for WerkPager.
 * Payloads stay ESP-NOW friendly: 4-byte checker moves, seed + flip indices, stroke chunks.
 */

import {
  MessageType,
  makeCkInvite,
  makeCkAccept,
  makeCkDecline,
  makeCkMove,
  makeCkForfeit,
  makeMemInvite,
  makeMemAccept,
  makeMemDecline,
  makeMemFlip,
  makeMemForfeit,
  makeDoodleStroke,
  makeDoodleClear,
} from "../protocol/messages.js";

/* —— Memory faces: RPDR art in web/assets/memory/ —— */
const MEM_ASSET = (name) => `./assets/memory/${name}`;
const MEM_ICONS = [
  { id: "bob", src: MEM_ASSET("bob.jpg") },
  { id: "katya", src: MEM_ASSET("katya.jpg") },
  { id: "jinkx", src: MEM_ASSET("jinkx.png") },
  { id: "michelle", src: MEM_ASSET("michelle.jpg") },
  { id: "trixie", src: MEM_ASSET("trixie.png") },
  { id: "jinkx2", src: MEM_ASSET("jinkx-thumbs.jpg") },
  { id: "michelle2", src: MEM_ASSET("michelle-circle.jpg") },
  { id: "logo", src: MEM_ASSET("rpdr-logo.png") },
];
const MEM_CARD_BACK = MEM_ASSET("card-back.svg");

const DOODLE_PALETTE = [
  { id: 0, hex: "#2a2438" },
  { id: 1, hex: "#e07090" },
  { id: 2, hex: "#e8b056" },
  { id: 3, hex: "#5cb88a" },
  { id: 4, hex: "#5a9fd4" },
  { id: 5, hex: "#9a7ad4" },
  { id: 6, hex: "#e87858" },
  { id: 7, hex: "#f0f0f0" },
];
/** ESP-NOW: color -1 = erase; w = 1|2|3 → thin/med/thick */
const DOODLE_SIZES = [
  { id: 1, px: 3, label: "S" },
  { id: 2, px: 7, label: "M" },
  { id: 3, px: 14, label: "L" },
];
const DOODLE_BG = "#120e1c";
const DOODLE_ERASE = -1;

const CK_SIZE = 8;
/** Quantize doodle coords to 0–120 (≈4px on 480) — 2 values per point, ESP-friendly */
const DOODLE_Q = 120;
const DOODLE_MAX_PTS = 40; // ~80 nums → stays under ESP-NOW JSON budget when chunked

function mulberry32(a) {
  return function () {
    let t = (a += 0x6d2b79f5);
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

function hashSeed(str) {
  let h = 2166136261;
  for (let i = 0; i < str.length; i++) {
    h ^= str.charCodeAt(i);
    h = Math.imul(h, 16777619);
  }
  return h >>> 0;
}

function shuffleInPlace(arr, rand) {
  for (let i = arr.length - 1; i > 0; i--) {
    const j = Math.floor(rand() * (i + 1));
    [arr[i], arr[j]] = [arr[j], arr[i]];
  }
  return arr;
}

export function buildMemoryDeck(seed) {
  const rand = mulberry32(hashSeed(String(seed)));
  const pairs = MEM_ICONS.flatMap((icon) => [icon.id, icon.id]);
  shuffleInPlace(pairs, rand);
  return pairs;
}

function iconFace(id) {
  const src = MEM_ICONS.find((i) => i.id === id)?.src;
  if (!src) return "";
  return `<img class="mem-face" src="${src}" alt="" draggable="false" />`;
}

function cardBackFace() {
  return `<img class="mem-face mem-face-back" src="${MEM_CARD_BACK}" alt="" draggable="false" />`;
}

function emptyCkBoard() {
  const b = Array.from({ length: CK_SIZE }, () => Array(CK_SIZE).fill(""));
  for (let y = 0; y < CK_SIZE; y++) {
    for (let x = 0; x < CK_SIZE; x++) {
      if ((x + y) % 2 === 0) continue;
      if (y < 3) b[y][x] = "b";
      if (y > 4) b[y][x] = "r";
    }
  }
  return b;
}

function isRed(p) {
  return p === "r" || p === "R";
}
function isBlack(p) {
  return p === "b" || p === "B";
}
function isKing(p) {
  return p === "R" || p === "B";
}
function sideOf(p) {
  if (isRed(p)) return "r";
  if (isBlack(p)) return "b";
  return "";
}

function cloneBoard(board) {
  return board.map((row) => [...row]);
}

/** All legal moves for side ('r'|'b'). Jumps preferred when any exist. */
function legalMoves(board, side, fromOnly = null) {
  const jumps = [];
  const steps = [];

  for (let y = 0; y < CK_SIZE; y++) {
    for (let x = 0; x < CK_SIZE; x++) {
      if (fromOnly && (fromOnly.x !== x || fromOnly.y !== y)) continue;
      const p = board[y][x];
      if (sideOf(p) !== side) continue;

      const moveDirs = isKing(p)
        ? [
            [-1, -1],
            [1, -1],
            [-1, 1],
            [1, 1],
          ]
        : side === "r"
          ? [
              [-1, -1],
              [1, -1],
            ]
          : [
              [-1, 1],
              [1, 1],
            ];

      for (const [dx, dy] of moveDirs) {
        const nx = x + dx;
        const ny = y + dy;
        if (nx < 0 || nx >= CK_SIZE || ny < 0 || ny >= CK_SIZE) continue;
        if ((nx + ny) % 2 === 0) continue;
        if (!board[ny][nx]) {
          steps.push({ fromX: x, fromY: y, toX: nx, toY: ny, jump: false });
        }
      }

      for (const [dx, dy] of [
        [-1, -1],
        [1, -1],
        [-1, 1],
        [1, 1],
      ]) {
        if (!isKing(p)) {
          const forward = side === "r" ? dy < 0 : dy > 0;
          if (!forward) continue;
        }
        const mx = x + dx;
        const my = y + dy;
        const lx = x + dx * 2;
        const ly = y + dy * 2;
        if (lx < 0 || lx >= CK_SIZE || ly < 0 || ly >= CK_SIZE) continue;
        if ((lx + ly) % 2 === 0) continue;
        const mid = board[my]?.[mx];
        if (!mid || sideOf(mid) === side) continue;
        if (board[ly][lx]) continue;
        jumps.push({ fromX: x, fromY: y, toX: lx, toY: ly, jump: true });
      }
    }
  }
  return jumps.length ? jumps : steps;
}

function applyCkMove(board, move) {
  const p = board[move.fromY][move.fromX];
  board[move.fromY][move.fromX] = "";
  let piece = p;
  if (isRed(p) && move.toY === 0) piece = "R";
  if (isBlack(p) && move.toY === CK_SIZE - 1) piece = "B";
  board[move.toY][move.toX] = piece;
  if (move.jump) {
    const mx = (move.fromX + move.toX) / 2;
    const my = (move.fromY + move.toY) / 2;
    board[my][mx] = "";
  }
  return piece;
}

function countSide(board, side) {
  let n = 0;
  for (const row of board) for (const c of row) if (sideOf(c) === side) n++;
  return n;
}

function quantizePt(x, y, canvas) {
  const qx = Math.max(0, Math.min(DOODLE_Q, Math.round((x / canvas.width) * DOODLE_Q)));
  const qy = Math.max(0, Math.min(DOODLE_Q, Math.round((y / canvas.height) * DOODLE_Q)));
  return [qx, qy];
}

function dequantizePt(qx, qy, canvas) {
  return [(qx / DOODLE_Q) * canvas.width, (qy / DOODLE_Q) * canvas.height];
}

/**
 * @param {object} api
 */
export function installMoreGames(api) {
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
    updateBrandSub,
    confirmForfeit,
  } = api;

  let doodleColor = 1;
  let doodleSize = 2; // 1=S 2=M 3=L
  let doodleErase = false;
  let doodleStrokeId = 1;
  let drawing = false;
  /** @type {number[]} */
  let strokePts = [];
  let doodlePeer = null;

  function activeKey() {
    return activeDeskKeyRef.current;
  }

  function ensureFields(d) {
    if (!("ckInvite" in d)) d.ckInvite = null;
    if (!("ckGame" in d)) d.ckGame = null;
    if (!("memInvite" in d)) d.memInvite = null;
    if (!("memGame" in d)) d.memGame = null;
    if (!("doodlePeerId" in d)) d.doodlePeerId = null;
    if (!("doodlePeerName" in d)) d.doodlePeerName = null;
  }
  Object.values(desks).forEach(ensureFields);

  function setDock(elId, buttons) {
    const dock = document.getElementById(elId);
    if (!dock) return;
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

  function fillPeers(listId, onPick) {
    const list = document.getElementById(listId);
    const d = desk();
    list.innerHTML = "";
    if (d.peers.size === 0) {
      list.innerHTML = '<p class="tagline">No peers yet — scan from Settings.</p>';
      return;
    }
    for (const peer of d.peers.values()) {
      const btn = document.createElement("button");
      btn.type = "button";
      btn.className = "peer-btn";
      btn.innerHTML = `${escapeHtml(peer.name)}<small>challenge</small>`;
      btn.addEventListener("click", () => onPick(peer));
      list.appendChild(btn);
    }
  }

  /* ========== CHECKERS ========== */
  function showCkPanel(id) {
    for (const p of ["ckPick", "ckWaiting", "ckInvite", "ckPlay"]) {
      const el = document.getElementById(p);
      if (el) el.hidden = p !== id;
    }
    const result = document.getElementById("ckResult");
    if (result && id !== "ckPlay") result.hidden = true;
  }

  function renderCkScreen() {
    const d = desk();
    ensureFields(d);

    if (d.ckInvite) {
      showCkPanel("ckInvite");
      document.getElementById("ckInviteName").textContent = d.ckInvite.fromName;
      setDock("ckDock", [
        {
          label: "Accept",
          primary: true,
          onClick: () => {
            const inv = d.ckInvite;
            if (!inv) return;
            broadcast(activeKey(), makeCkAccept(d.id, d.name, inv.fromId));
            d.ckInvite = null;
            d.ckGame = {
              opponentId: inv.fromId,
              opponentName: inv.fromName,
              side: "b",
              board: emptyCkBoard(),
              turn: "r",
              selected: null,
              mustJumpFrom: null,
              over: false,
              resultDismissed: false,
              waiting: false,
            };
            renderCkScreen();
          },
        },
        {
          label: "Decline",
          danger: true,
          onClick: () => {
            if (!d.ckInvite) return;
            broadcast(activeKey(), makeCkDecline(d.id, d.name, d.ckInvite.fromId));
            d.ckInvite = null;
            syncActiveDeskUi();
          },
        },
      ]);
      return;
    }

    if (d.ckGame?.waiting) {
      showCkPanel("ckWaiting");
      document.getElementById("ckWaitingName").textContent = d.ckGame.opponentName;
      setDock("ckDock", [
        {
          label: "Cancel",
          danger: true,
          onClick: () => {
            broadcast(activeKey(), makeCkForfeit(d.id, d.name, d.ckGame.opponentId));
            d.ckGame = null;
            syncActiveDeskUi();
          },
        },
      ]);
      return;
    }

    if (d.ckGame) {
      showCkPanel("ckPlay");
      renderCkBoard();
      updateBrandSub?.("checkers");
      const result = document.getElementById("ckResult");
      if (d.ckGame.over && !d.ckGame.resultDismissed) {
        result.hidden = false;
        const opp = d.ckGame.side === "r" ? "b" : "r";
        const myCount = countSide(d.ckGame.board, d.ckGame.side);
        const oppCount = countSide(d.ckGame.board, opp);
        let win = myCount > 0 && oppCount === 0;
        if (!win && myCount === 0) win = false;
        else if (oppCount > 0 && myCount > 0) {
          win = d.ckGame.turn !== d.ckGame.side;
        }
        document.getElementById("ckResultEmoji").textContent = win ? "👑" : "💀";
        document.getElementById("ckResultText").textContent = win ? "Condragulations!" : "Sashay away…";
        result.onclick = () => {
          d.ckGame.resultDismissed = true;
          result.hidden = true;
          updateBrandSub?.("checkers");
        };
      } else result.hidden = true;

      setDock("ckDock", [
        {
          label: "Forfeit",
          danger: true,
          onClick: () => {
            confirmForfeit(() => {
              broadcast(activeKey(), makeCkForfeit(d.id, d.name, d.ckGame.opponentId));
              d.ckGame = null;
              syncActiveDeskUi();
            });
          },
        },
      ]);
      return;
    }

    showCkPanel("ckPick");
    fillPeers("ckPeerList", (peer) => {
      d.ckGame = {
        opponentId: peer.id,
        opponentName: peer.name,
        side: "r",
        board: emptyCkBoard(),
        turn: "r",
        selected: null,
        mustJumpFrom: null,
        over: false,
        resultDismissed: false,
        waiting: true,
      };
      broadcast(activeKey(), makeCkInvite(d.id, d.name, peer.id));
      renderCkScreen();
    });
    setDock("ckDock", [{ label: "Back", onClick: () => showScreen("gamesfolder") }]);
  }

  function viewFlip(side) {
    return side === "b";
  }

  function toBoardCoords(side, viewX, viewY) {
    if (!viewFlip(side)) return { x: viewX, y: viewY };
    return { x: CK_SIZE - 1 - viewX, y: CK_SIZE - 1 - viewY };
  }

  function renderCkBoard() {
    const g = desk().ckGame;
    const boardEl = document.getElementById("ckBoard");
    if (!g || !boardEl) return;
    const moves = g.selected
      ? legalMoves(g.board, g.side, g.mustJumpFrom || g.selected).filter(
          (m) => m.fromX === g.selected.x && m.fromY === g.selected.y
        )
      : [];
    const destSet = new Set(moves.map((m) => `${m.toX},${m.toY}`));
    const flip = viewFlip(g.side);

    boardEl.innerHTML = "";
    boardEl.classList.toggle("ck-flipped", flip);
    for (let viewY = 0; viewY < CK_SIZE; viewY++) {
      for (let viewX = 0; viewX < CK_SIZE; viewX++) {
        const { x, y } = toBoardCoords(g.side, viewX, viewY);
        const cell = document.createElement("button");
        cell.type = "button";
        const dark = (x + y) % 2 === 1;
        cell.className = `ck-cell ${dark ? "dark" : "light"}`;
        if (g.selected?.x === x && g.selected?.y === y) cell.classList.add("selected");
        if (destSet.has(`${x},${y}`)) cell.classList.add("dest");
        const p = g.board[y][x];
        if (p) {
          const piece = document.createElement("span");
          piece.className = `ck-piece ${isRed(p) ? "red" : "black"}${isKing(p) ? " king" : ""}`;
          cell.appendChild(piece);
        }
        cell.addEventListener("click", () => onCkTap(x, y));
        boardEl.appendChild(cell);
      }
    }
  }

  function onCkTap(x, y) {
    const d = desk();
    const g = d.ckGame;
    if (!g || g.over || g.waiting || g.turn !== g.side) return;

    if (g.selected) {
      const moves = legalMoves(g.board, g.side, g.mustJumpFrom || g.selected).filter(
        (m) => m.fromX === g.selected.x && m.fromY === g.selected.y && m.toX === x && m.toY === y
      );
      if (moves.length) {
        doCkMove(moves[0], true);
        return;
      }
      if (!g.mustJumpFrom) g.selected = null;
    }

    if (g.mustJumpFrom) {
      renderCkBoard();
      return;
    }

    const p = g.board[y][x];
    if (sideOf(p) === g.side) {
      g.selected = { x, y };
    }
    renderCkBoard();
  }

  function doCkMove(move, send) {
    const d = desk();
    const g = d.ckGame;
    if (!g) return;
    applyCkMove(g.board, move);
    if (send) {
      broadcast(activeKey(), makeCkMove(d.id, g.opponentId, move.fromX, move.fromY, move.toX, move.toY));
    }

    if (move.jump) {
      const more = legalMoves(g.board, g.side, { x: move.toX, y: move.toY }).filter((m) => m.jump);
      if (more.length) {
        g.mustJumpFrom = { x: move.toX, y: move.toY };
        g.selected = { x: move.toX, y: move.toY };
        renderCkScreen();
        return;
      }
    }

    g.mustJumpFrom = null;
    g.selected = null;
    const opp = g.side === "r" ? "b" : "r";
    g.turn = send ? opp : g.side;

    if (countSide(g.board, "r") === 0 || countSide(g.board, "b") === 0) {
      g.over = true;
      g.resultDismissed = false;
    } else if (legalMoves(g.board, g.turn).length === 0) {
      g.over = true;
      g.resultDismissed = false;
    }
    renderCkScreen();
  }

  function applyIncomingCkMove(targetKey, msg) {
    const d = desks[targetKey];
    const g = d.ckGame;
    if (!g || g.opponentId !== msg.fromId) return;
    const move = {
      fromX: msg.fromX,
      fromY: msg.fromY,
      toX: msg.toX,
      toY: msg.toY,
      jump: Math.abs(msg.toX - msg.fromX) === 2,
    };
    applyCkMove(g.board, move);
    if (move.jump) {
      const oppSide = g.side === "r" ? "b" : "r";
      const more = legalMoves(g.board, oppSide, { x: move.toX, y: move.toY }).filter((m) => m.jump);
      if (more.length) {
        // opponent still jumping — keep their turn
        if (targetKey === activeKey()) renderCkScreen();
        return;
      }
    }
    g.turn = g.side;
    if (countSide(g.board, "r") === 0 || countSide(g.board, "b") === 0) {
      g.over = true;
      g.resultDismissed = false;
    } else if (legalMoves(g.board, g.turn).length === 0) {
      g.over = true;
      g.resultDismissed = false;
    }
    if (targetKey === activeKey()) {
      renderCkScreen();
      showScreen("checkers");
    }
  }

  /* ========== MEMORY ========== */
  function showMemPanel(id) {
    for (const p of ["memPick", "memWaiting", "memInvite", "memPlay"]) {
      const el = document.getElementById(p);
      if (el) el.hidden = p !== id;
    }
    const result = document.getElementById("memResult");
    if (result && id !== "memPlay") result.hidden = true;
  }

  function startMemGame(d, opponentId, opponentName, seed, iAmFirst) {
    d.memGame = {
      opponentId,
      opponentName,
      seed,
      deck: buildMemoryDeck(seed),
      flipped: [],
      matched: Array(16).fill(false),
      myScore: 0,
      oppScore: 0,
      myTurn: iAmFirst,
      lock: false,
      over: false,
      resultDismissed: false,
      waiting: false,
      localFlip: null,
    };
  }

  function renderMemScreen() {
    const d = desk();
    ensureFields(d);

    if (d.memInvite) {
      showMemPanel("memInvite");
      document.getElementById("memInviteName").textContent = d.memInvite.fromName;
      setDock("memDock", [
        {
          label: "Accept",
          primary: true,
          onClick: () => {
            const inv = d.memInvite;
            if (!inv) return;
            broadcast(activeKey(), makeMemAccept(d.id, d.name, inv.fromId));
            d.memInvite = null;
            startMemGame(d, inv.fromId, inv.fromName, inv.seed, false);
            renderMemScreen();
          },
        },
        {
          label: "Decline",
          danger: true,
          onClick: () => {
            if (!d.memInvite) return;
            broadcast(activeKey(), makeMemDecline(d.id, d.name, d.memInvite.fromId));
            d.memInvite = null;
            syncActiveDeskUi();
          },
        },
      ]);
      return;
    }

    if (d.memGame?.waiting) {
      showMemPanel("memWaiting");
      document.getElementById("memWaitingName").textContent = d.memGame.opponentName;
      setDock("memDock", [
        {
          label: "Cancel",
          danger: true,
          onClick: () => {
            broadcast(activeKey(), makeMemForfeit(d.id, d.name, d.memGame.opponentId));
            d.memGame = null;
            syncActiveDeskUi();
          },
        },
      ]);
      return;
    }

    if (d.memGame) {
      showMemPanel("memPlay");
      const g = d.memGame;
      renderMemBoard();
      updateBrandSub?.("memory");
      const result = document.getElementById("memResult");
      if (g.over && !g.resultDismissed) {
        result.hidden = false;
        const win = g.myScore > g.oppScore;
        const tie = g.myScore === g.oppScore;
        document.getElementById("memResultEmoji").textContent = tie ? "🤝" : win ? "👑" : "💀";
        document.getElementById("memResultText").textContent = tie
          ? "Double win!"
          : win
            ? "Condragulations!"
            : "Sashay away…";
        result.onclick = () => {
          g.resultDismissed = true;
          result.hidden = true;
          updateBrandSub?.("memory");
        };
      } else result.hidden = true;

      setDock("memDock", [
        {
          label: "Forfeit",
          danger: true,
          onClick: () => {
            confirmForfeit(() => {
              broadcast(activeKey(), makeMemForfeit(d.id, d.name, g.opponentId));
              d.memGame = null;
              syncActiveDeskUi();
            });
          },
        },
      ]);
      return;
    }

    showMemPanel("memPick");
    fillPeers("memPeerList", (peer) => {
      const seed = `${Date.now().toString(36)}-${Math.random().toString(36).slice(2, 8)}`;
      startMemGame(d, peer.id, peer.name, seed, true);
      d.memGame.waiting = true;
      broadcast(activeKey(), makeMemInvite(d.id, d.name, peer.id, seed));
      renderMemScreen();
    });
    setDock("memDock", [{ label: "Back", onClick: () => showScreen("gamesfolder") }]);
  }

  function renderMemBoard() {
    const g = desk().memGame;
    const board = document.getElementById("memBoard");
    if (!g || !board) return;
    board.innerHTML = "";
    for (let i = 0; i < 16; i++) {
      const btn = document.createElement("button");
      btn.type = "button";
      btn.className = "mem-card";
      const faceUp =
        g.matched[i] || g.flipped.includes(i) || g.localFlip === i;
      if (faceUp) {
        btn.classList.add("up");
        btn.innerHTML = `<span class="mem-icon">${iconFace(g.deck[i])}</span>`;
      } else {
        btn.classList.add("down");
        btn.innerHTML = `<span class="mem-back">${cardBackFace()}</span>`;
      }
      if (g.matched[i]) btn.classList.add("matched");
      btn.addEventListener("click", () => onMemTap(i));
      board.appendChild(btn);
    }
  }

  function onMemTap(i) {
    const d = desk();
    const g = d.memGame;
    if (!g || g.over || g.waiting || !g.myTurn || g.lock) return;
    if (g.matched[i] || g.flipped.includes(i) || g.localFlip === i) return;

    if (g.localFlip == null) {
      g.localFlip = i;
      renderMemBoard();
      return;
    }

    const a = g.localFlip;
    const b = i;
    g.localFlip = null;
    g.flipped = [a, b];
    g.lock = true;
    renderMemBoard();
    broadcast(activeKey(), makeMemFlip(d.id, g.opponentId, a, b));
    resolveMemFlip(d, g, a, b, true);
  }

  function resolveMemFlip(d, g, a, b, iPlayed) {
    const match = g.deck[a] === g.deck[b];
    setTimeout(() => {
      if (match) {
        g.matched[a] = true;
        g.matched[b] = true;
        if (iPlayed) g.myScore++;
        else g.oppScore++;
        g.myTurn = iPlayed;
      } else {
        g.myTurn = !iPlayed;
      }

      g.flipped = [];
      g.lock = false;
      if (g.matched.every(Boolean)) {
        g.over = true;
        g.resultDismissed = false;
      }
      if (d === desk() && activeKey()) {
        renderMemScreen();
      }
    }, 700);
  }

  function applyIncomingMemFlip(targetKey, msg) {
    const d = desks[targetKey];
    const g = d.memGame;
    if (!g || g.opponentId !== msg.fromId) return;
    g.flipped = [msg.cardA, msg.cardB];
    g.localFlip = null;
    g.lock = true;
    g.myTurn = false;
    if (targetKey === activeKey()) {
      renderMemScreen();
      showScreen("memory");
    }
    resolveMemFlip(d, g, msg.cardA, msg.cardB, false);
  }

  /* ========== DOODLE ========== */
  function getCanvas() {
    return document.getElementById("doodleCanvas");
  }

  function canvasCtx() {
    const c = getCanvas();
    return c ? c.getContext("2d") : null;
  }

  function clearCanvasLocal() {
    const c = getCanvas();
    const ctx = canvasCtx();
    if (!c || !ctx) return;
    ctx.globalCompositeOperation = "source-over";
    ctx.fillStyle = DOODLE_BG;
    ctx.fillRect(0, 0, c.width, c.height);
  }

  function sizePx(sizeId) {
    return DOODLE_SIZES.find((s) => s.id === sizeId)?.px || 7;
  }

  function strokeStyleFor(colorId) {
    if (colorId === DOODLE_ERASE) return DOODLE_BG;
    return DOODLE_PALETTE.find((p) => p.id === colorId)?.hex || "#e07090";
  }

  function drawStrokePts(pts, colorId, widthId) {
    const c = getCanvas();
    const ctx = canvasCtx();
    if (!c || !ctx || pts.length < 4) return;
    ctx.globalCompositeOperation = "source-over";
    ctx.strokeStyle = strokeStyleFor(colorId);
    ctx.lineWidth = sizePx(widthId);
    ctx.lineCap = "round";
    ctx.lineJoin = "round";
    ctx.beginPath();
    for (let i = 0; i < pts.length; i += 2) {
      const [x, y] = dequantizePt(pts[i], pts[i + 1], c);
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();
  }

  function beginLocalStroke(ctx, x, y) {
    ctx.globalCompositeOperation = "source-over";
    ctx.strokeStyle = doodleErase ? DOODLE_BG : strokeStyleFor(doodleColor);
    ctx.lineWidth = sizePx(doodleSize);
    ctx.lineCap = "round";
    ctx.lineJoin = "round";
    ctx.beginPath();
    ctx.moveTo(x, y);
  }

  function sendStrokeChunks(pts) {
    const d = desk();
    if (!d.doodlePeerId || pts.length < 4) return;
    const sid = doodleStrokeId++;
    let seq = 0;
    const color = doodleErase ? DOODLE_ERASE : doodleColor;
    for (let i = 0; i < pts.length; i += DOODLE_MAX_PTS * 2) {
      const chunk = pts.slice(i, i + DOODLE_MAX_PTS * 2);
      const last = i + DOODLE_MAX_PTS * 2 >= pts.length;
      broadcast(
        activeKey(),
        makeDoodleStroke(d.id, d.name, d.doodlePeerId, {
          strokeId: sid,
          seq: seq++,
          last,
          color,
          w: doodleSize,
          pts: chunk,
        })
      );
    }
  }

  function renderDoodleScreen() {
    const d = desk();
    ensureFields(d);
    const pick = document.getElementById("doodlePick");
    const draw = document.getElementById("doodleDraw");

    if (!d.doodlePeerId) {
      pick.hidden = false;
      draw.hidden = true;
      const list = document.getElementById("doodlePeerList");
      list.innerHTML = "";
      if (d.peers.size === 0) {
        list.innerHTML = '<p class="tagline">No peers yet — scan from Settings.</p>';
      } else {
        for (const peer of d.peers.values()) {
          const btn = document.createElement("button");
          btn.type = "button";
          btn.className = "peer-btn";
          btn.innerHTML = `${escapeHtml(peer.name)}<small>draw together</small>`;
          btn.addEventListener("click", () => {
            d.doodlePeerId = peer.id;
            d.doodlePeerName = peer.name;
            doodlePeer = peer;
            renderDoodleScreen();
            updateBrandSub?.("doodle");
          });
          list.appendChild(btn);
        }
      }
      setDock("doodleDock", [{ label: "Home", onClick: () => showScreen("hub") }]);
      return;
    }

    pick.hidden = true;
    draw.hidden = false;

    const colors = document.getElementById("doodleColors");
    colors.innerHTML = "";
    for (const col of DOODLE_PALETTE) {
      const btn = document.createElement("button");
      btn.type = "button";
      btn.className = "doodle-swatch";
      btn.style.background = col.hex;
      btn.setAttribute("aria-pressed", !doodleErase && col.id === doodleColor ? "true" : "false");
      btn.addEventListener("click", () => {
        doodleErase = false;
        doodleColor = col.id;
        renderDoodleScreen();
      });
      colors.appendChild(btn);
    }
    const eraseBtn = document.createElement("button");
    eraseBtn.type = "button";
    eraseBtn.className = "doodle-swatch doodle-eraser";
    eraseBtn.setAttribute("aria-pressed", doodleErase ? "true" : "false");
    eraseBtn.title = "Eraser";
    eraseBtn.setAttribute("aria-label", "Eraser");
    eraseBtn.innerHTML =
      '<svg viewBox="0 0 24 24" width="16" height="16" aria-hidden="true"><path fill="currentColor" d="M16.2 3.5 20.5 7.8a2 2 0 0 1 0 2.8l-8.9 8.9H5.8l-2.3-2.3a2 2 0 0 1 0-2.8l11-11a2 2 0 0 1 2.7 0ZM7.2 17.2h2.6l7.4-7.4-2.6-2.6-7.4 7.4v2.6Z"/></svg>';
    eraseBtn.addEventListener("click", () => {
      doodleErase = true;
      renderDoodleScreen();
    });
    colors.appendChild(eraseBtn);

    const sizes = document.getElementById("doodleSizes");
    sizes.innerHTML = "";
    for (const sz of DOODLE_SIZES) {
      const btn = document.createElement("button");
      btn.type = "button";
      btn.className = "doodle-size";
      btn.textContent = sz.label;
      btn.setAttribute("aria-pressed", sz.id === doodleSize ? "true" : "false");
      btn.addEventListener("click", () => {
        doodleSize = sz.id;
        renderDoodleScreen();
      });
      sizes.appendChild(btn);
    }

    const c = getCanvas();
    if (c && !c.dataset.ready) {
      c.dataset.ready = "1";
      clearCanvasLocal();
      const pos = (ev) => {
        const r = c.getBoundingClientRect();
        return {
          x: ((ev.clientX - r.left) / r.width) * c.width,
          y: ((ev.clientY - r.top) / r.height) * c.height,
        };
      };
      c.addEventListener("pointerdown", (ev) => {
        c.setPointerCapture(ev.pointerId);
        drawing = true;
        strokePts = [];
        const { x, y } = pos(ev);
        strokePts.push(...quantizePt(x, y, c));
        beginLocalStroke(canvasCtx(), x, y);
      });
      c.addEventListener("pointermove", (ev) => {
        if (!drawing) return;
        const { x, y } = pos(ev);
        const [qx, qy] = quantizePt(x, y, c);
        const prevX = strokePts[strokePts.length - 2];
        const prevY = strokePts[strokePts.length - 1];
        if (qx === prevX && qy === prevY) return;
        strokePts.push(qx, qy);
        const ctx = canvasCtx();
        ctx.lineTo(x, y);
        ctx.stroke();
        ctx.beginPath();
        ctx.moveTo(x, y);
      });
      const endStroke = () => {
        if (!drawing) return;
        drawing = false;
        sendStrokeChunks(strokePts);
        strokePts = [];
      };
      c.addEventListener("pointerup", endStroke);
      c.addEventListener("pointercancel", endStroke);
    }

    setDock("doodleDock", [
      {
        label: "Clear",
        onClick: () => {
          clearCanvasLocal();
          broadcast(activeKey(), makeDoodleClear(d.id, d.name, d.doodlePeerId));
        },
      },
      {
        label: "Back",
        onClick: () => {
          d.doodlePeerId = null;
          d.doodlePeerName = null;
          renderDoodleScreen();
          updateBrandSub?.("doodle");
        },
      },
      { label: "Home", onClick: () => showScreen("hub") },
    ]);
  }

  function onRadio(msg, fromDeskKey) {
    if (msg.type === MessageType.CK_INVITE) {
      const targetKey = deskKeyById(msg.toId);
      if (!targetKey || targetKey === fromDeskKey) return true;
      const t = desks[targetKey];
      ensureFields(t);
      if (t.ckGame || t.ckInvite) return true;
      t.ckInvite = { fromId: msg.fromId, fromName: msg.fromName };
      if (targetKey === activeKey()) {
        renderCkScreen();
        showScreen("checkers");
        toast(`${msg.fromName} challenged you · Checkers`);
      }
      return true;
    }
    if (msg.type === MessageType.CK_ACCEPT) {
      const key = deskKeyById(msg.toId);
      if (!key) return true;
      const d = desks[key];
      if (!d.ckGame?.waiting || d.ckGame.opponentId !== msg.fromId) return true;
      d.ckGame.waiting = false;
      d.ckGame.opponentName = msg.fromName;
      if (key === activeKey()) {
        renderCkScreen();
        showScreen("checkers");
      }
      return true;
    }
    if (msg.type === MessageType.CK_DECLINE) {
      const key = deskKeyById(msg.toId);
      if (!key) return true;
      const d = desks[key];
      if (d.ckGame?.waiting && d.ckGame.opponentId === msg.fromId) {
        d.ckGame = null;
        if (key === activeKey()) {
          toast(`${msg.fromName} declined`);
          syncActiveDeskUi();
        }
      }
      return true;
    }
    if (msg.type === MessageType.CK_MOVE) {
      const key = deskKeyById(msg.toId);
      if (!key || key === fromDeskKey) return true;
      applyIncomingCkMove(key, msg);
      return true;
    }
    if (msg.type === MessageType.CK_FORFEIT) {
      const key = deskKeyById(msg.toId);
      if (!key) return true;
      const d = desks[key];
      if (d.ckInvite?.fromId === msg.fromId) d.ckInvite = null;
      if (d.ckGame?.opponentId === msg.fromId) d.ckGame = null;
      if (key === activeKey()) {
        toast(`${msg.fromName} left Checkers`);
        syncActiveDeskUi();
      }
      return true;
    }

    if (msg.type === MessageType.MEM_INVITE) {
      const targetKey = deskKeyById(msg.toId);
      if (!targetKey || targetKey === fromDeskKey) return true;
      const t = desks[targetKey];
      ensureFields(t);
      if (t.memGame || t.memInvite) return true;
      t.memInvite = { fromId: msg.fromId, fromName: msg.fromName, seed: msg.seed };
      if (targetKey === activeKey()) {
        renderMemScreen();
        showScreen("memory");
        toast(`${msg.fromName} challenged you · Memory`);
      }
      return true;
    }
    if (msg.type === MessageType.MEM_ACCEPT) {
      const key = deskKeyById(msg.toId);
      if (!key) return true;
      const d = desks[key];
      if (!d.memGame?.waiting || d.memGame.opponentId !== msg.fromId) return true;
      d.memGame.waiting = false;
      d.memGame.opponentName = msg.fromName;
      if (key === activeKey()) {
        renderMemScreen();
        showScreen("memory");
      }
      return true;
    }
    if (msg.type === MessageType.MEM_DECLINE) {
      const key = deskKeyById(msg.toId);
      if (!key) return true;
      const d = desks[key];
      if (d.memGame?.waiting && d.memGame.opponentId === msg.fromId) {
        d.memGame = null;
        if (key === activeKey()) {
          toast(`${msg.fromName} declined`);
          syncActiveDeskUi();
        }
      }
      return true;
    }
    if (msg.type === MessageType.MEM_FLIP) {
      const key = deskKeyById(msg.toId);
      if (!key || key === fromDeskKey) return true;
      applyIncomingMemFlip(key, msg);
      return true;
    }
    if (msg.type === MessageType.MEM_FORFEIT) {
      const key = deskKeyById(msg.toId);
      if (!key) return true;
      const d = desks[key];
      if (d.memInvite?.fromId === msg.fromId) d.memInvite = null;
      if (d.memGame?.opponentId === msg.fromId) d.memGame = null;
      if (key === activeKey()) {
        toast(`${msg.fromName} left Memory`);
        syncActiveDeskUi();
      }
      return true;
    }

    if (msg.type === MessageType.DOODLE_STROKE) {
      const key = deskKeyById(msg.toId);
      if (!key || key === fromDeskKey) return true;
      const d = desks[key];
      ensureFields(d);
      if (!d.doodlePeerId) {
        d.doodlePeerId = msg.fromId;
        d.doodlePeerName = msg.fromName;
      }
      if (key === activeKey()) {
        if (currentScreenIsDoodle()) {
          drawStrokePts(msg.pts || [], msg.color ?? 1, msg.w || 2);
        } else {
          toast(`Doodle from ${msg.fromName}`);
          renderDoodleScreen();
          showScreen("doodle");
          requestAnimationFrame(() => drawStrokePts(msg.pts || [], msg.color ?? 1, msg.w || 2));
        }
      }
      return true;
    }
    if (msg.type === MessageType.DOODLE_CLEAR) {
      const key = deskKeyById(msg.toId);
      if (!key || key === fromDeskKey) return true;
      if (key === activeKey()) clearCanvasLocal();
      return true;
    }

    return false;
  }

  function currentScreenIsDoodle() {
    return document.querySelector('.screen.active')?.dataset.screen === "doodle";
  }

  return {
    renderCkScreen,
    renderMemScreen,
    renderDoodleScreen,
    onRadio,
    busy(d) {
      ensureFields(d);
      return Boolean(d.ckInvite || d.ckGame || d.memInvite || d.memGame);
    },
  };
}
