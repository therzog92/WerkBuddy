/**
 * WerkPager message protocol (shared stub).
 * ESP-NOW payload target: keep serialized messages small (~ESP-NOW ~250B/chunk).
 * Firmware can mirror these type strings + fields later.
 */

export const MessageType = Object.freeze({
  DISCOVER: 'discover',
  DISCOVER_REPLY: 'discover_reply',
  CALL: 'call',
  ACK: 'ack',
  CLEAR: 'clear',
  STATUS: 'status',
  DOODLE: 'doodle',
  TTT_INVITE: 'ttt_invite',
  TTT_ACCEPT: 'ttt_accept',
  TTT_DECLINE: 'ttt_decline',
  TTT_MOVE: 'ttt_move',
  TTT_FORFEIT: 'ttt_forfeit',
  C4_INVITE: 'c4_invite',
  C4_ACCEPT: 'c4_accept',
  C4_DECLINE: 'c4_decline',
  C4_DROP: 'c4_drop',
  C4_FORFEIT: 'c4_forfeit',
  BS_INVITE: 'bs_invite',
  BS_ACCEPT: 'bs_accept',
  BS_DECLINE: 'bs_decline',
  BS_READY: 'bs_ready',
  BS_FIRE: 'bs_fire',
  BS_RESULT: 'bs_result',
  BS_FORFEIT: 'bs_forfeit',
  CK_INVITE: 'ck_invite',
  CK_ACCEPT: 'ck_accept',
  CK_DECLINE: 'ck_decline',
  CK_MOVE: 'ck_move',
  CK_FORFEIT: 'ck_forfeit',
  MEM_INVITE: 'mem_invite',
  MEM_ACCEPT: 'mem_accept',
  MEM_DECLINE: 'mem_decline',
  MEM_FLIP: 'mem_flip',
  MEM_FORFEIT: 'mem_forfeit',
  STTT_INVITE: 'sttt_invite',
  STTT_ACCEPT: 'sttt_accept',
  STTT_DECLINE: 'sttt_decline',
  STTT_MOVE: 'sttt_move',
  STTT_FORFEIT: 'sttt_forfeit',
  DOODLE_STROKE: 'doodle_stroke',
  DOODLE_CLEAR: 'doodle_clear',
});

/**
 * @typedef {Object} Peer
 * @property {string} id - stable id (MAC hex on device; uuid in web sim)
 * @property {string} name
 * @property {string} [status] - free | busy | afk | headphones
 */

/**
 * @param {string} fromId
 * @param {string} fromName
 * @returns {{ type: string, fromId: string, fromName: string, ts: number }}
 */
export function makeDiscover(fromId, fromName) {
  return { type: MessageType.DISCOVER, fromId, fromName, ts: Date.now() };
}

/**
 * @param {string} fromId
 * @param {string} fromName
 * @returns {{ type: string, fromId: string, fromName: string, ts: number }}
 */
export function makeDiscoverReply(fromId, fromName) {
  return { type: MessageType.DISCOVER_REPLY, fromId, fromName, ts: Date.now() };
}

/**
 * @param {string} fromId
 * @param {string} fromName
 * @param {string} toId - recipient device id (MAC on hardware)
 * @param {{ emoji?: string|null, message?: string|null }} [extras]
 * @returns {{ type: string, fromId: string, fromName: string, toId: string, emoji: string|null, message: string|null, ts: number }}
 */
export function makeCall(fromId, fromName, toId, extras = {}) {
  return {
    type: MessageType.CALL,
    fromId,
    fromName,
    toId,
    emoji: extras.emoji ?? null,
    message: extras.message ?? null,
    ts: Date.now(),
  };
}

/**
 * @param {string} fromId
 * @param {string} fromName
 * @param {string} forCallFromId
 * @returns {{ type: string, fromId: string, fromName: string, forCallFromId: string, ts: number }}
 */
export function makeAck(fromId, fromName, forCallFromId) {
  return {
    type: MessageType.ACK,
    fromId,
    fromName,
    forCallFromId,
    ts: Date.now(),
  };
}

/**
 * @param {string} fromId
 * @param {string} fromName
 * @returns {{ type: string, fromId: string, fromName: string, ts: number }}
 */
export function makeClear(fromId, fromName) {
  return { type: MessageType.CLEAR, fromId, fromName, ts: Date.now() };
}

export function makeTttInvite(fromId, fromName, toId) {
  return { type: MessageType.TTT_INVITE, fromId, fromName, toId, ts: Date.now() };
}

export function makeTttAccept(fromId, fromName, toId) {
  return { type: MessageType.TTT_ACCEPT, fromId, fromName, toId, ts: Date.now() };
}

export function makeTttDecline(fromId, fromName, toId) {
  return { type: MessageType.TTT_DECLINE, fromId, fromName, toId, ts: Date.now() };
}

export function makeTttMove(fromId, toId, cell, mark) {
  return { type: MessageType.TTT_MOVE, fromId, toId, cell, mark, ts: Date.now() };
}

export function makeTttForfeit(fromId, fromName, toId) {
  return { type: MessageType.TTT_FORFEIT, fromId, fromName, toId, ts: Date.now() };
}

export function makeStttInvite(fromId, fromName, toId) {
  return { type: MessageType.STTT_INVITE, fromId, fromName, toId, ts: Date.now() };
}

export function makeStttAccept(fromId, fromName, toId) {
  return { type: MessageType.STTT_ACCEPT, fromId, fromName, toId, ts: Date.now() };
}

export function makeStttDecline(fromId, fromName, toId) {
  return { type: MessageType.STTT_DECLINE, fromId, fromName, toId, ts: Date.now() };
}

/** @param {number} board mini-board 0..8 @param {number} cell cell 0..8 */
export function makeStttMove(fromId, toId, board, cell, mark) {
  return { type: MessageType.STTT_MOVE, fromId, toId, board, cell, mark, ts: Date.now() };
}

export function makeStttForfeit(fromId, fromName, toId) {
  return { type: MessageType.STTT_FORFEIT, fromId, fromName, toId, ts: Date.now() };
}

function party(type, fromId, fromName, toId, extra = {}) {
  return { type, fromId, fromName, toId, ts: Date.now(), ...extra };
}

export const makeC4Invite = (a, b, c, color = "pink") =>
  party(MessageType.C4_INVITE, a, b, c, { color });
export const makeC4Accept = (a, b, c, color = "gold") =>
  party(MessageType.C4_ACCEPT, a, b, c, { color });
export const makeC4Decline = (a, b, c) => party(MessageType.C4_DECLINE, a, b, c);
export const makeC4Drop = (fromId, toId, col, color) =>
  party(MessageType.C4_DROP, fromId, "", toId, { col, color });
export const makeC4Forfeit = (a, b, c) => party(MessageType.C4_FORFEIT, a, b, c);

export const makeBsInvite = (a, b, c) => party(MessageType.BS_INVITE, a, b, c);
export const makeBsAccept = (a, b, c) => party(MessageType.BS_ACCEPT, a, b, c);
export const makeBsDecline = (a, b, c) => party(MessageType.BS_DECLINE, a, b, c);
export const makeBsReady = (a, b, c) => party(MessageType.BS_READY, a, b, c);
export const makeBsFire = (fromId, toId, x, y) =>
  party(MessageType.BS_FIRE, fromId, "", toId, { x, y });
export const makeBsResult = (fromId, toId, x, y, hit, sunk = false, gameOver = false) =>
  party(MessageType.BS_RESULT, fromId, "", toId, { x, y, hit, sunk, gameOver });
export const makeBsForfeit = (a, b, c) => party(MessageType.BS_FORFEIT, a, b, c);

export const makeCkInvite = (a, b, c) => party(MessageType.CK_INVITE, a, b, c);
export const makeCkAccept = (a, b, c) => party(MessageType.CK_ACCEPT, a, b, c);
export const makeCkDecline = (a, b, c) => party(MessageType.CK_DECLINE, a, b, c);
/** @param {number} fromX @param {number} fromY @param {number} toX @param {number} toY */
export const makeCkMove = (fromId, toId, fromX, fromY, toX, toY) =>
  party(MessageType.CK_MOVE, fromId, "", toId, { fromX, fromY, toX, toY });
export const makeCkForfeit = (a, b, c) => party(MessageType.CK_FORFEIT, a, b, c);

/** @param {string} seed */
export const makeMemInvite = (fromId, fromName, toId, seed) =>
  party(MessageType.MEM_INVITE, fromId, fromName, toId, { seed });
export const makeMemAccept = (a, b, c) => party(MessageType.MEM_ACCEPT, a, b, c);
export const makeMemDecline = (a, b, c) => party(MessageType.MEM_DECLINE, a, b, c);
/** @param {number} cardA @param {number} cardB */
export const makeMemFlip = (fromId, toId, cardA, cardB) =>
  party(MessageType.MEM_FLIP, fromId, "", toId, { cardA, cardB });
export const makeMemForfeit = (a, b, c) => party(MessageType.MEM_FORFEIT, a, b, c);

/**
 * Stroke sync for ESP-NOW: quantized points (0–120 ≈ 4px on 480 display).
 * Keep pts short; split long strokes with strokeId + seq + last.
 * @param {{ strokeId: number, seq?: number, last?: boolean, color: number, w?: number, pts: number[] }} stroke
 *   color: palette id, or -1 to erase; w: 1|2|3 (S/M/L)
 *   pts flat [x0,y0,x1,y1,…] — 2 bytes per point in binary firmware
 */
export const makeDoodleStroke = (fromId, fromName, toId, stroke) =>
  party(MessageType.DOODLE_STROKE, fromId, fromName, toId, stroke);
export const makeDoodleClear = (fromId, fromName, toId) =>
  party(MessageType.DOODLE_CLEAR, fromId, fromName, toId);

/**
 * JSON serialize for transport. On device, prefer a tighter binary pack later.
 * @param {object} msg
 * @returns {string}
 */
export function encode(msg) {
  return JSON.stringify(msg);
}

/**
 * @param {string} raw
 * @returns {object}
 */
export function decode(raw) {
  return JSON.parse(raw);
}
