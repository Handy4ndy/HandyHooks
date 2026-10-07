/**
 * Auction House V2 Bids - full matrix (xahau.js).
 * Setup Sub+Create+Bids, create auctions, bid happy/reject/passthrough/
 * outbid-refund/buy-now/IOU/XAH.
 *
 * Max bid (p2_* cases): self-raise on a separate fresh host that
 * carries pinned Sub + Create, the new Bids, the max bid Finalise and the
 * test-only seed hook (SmokeStateSeed.wasm via SEED_WASM, op 0x09) in one
 * shared namespace. Without the seed file the seeded PRC cases are skipped.
 * Max bid (p3_* cases) run on the same fresh host: proxy price
 * climb, ties, underbids with refund, IFR cap (same-ledger burst and seeded),
 * outbid string, buy-now overpay, low host float (forged LCK, restored),
 * PEN block, IFR strand and claim, admin CLR bitmask, IOU variants and timed
 * Finalise settling at PRC with the remainder refunded.
 *
 * Run: node IT_BIDS.js
 * Writes IT_BIDS.json next to this script.
 */
import fs from 'fs';
import path from 'path';
import crypto from 'crypto';
import { fileURLToPath } from 'url';
import { Client, Wallet, decodeAccountID } from 'xahau';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const OUTDIR = __dirname;
const ROOT = path.resolve(__dirname, '..');
const WS = process.env.XAHAU_WS || 'wss://xahau-test.net';
const FAUCET_URL = process.env.XAHAU_FAUCET || 'https://xahau-test.net/accounts';
const NETWORK_ID = 21338;
const ASF_DISALLOW_INCOMING_REMIT = 16;
const ASF_DEPOSIT_AUTH = 9;
const ASF_REQUIRE_DEST_TAG = 1;

const WASM_BIDS = fs.readFileSync(path.join(OUTDIR, 'AuctionBids.wasm'));
const WASM_CREATE = fs.readFileSync(path.join(ROOT, 'Create', 'AuctionCreate.wasm'));
const WASM_SUB = fs.readFileSync(
  fs.existsSync(path.join(ROOT, 'Subscription', 'AuctionSub.wasm'))
    ? path.join(ROOT, 'Subscription', 'AuctionSub.wasm')
    : path.join(ROOT, 'AuctionSub.wasm'),
);

const WASM_FIN = fs.readFileSync(path.join(ROOT, 'Finalise', 'AuctionFinalise.wasm'));
/* Test-only seed hook for the seeded PRC/IFR cases. Set SEED_WASM to its path. */
const SEED_DEFAULT = path.join(__dirname, '..', '_tools', 'SmokeStateSeed.wasm');
const SEED_PATH = process.env.SEED_WASM || SEED_DEFAULT;
if (process.env.SEED_WASM && !fs.existsSync(SEED_PATH)) {
  throw new Error('SEED_WASM is set but no file exists at ' + SEED_PATH);
}
if (!fs.existsSync(SEED_PATH)) {
  console.error('SEED_WASM not set: seeded PRC/IFR cases need the test seed hook (SmokeStateSeed.wasm) and will be skipped');
}
const WASM_SEED = fs.existsSync(SEED_PATH) ? fs.readFileSync(SEED_PATH) : null;
const HOOK_ON_INVOKE = 'FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFFFFFFFFFFFFFBFFFFF';

const BIDS_HASH = crypto.createHash('sha512').update(WASM_BIDS).digest().slice(0, 32).toString('hex').toUpperCase();
const CREATE_HASH = crypto.createHash('sha512').update(WASM_CREATE).digest().slice(0, 32).toString('hex').toUpperCase();
const SUB_HASH = crypto.createHash('sha512').update(WASM_SUB).digest().slice(0, 32).toString('hex').toUpperCase();
const NS_SUB = crypto.createHash('sha256').update('AuctionHouseV2Sub-bids-matrix-' + Date.now()).digest().toString('hex').toUpperCase();
const NS_CREATE = crypto.createHash('sha256').update('AuctionHouseV2Create-bids-matrix-' + Date.now()).digest().toString('hex').toUpperCase();
const NS_BIDS = crypto.createHash('sha256').update('AuctionHouseV2Bids-matrix-' + Date.now()).digest().toString('hex').toUpperCase();

const HOOK_ON_PAYMENT_INVOKE = 'FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFFFFFFFFFFFFFBFFFFE';
const HOOK_ON_CREATE = 'FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF77FFFFFFFFFFFFFFFFFBFFFFE';
const HOOK_ON_BIDS = 'FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF77FFFFFFFFFFFFFFFFFBFFFFE';
const HSF_OVERRIDE = 1;

const PRICE = 10_000_000n;
const PERIOD = 3600;
const SPLIT_PCT = 0;
const AUCCAP = 20;
const DUR_S = 3600;

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const logLines = [];
function log(...a) {
  const line = a.map((x) => (typeof x === 'string' ? x : JSON.stringify(x))).join(' ');
  console.log(line);
  logLines.push(line);
}

function u64be(n) {
  const b = Buffer.alloc(8);
  b.writeBigUInt64BE(BigInt(n));
  return b.toString('hex').toUpperCase();
}
function u32be(n) {
  const b = Buffer.alloc(4);
  b.writeUInt32BE(Number(n) >>> 0);
  return b.toString('hex').toUpperCase();
}
function u16be(n) {
  const b = Buffer.alloc(2);
  b.writeUInt16BE(n & 0xffff);
  return b.toString('hex').toUpperCase();
}
function accHex(addr) {
  return Buffer.from(decodeAccountID(addr)).toString('hex').toUpperCase();
}
function hp(name, valueHex) {
  return {
    HookParameter: {
      HookParameterName: Buffer.from(name, 'ascii').toString('hex').toUpperCase(),
      HookParameterValue: String(valueHex).toUpperCase(),
    },
  };
}
function curIso(s) {
  return Buffer.from(s, 'ascii').toString('hex').toUpperCase();
}

/** Canonical positive XFL hex (bit62 set, exp bits 54-61 bias 97, 15-digit mant). */
function xflHex(val) {
  if (val === 0 || val === 0n) return '0000000000000000';
  const v = Math.abs(Number(val));
  let exp = Math.floor(Math.log10(v)) - 15;
  let mant = Math.round(v / 10 ** exp);
  while (mant >= 10_000_000_000_000_000) { mant = Math.floor(mant / 10); exp += 1; }
  while (mant < 1_000_000_000_000_000 && mant > 0) { mant *= 10; exp -= 1; }
  const e = BigInt(exp + 97);
  let raw = (1n << 62n) | (e << 54n) | BigInt(mant);
  if (Number(val) < 0) raw |= (1n << 63n);
  return raw.toString(16).toUpperCase().padStart(16, '0');
}

function genWallet() {
  try { return Wallet.generate('ecdsa-secp256k1'); }
  catch { return Wallet.generate(); }
}

function walletFromFaucetSecret(secret) {
  for (const algo of ['ecdsa-secp256k1', 'ed25519', undefined]) {
    try {
      return algo ? Wallet.fromSeed(secret, { algorithm: algo }) : Wallet.fromSeed(secret);
    } catch { /* next */ }
  }
  throw new Error('cannot decode faucet secret');
}

async function faucetWallet() {
  let last;
  for (let i = 0; i < 8; i++) {
    try {
      const res = await fetch(FAUCET_URL, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: '{}',
      });
      const text = await res.text();
      if (!res.ok) throw new Error(`faucet HTTP ${res.status}: ${text.slice(0, 160)}`);
      const body = JSON.parse(text);
      const acct = body.account || body;
      const address = acct.classicAddress || acct.address || body.address;
      const secret = acct.secret || acct.seed || body.secret;
      if (!address || !secret) throw new Error('faucet bad body');
      return walletFromFaucetSecret(secret);
    } catch (e) {
      last = e;
      log('faucet retry', String(i), String(e.message || e).slice(0, 120));
      await sleep(4000 * (i + 1));
    }
  }
  throw last || new Error('faucet failed');
}

async function submitAndWait(client, wallet, tx) {
  /* testnet ws drops: wait for the auto reconnect before submitting */
  for (let i = 0; i < 60 && !client.isConnected(); i++) await sleep(1000);
  const prepared = await client.autofill({ ...tx, NetworkID: NETWORK_ID });
  const signed = wallet.sign(prepared);
  let wd;
  const result = await Promise.race([
    client.submitAndWait(signed.tx_blob),
    new Promise((_, rej) => { wd = setTimeout(() => rej(new Error('submit watchdog 240s ' + signed.hash)), 240000); }),
  ]).finally(() => clearTimeout(wd));
  const engine = result.result?.meta?.TransactionResult
    || result.result?.engine_result
    || 'unknown';
  return {
    engine,
    hash: result.result?.hash || signed.hash,
    meta: result.result?.meta,
  };
}

function softSubmit(p) {
  return p.catch((e) => ({
    engine: String(e?.data?.engine_result || e?.message || e).slice(0, 220),
    meta: e?.data?.meta,
    hash: e?.data?.hash,
  }));
}

function decodeHr(meta) {
  const he = meta?.HookExecutions || [];
  return he.map((h) => {
    const e = h.HookExecution || {};
    let msg = e.HookReturnString || '';
    if (typeof msg === 'string' && /^[0-9A-Fa-f]+$/.test(msg) && msg.length % 2 === 0) {
      msg = Buffer.from(msg, 'hex').toString('utf8').replace(/\0+$/, '');
    }
    return {
      result: e.HookResult,
      code: e.HookReturnCode,
      msg,
      emit: e.HookEmitCount,
      hash: e.HookHash,
    };
  });
}

function bidsMsgs(hrs) {
  return hrs
    .filter((h) => String(h.hash || '').toUpperCase() === BIDS_HASH)
    .map((h) => h.msg)
    .filter(Boolean);
}
function anyMsgs(hrs) {
  return hrs.map((h) => h.msg).filter(Boolean);
}
function bidsHr(hrs) {
  return hrs.find((h) => String(h.hash || '').toUpperCase() === BIDS_HASH) || null;
}

async function bal(client, acct) {
  const r = await client.request({ command: 'account_info', account: acct, ledger_index: 'validated' });
  return BigInt(r.result.account_data.Balance);
}

async function uriOwner(client, uriId) {
  const r = await client.request({
    command: 'ledger_entry',
    index: String(uriId).toUpperCase(),
    ledger_index: 'validated',
  }).catch(() => null);
  const node = r?.result?.node || r?.result?.ledger_entry;
  return node?.Owner || null;
}

async function waitUriOwner(client, uriId, wantAddr, maxMs = 90000) {
  const start = Date.now();
  while (Date.now() - start < maxMs) {
    const o = await uriOwner(client, uriId);
    if (o && String(o) === String(wantAddr)) return true;
    await sleep(2000);
  }
  return false;
}


async function pay(client, from, to, drops) {
  const r = await softSubmit(submitAndWait(client, from, {
    TransactionType: 'Payment',
    Account: from.classicAddress,
    Destination: typeof to === 'string' ? to : to.classicAddress,
    Amount: String(drops),
  }));
  if (r.engine !== 'tesSUCCESS') throw new Error('pay ' + r.engine);
  return r;
}

async function invokeAdmin(client, signer, host, name, valueHex) {
  return softSubmit(submitAndWait(client, signer, {
    TransactionType: 'Invoke',
    Account: signer.classicAddress,
    Destination: host.classicAddress,
    HookParameters: [hp(name, valueHex)],
  }));
}

async function subPay(client, seller, host, drops) {
  return softSubmit(submitAndWait(client, seller, {
    TransactionType: 'Payment',
    Account: seller.classicAddress,
    Destination: host.classicAddress,
    Amount: String(drops),
    HookParameters: [hp('SUB', '01')],
  }));
}

async function mintUT(client, seller) {
  const uri = `aucv3bids:${Date.now()}:${Math.random().toString(16).slice(2)}`;
  const uriHex = Buffer.from(uri, 'utf8').toString('hex').toUpperCase();
  const digest = crypto.createHash('sha256').update(uri, 'utf8').digest('hex').toUpperCase();
  const r = await softSubmit(submitAndWait(client, seller, {
    TransactionType: 'URITokenMint',
    Account: seller.classicAddress,
    URI: uriHex,
    Digest: digest,
  }));
  if (r.engine !== 'tesSUCCESS') throw new Error(`mint fail ${r.engine}`);
  let id = null;
  for (const n of r.meta?.AffectedNodes || []) {
    const created = n.CreatedNode;
    if (created?.LedgerEntryType === 'URIToken') {
      id = created.LedgerIndex || created.NewFields?.URITokenID;
    }
  }
  if (!id) {
    const ao = await client.request({
      command: 'account_objects',
      account: seller.classicAddress,
      type: 'uri_token',
      ledger_index: 'validated',
    });
    const objs = ao.result.account_objects || [];
    id = objs[objs.length - 1]?.index || objs[objs.length - 1]?.URITokenID;
  }
  if (!id) throw new Error('no URIToken id');
  return String(id).toUpperCase();
}

async function createRemit(client, seller, host, lot, params) {
  return softSubmit(submitAndWait(client, seller, {
    TransactionType: 'Remit',
    Account: seller.classicAddress,
    Destination: host.classicAddress,
    URITokenIDs: [lot],
    HookParameters: Object.entries(params).map(([k, v]) => hp(k, v)),
  }));
}

function aidFrom(txHash, uriId) {
  const pre = Buffer.concat([
    Buffer.from(String(txHash).replace(/^0x/i, ''), 'hex'),
    Buffer.from(String(uriId).replace(/^0x/i, ''), 'hex'),
  ]);
  return crypto.createHash('sha512').update(pre).digest().slice(0, 32).toString('hex').toUpperCase();
}


async function readHostLocalKeys(client, hostAddr, namespaceId, names) {
  const ns = await client.request({
    command: 'account_namespace',
    account: hostAddr,
    namespace_id: namespaceId,
    ledger_index: 'validated',
  }).catch(() => null);
  const found = {};
  for (const o of ns?.result?.namespace_entries || []) {
    const k = String(o.HookStateKey || '').toUpperCase();
    const data = String(o.HookStateData || '').toUpperCase();
    for (const name of names) {
      const suf = Buffer.from(name, 'ascii').toString('hex').toUpperCase();
      if (k.endsWith(suf)) found[name] = data;
    }
  }
  return found;
}

function iouLckKeyHex(currency20Hex, issuer20Hex) {
  const pre = Buffer.concat([
    Buffer.from(String(currency20Hex).replace(/^0x/i, ''), 'hex'),
    Buffer.from(String(issuer20Hex).replace(/^0x/i, ''), 'hex'),
  ]);
  return crypto.createHash('sha512').update(pre).digest().slice(0, 32).toString('hex').toUpperCase();
}

async function readAidKeys(client, hostAddr, aid) {
  const ns = await client.request({
    command: 'account_namespace',
    account: hostAddr,
    namespace_id: aid,
    ledger_index: 'validated',
  }).catch(() => null);
  const want = ['DUR', 'SP', 'MB', 'BN', 'CUR', 'ISS', 'SLR', 'URI', 'EXP', 'ST', 'HIGH', 'WIN', 'BCNT', 'BNW', 'WDT', 'RFD', 'RFDA', 'RFDT', 'PEN', 'SSF', 'TSF', 'CPR', 'PRC', 'IFR'];
  const found = {};
  for (const o of ns?.result?.namespace_entries || []) {
    const k = String(o.HookStateKey || '').toUpperCase();
    const data = String(o.HookStateData || '').toUpperCase();
    for (const name of want) {
      const suf = Buffer.from(name, 'ascii').toString('hex').toUpperCase();
      if (k.endsWith(suf)) found[name] = data;
    }
  }
  return found;
}

async function waitHostTrustLine(client, hostAddr, issuerAddr, currencyIso, maxMs = 90000) {
  const start = Date.now();
  while (Date.now() - start < maxMs) {
    const r = await client.request({
      command: 'account_lines',
      account: hostAddr,
      peer: issuerAddr,
      ledger_index: 'validated',
    }).catch(() => null);
    for (const line of r?.result?.lines || []) {
      if (line.currency === currencyIso && Number(line.limit || 0) > 0) return true;
    }
    await sleep(2000);
  }
  return false;
}

async function bidPay(client, bidder, host, amount, aid, extra = {}) {
  const tx = {
    TransactionType: 'Payment',
    Account: bidder.classicAddress,
    Destination: host.classicAddress,
    Amount: amount,
    HookParameters: [hp('AID', aid)],
    ...extra,
  };
  return softSubmit(submitAndWait(client, bidder, tx));
}

function expectCase(name, r, want) {
  const hrs = decodeHr(r.meta);
  const msgs = anyMsgs(hrs);
  const bMsgs = bidsMsgs(hrs);
  const primary = bMsgs[0] || msgs[0] || '';
  const engineOk = want.engine === 'tesSUCCESS'
    ? r.engine === 'tesSUCCESS'
    : r.engine !== 'tesSUCCESS';
  let msgOk = true;
  const pool = want.bidsOnly ? bMsgs : (want.anyHook ? msgs : bMsgs.length ? bMsgs : msgs);
  if (want.msg != null) {
    msgOk = pool.includes(want.msg) || primary === want.msg;
  }
  if (want.msgIncludes) {
    msgOk = pool.some((m) => m.includes(want.msgIncludes))
      || primary.includes(want.msgIncludes);
  }
  if (want.msgAnyOf) {
    msgOk = want.msgAnyOf.some((m) => pool.includes(m) || msgs.includes(m));
  }
  if (want.emitMin != null) {
    const hr = bidsHr(hrs) || hrs[0];
    if (!(Number(hr?.emit || 0) >= want.emitMin)) msgOk = false;
  }
  if (want.emitMax != null) {
    const hr = bidsHr(hrs) || hrs[0];
    if (!(Number(hr?.emit || 0) <= want.emitMax)) msgOk = false;
  }
  return {
    name,
    pass: engineOk && msgOk,
    engine: r.engine,
    hash: r.hash,
    hook: hrs,
    want,
    gotMsg: primary,
    gotMsgs: msgs,
    bidsMsgs: bMsgs,
  };
}

async function main() {
  const OUT = {
    when: new Date().toISOString(),
    ws: WS,
    hook_hashes: {
      Bids: BIDS_HASH,
      Create: CREATE_HASH,
      Sub: SUB_HASH,
    },
    wasm_bytes: {
      Bids: WASM_BIDS.length,
      Create: WASM_CREATE.length,
      Sub: WASM_SUB.length,
    },
    namespaces: { NS_SUB, NS_CREATE, NS_BIDS },
    cases: [],
    pass: 0,
    fail: 0,
    log: logLines,
  };

  function record(c) {
    OUT.cases.push(c);
    if (c.pass) OUT.pass++;
    else OUT.fail++;
    log(c.pass ? 'PASS' : 'FAIL', c.name, c.engine, c.gotMsg || '', c.pass ? '' : JSON.stringify(c.want));
  }
  function save() {
    OUT.log = logLines.slice();
    fs.writeFileSync(path.join(OUTDIR, 'IT_BIDS.json'), JSON.stringify(OUT, null, 2));
  }

  log('Bids HookHash', BIDS_HASH, 'bytes', WASM_BIDS.length);
  log('Create HookHash', CREATE_HASH, 'bytes', WASM_CREATE.length);
  log('Sub HookHash', SUB_HASH, 'bytes', WASM_SUB.length);

  const client = new Client(WS);
  /* testnet ws flakes: retry read-only requests on timeout or disconnect
     with backoff, force a fresh connection after a timeout (submits are
     never retried here) */
  {
    const READ_CMDS = new Set(['ledger', 'ledger_current', 'ledger_entry', 'account_info', 'account_lines',
      'account_objects', 'account_namespace', 'account_tx', 'tx', 'server_state', 'server_info', 'fee']);
    const rawReq = client.request.bind(client);
    client.request = async (req) => {
      for (let i = 0; ; i++) {
        try {
          return await rawReq(req);
        } catch (e) {
          const m = String((e && e.name) || '') + ' ' + String((e && e.message) || e);
          if (!READ_CMDS.has(req.command) || i >= 10 || !/Timeout|NotConnected|not open|Disconnected|CONNECTING/i.test(m)) throw e;
          await sleep(Math.min(3000 * 2 ** i, 30000));
          if (/Timeout/i.test(m)) {
            try { await client.disconnect(); } catch { /* */ }
            try { await client.connect(); } catch { /* */ }
          }
          for (let j = 0; j < 30 && !client.isConnected(); j++) await sleep(1000);
        }
      }
    };
  }
  await client.connect();

  const bank = genWallet();
  const host = genWallet();
  const admin = genWallet();
  const treasury = genWallet();
  const issuer = genWallet();
  const seller = genWallet();
  const bidderA = genWallet();
  const bidderB = genWallet();
  const bidderC = genWallet();
  const other = genWallet();

  OUT.accounts = {
    bank: bank.classicAddress,
    host: host.classicAddress,
    admin: admin.classicAddress,
    treasury: treasury.classicAddress,
    issuer: issuer.classicAddress,
    seller: seller.classicAddress,
    bidderA: bidderA.classicAddress,
    bidderB: bidderB.classicAddress,
    bidderC: bidderC.classicAddress,
    other: other.classicAddress,
  };

  async function ensureBank(minDrops) {
    for (let attempt = 0; attempt < 12; attempt++) {
      let b = 0n;
      try { b = await bal(client, bank.classicAddress); } catch { /* */ }
      if (b >= minDrops) return b;
      log('bank top-up via faucet, bal', String(b), 'need', String(minDrops));
      const donor = await faucetWallet();
      for (let i = 0; i < 40; i++) {
        try {
          const db = await bal(client, donor.classicAddress);
          if (db > 50_000_000n) break;
        } catch { /* */ }
        await sleep(1000);
      }
      const db = await bal(client, donor.classicAddress);
      const send = db > 20_000_000n ? db - 15_000_000n : 0n;
      if (send > 0n) {
        await softSubmit(submitAndWait(client, donor, {
          TransactionType: 'Payment',
          Account: donor.classicAddress,
          Destination: bank.classicAddress,
          Amount: String(send),
        }));
      }
      await sleep(1500);
    }
    return bal(client, bank.classicAddress);
  }

  log('funding...');
  const fundPlan = [
    [host, 500_000_000n],
    [admin, 40_000_000n],
    [treasury, 30_000_000n],
    [issuer, 80_000_000n],
    [seller, 200_000_000n],
    [bidderA, 200_000_000n],
    [bidderB, 200_000_000n],
    [bidderC, 150_000_000n],
    [other, 40_000_000n],
  ];
  for (const [w, drops] of fundPlan) {
    await ensureBank(drops + 40_000_000n);
    await pay(client, bank, w, drops);
  }

  /* ---- SetHook Sub + Create + Bids ---- */
  {
    const r = await softSubmit(submitAndWait(client, host, {
      TransactionType: 'SetHook',
      Account: host.classicAddress,
      Hooks: [
        {
          Hook: {
            CreateCode: WASM_SUB.toString('hex').toUpperCase(),
            Flags: HSF_OVERRIDE,
            HookApiVersion: 0,
            HookNamespace: NS_SUB,
            HookOn: HOOK_ON_PAYMENT_INVOKE,
            HookParameters: [hp('ADMIN', accHex(admin.classicAddress))],
          },
        },
        {
          Hook: {
            CreateCode: WASM_CREATE.toString('hex').toUpperCase(),
            Flags: HSF_OVERRIDE,
            HookApiVersion: 0,
            HookNamespace: NS_CREATE,
            HookOn: HOOK_ON_CREATE,
          },
        },
        {
          Hook: {
            CreateCode: WASM_BIDS.toString('hex').toUpperCase(),
            Flags: HSF_OVERRIDE,
            HookApiVersion: 0,
            HookNamespace: NS_BIDS,
            HookOn: HOOK_ON_BIDS,
          },
        },
      ],
    }));
    record(expectCase('setup_sethook_sub_create_bids', r, { engine: 'tesSUCCESS', anyHook: true }));
    if (r.engine !== 'tesSUCCESS') {
      OUT.blocker = 'SetHook failed: ' + r.engine;
      save();
      await client.disconnect();
      process.exit(2);
    }
  }

  for (const [name, hex] of [
    ['SUBPRICE', u64be(PRICE)],
    ['SUBPERIOD', u32be(PERIOD)],
    ['SUBSPLIT', u16be(SPLIT_PCT)],
    ['AUCCAP', u16be(AUCCAP)],
    ['TREASURY', accHex(treasury.classicAddress)],
  ]) {
    record(expectCase('setup_sub_set_' + name, await invokeAdmin(client, admin, host, name, hex), {
      engine: 'tesSUCCESS',
      anyHook: true,
    }));
  }

  {
    const r = await subPay(client, seller, host, PRICE);
    record(expectCase('setup_sub_seller', r, {
      engine: 'tesSUCCESS',
      msgIncludes: 'Subscription',
      anyHook: true,
    }));
  }

  /* ===== Passthrough ===== */
  {
    const r = await softSubmit(submitAndWait(client, other, {
      TransactionType: 'Payment',
      Account: other.classicAddress,
      Destination: host.classicAddress,
      Amount: '1000000',
    }));
    record(expectCase('bids_payment_no_aid_passthrough', r, {
      engine: 'tesSUCCESS',
      msgIncludes: 'passthrough',
      bidsOnly: true,
    }));
  }
  {
    const r = await softSubmit(submitAndWait(client, other, {
      TransactionType: 'Invoke',
      Account: other.classicAddress,
      Destination: host.classicAddress,
    }));
    record(expectCase('bids_invoke_passthrough', r, {
      engine: 'tesSUCCESS',
      msgIncludes: 'Passthrough',
      bidsOnly: true,
    }));
  }
  {
    const r = await softSubmit(submitAndWait(client, host, {
      TransactionType: 'Payment',
      Account: host.classicAddress,
      Destination: other.classicAddress,
      Amount: '1000000',
    }));
    record(expectCase('bids_outgoing_ok', r, {
      engine: 'tesSUCCESS',
      msgAnyOf: ['Outgoing ok', 'Passthrough', 'Payment passthrough'],
      anyHook: true,
    }));
  }
  {
    const r = await subPay(client, seller, host, PRICE);
    record(expectCase('bids_sub_payment_passthrough', r, {
      engine: 'tesSUCCESS',
      msgIncludes: 'passthrough',
      bidsOnly: true,
    }));
  }

  /* ===== Create auctions ===== */
  const auctions = {};

  async function makeAuction(label, params) {
    const lot = await mintUT(client, seller);
    const r = await createRemit(client, seller, host, lot, params);
    const ok = r.engine === 'tesSUCCESS';
    const aid = ok ? aidFrom(r.hash, lot) : null;
    record({
      name: 'setup_create_' + label,
      pass: ok,
      engine: r.engine,
      hash: r.hash,
      gotMsg: decodeHr(r.meta).map((h) => h.msg).join('|'),
      want: { engine: 'tesSUCCESS' },
      aid,
      lot,
    });
    if (ok) auctions[label] = { aid, lot, hash: r.hash, params };
    return auctions[label];
  }

  await makeAuction('xah_full', {
    DUR: u64be(DUR_S),
    SP: u64be(1_000_000),
    MB: u64be(100_000),
    BN: u64be(10_000_000),
  });
  await makeAuction('xah_sp_only', {
    DUR: u64be(DUR_S),
    SP: u64be(2_000_000),
  });
  await makeAuction('xah_no_sp', {
    DUR: u64be(DUR_S),
  });
  await makeAuction('xah_bn_only', {
    DUR: u64be(DUR_S),
    BN: u64be(5_000_000),
  });
  await makeAuction('xah_mb_only', {
    DUR: u64be(DUR_S),
    SP: u64be(500_000),
    MB: u64be(50_000),
  });
  await makeAuction('xah_outbid', {
    DUR: u64be(DUR_S),
    SP: u64be(1_000_000),
    MB: u64be(200_000),
  });
  await makeAuction('xah_buynow', {
    DUR: u64be(DUR_S),
    SP: u64be(1_000_000),
    BN: u64be(8_000_000),
  });
  await makeAuction('xah_seller_bid', {
    DUR: u64be(DUR_S),
    SP: u64be(1_000_000),
  });

  /* Issuer DefaultRipple BEFORE Create TrustSet so host line is ripple-enabled.
   * If DefaultRipple is set after Create, issuer keeps NoRipple on the host line
   * and IOU Remit refunds fail with tecPATH_DRY (see bids_iou_outbid_refund_settled). */
  {
    const r = await softSubmit(submitAndWait(client, issuer, {
      TransactionType: 'AccountSet',
      Account: issuer.classicAddress,
      SetFlag: 8, // asfDefaultRipple
    }));
    record(expectCase('setup_issuer_default_ripple', r, { engine: 'tesSUCCESS', anyHook: true }));
  }

  /* IOU auction */
  {
    const lot = await mintUT(client, seller);
    const r = await createRemit(client, seller, host, lot, {
      DUR: u64be(DUR_S),
      SP: xflHex(10),
      MB: xflHex(1),
      BN: xflHex(100),
      CUR: curIso('AUC'),
      ISS: accHex(issuer.classicAddress),
    });
    const ok = r.engine === 'tesSUCCESS';
    const aid = ok ? aidFrom(r.hash, lot) : null;
    record({
      name: 'setup_create_iou_full',
      pass: ok,
      engine: r.engine,
      hash: r.hash,
      gotMsg: decodeHr(r.meta).map((h) => h.msg).join('|'),
      want: { engine: 'tesSUCCESS' },
      aid,
    });
    if (ok) auctions.iou_full = { aid, lot, hash: r.hash };
    const lined = await waitHostTrustLine(client, host.classicAddress, issuer.classicAddress, 'AUC');
    record({
      name: 'setup_host_auc_trustline',
      pass: lined,
      engine: lined ? 'ok' : 'timeout',
      gotMsg: lined ? 'line ready' : 'missing',
      want: { trustline: true },
    });
    /* Belt: clear issuer NoRipple toward host if still set (pre-DefaultRipple race). */
    if (lined) {
      const clr = await softSubmit(submitAndWait(client, issuer, {
        TransactionType: 'TrustSet',
        Account: issuer.classicAddress,
        LimitAmount: {
          currency: 'AUC',
          issuer: host.classicAddress,
          value: '0',
        },
        Flags: 262144, // tfClearNoRipple
      }));
      record({
        name: 'setup_issuer_clear_noripple_host',
        pass: clr.engine === 'tesSUCCESS' || clr.engine === 'tecNO_LINE_REDUNDANT',
        engine: clr.engine,
        gotMsg: 'ClearNoRipple host AUC line',
        want: { engine: 'tesSUCCESS|tecNO_LINE_REDUNDANT' },
      });
    }
  }

  /* Fund bidders with AUC IOU */
  for (const [label, w] of [['A', bidderA], ['B', bidderB], ['C', bidderC]]) {
    const ts = await softSubmit(submitAndWait(client, w, {
      TransactionType: 'TrustSet',
      Account: w.classicAddress,
      LimitAmount: {
        currency: 'AUC',
        issuer: issuer.classicAddress,
        value: '1000000',
      },
    }));
    record(expectCase('setup_bidder' + label + '_trust_auc', ts, { engine: 'tesSUCCESS', anyHook: true }));
    const iouPay = await softSubmit(submitAndWait(client, issuer, {
      TransactionType: 'Payment',
      Account: issuer.classicAddress,
      Destination: w.classicAddress,
      Amount: {
        currency: 'AUC',
        issuer: issuer.classicAddress,
        value: '5000',
      },
    }));
    record(expectCase('setup_issuer_pay_bidder' + label, iouPay, { engine: 'tesSUCCESS', anyHook: true }));
  }

  /* ===== Rejects: bad AID / not found / currency / seller ===== */
  {
    const r = await bidPay(client, bidderA, host, '1000000', 'AA'.repeat(16)); // 16 bytes hex = 8 bytes
    // AID value is hex of 32 bytes -> need 64 hex chars. 'AA'*16 = 32 hex = 16 bytes
    record(expectCase('bids_aid_bad_len_reject', r, {
      engine: 'tecHOOK_REJECTED',
      msgIncludes: 'AID must be 32 bytes',
      bidsOnly: true,
    }));
  }
  /* KVT Finding 3: Payment with both SUB + AID -> NOPE */
  {
    const r = await softSubmit(submitAndWait(client, bidderA, {
      TransactionType: 'Payment',
      Account: bidderA.classicAddress,
      Destination: host.classicAddress,
      Amount: '1000000',
      HookParameters: [hp('SUB', '01'), hp('AID', 'AA'.repeat(32))],
    }));
    record(expectCase('bids_sub_and_aid_both_set', r, {
      engine: 'tecHOOK_REJECTED',
      msgIncludes: 'SUB and AID both set',
      anyHook: true,
    }));
  }
  {
    const fake = crypto.randomBytes(32).toString('hex').toUpperCase();
    const r = await bidPay(client, bidderA, host, '1000000', fake);
    record(expectCase('bids_auction_not_found_reject', r, {
      engine: 'tecHOOK_REJECTED',
      msgIncludes: 'auction not found',
      bidsOnly: true,
    }));
  }
  {
    const a = auctions.xah_seller_bid;
    if (a) {
      const r = await bidPay(client, seller, host, '2000000', a.aid);
      record(expectCase('bids_seller_cannot_bid_reject', r, {
        engine: 'tecHOOK_REJECTED',
        msgIncludes: 'seller cannot bid',
        bidsOnly: true,
      }));
    }
  }
  {
    const a = auctions.xah_full;
    if (a) {
      const r = await bidPay(client, bidderA, host, {
        currency: 'AUC',
        issuer: issuer.classicAddress,
        value: '10',
      }, a.aid);
      record(expectCase('bids_xah_auction_iou_pay_reject', r, {
        engine: 'tecHOOK_REJECTED',
        msgIncludes: 'XAH',
        bidsOnly: true,
      }));
    }
  }
  {
    const a = auctions.iou_full;
    if (a) {
      const r = await bidPay(client, bidderA, host, '5000000', a.aid);
      record(expectCase('bids_iou_auction_xah_pay_reject', r, {
        engine: 'tecHOOK_REJECTED',
        msgIncludes: 'IOU',
        bidsOnly: true,
      }));
    }
  }

  /* ===== Happy: first bid XAH ===== */
  {
    const a = auctions.xah_full;
    if (a) {
      const r = await bidPay(client, bidderA, host, '1000000', a.aid);
      record(expectCase('bids_xah_first_at_sp_ok', r, {
        engine: 'tesSUCCESS',
        msg: 'Max bid accepted',
        bidsOnly: true,
        emitMax: 0,
      }));
      const keys = await readAidKeys(client, host.classicAddress, a.aid);
      record({
        name: 'bids_xah_first_state_high_win_bcnt',
        pass: keys.HIGH === u64be(1_000_000)
          && keys.WIN === accHex(bidderA.classicAddress)
          && keys.BCNT === u32be(1)
          && keys.ST === '01'
          && !keys.BNW,
        engine: 'ok',
        gotMsg: JSON.stringify({ HIGH: keys.HIGH, WIN: keys.WIN, BCNT: keys.BCNT, BNW: keys.BNW }),
        want: { HIGH: u64be(1_000_000), WIN: 'bidderA', BCNT: 1 },
      });
    }
  }

  /* below SP */
  {
    const a = auctions.xah_sp_only;
    if (a) {
      const r = await bidPay(client, bidderA, host, '1000000', a.aid);
      record(expectCase('bids_xah_below_sp_reject', r, {
        engine: 'tecHOOK_REJECTED',
        msgIncludes: 'below start price',
        bidsOnly: true,
      }));
      const r2 = await bidPay(client, bidderA, host, '2000000', a.aid);
      record(expectCase('bids_xah_sp_only_first_ok', r2, {
        engine: 'tesSUCCESS',
        msg: 'Max bid accepted',
        bidsOnly: true,
      }));
    }
  }

  /* no SP: any positive */
  {
    const a = auctions.xah_no_sp;
    if (a) {
      const r = await bidPay(client, bidderA, host, '1', a.aid);
      record(expectCase('bids_xah_no_sp_min_ok', r, {
        engine: 'tesSUCCESS',
        msg: 'Max bid accepted',
        bidsOnly: true,
      }));
    }
  }

  /* Max bid: the high bidder's rebid is a self-raise (was
   * bids_xah_already_high_reject). A at 1 XAH sends 2 XAH: max 3 XAH. */
  {
    const a = auctions.xah_full;
    if (a) {
      const r = await bidPay(client, bidderA, host, '2000000', a.aid);
      record(expectCase('bids_xah_self_raise_ok', r, {
        engine: 'tesSUCCESS',
        msg: 'Max bid raised',
        bidsOnly: true,
        emitMax: 0,
      }));
      const keys = await readAidKeys(client, host.classicAddress, a.aid);
      record({
        name: 'bids_xah_self_raise_high_total',
        pass: keys.HIGH === u64be(3_000_000) && keys.WIN === accHex(bidderA.classicAddress),
        engine: 'ok',
        gotMsg: JSON.stringify({ HIGH: keys.HIGH, WIN: keys.WIN }),
        want: { HIGH: 3000000, WIN: 'A' },
      });
    }
  }

  /* below min increment (MB) */
  {
    const a = auctions.xah_full;
    if (a) {
      const r = await bidPay(client, bidderB, host, '1050000', a.aid); // need >= price 1M + MB 0.1M
      record(expectCase('bids_xah_below_mb_reject', r, {
        engine: 'tecHOOK_REJECTED',
        msgIncludes: 'below min increment',
        bidsOnly: true,
      }));
    }
  }

  /* outbid with refund */
  {
    const a = auctions.xah_outbid;
    if (a) {
      const r1 = await bidPay(client, bidderA, host, '1000000', a.aid);
      record(expectCase('bids_outbid_seat_a', r1, {
        engine: 'tesSUCCESS',
        msg: 'Max bid accepted',
        bidsOnly: true,
      }));
      const balBefore = await bal(client, bidderA.classicAddress);
      const r2 = await bidPay(client, bidderB, host, '1500000', a.aid);
      record(expectCase('bids_outbid_b_refund_a', r2, {
        engine: 'tesSUCCESS',
        msgIncludes: 'prior refund',
        bidsOnly: true,
        emitMin: 1,
      }));
      // wait for emit
      let refunded = false;
      for (let i = 0; i < 30; i++) {
        await sleep(2000);
        const balAfter = await bal(client, bidderA.classicAddress);
        if (balAfter > balBefore + 500_000n) { refunded = true; break; }
      }
      record({
        name: 'bids_outbid_refund_balance_observed',
        pass: refunded,
        engine: refunded ? 'ok' : 'timeout',
        gotMsg: refunded ? 'A balance rose' : 'refund not observed',
        want: { refund: true },
      });
      const keys = await readAidKeys(client, host.classicAddress, a.aid);
      record({
        name: 'bids_outbid_state_win_b_bcnt2',
        pass: keys.WIN === accHex(bidderB.classicAddress)
          && keys.HIGH === u64be(1_500_000)
          && keys.BCNT === u32be(2),
        engine: 'ok',
        gotMsg: JSON.stringify({ HIGH: keys.HIGH, WIN: keys.WIN, BCNT: keys.BCNT }),
        want: { WIN: 'B', HIGH: 1500000, BCNT: 2 },
      });
    }
  }

  /* no-MB: must be strictly > HIGH */
  {
    const a = auctions.xah_sp_only;
    if (a) {
      /* price is SP 2M and HIGH 2M (PRC snapshot == HIGH), so an
       * equal bid is still not above the price */
      const rEq = await bidPay(client, bidderB, host, '2000000', a.aid);
      record(expectCase('bids_xah_no_mb_eq_high_reject', rEq, {
        engine: 'tecHOOK_REJECTED',
        msgIncludes: 'bid not above price',
        bidsOnly: true,
      }));
      const rUp = await bidPay(client, bidderB, host, '2000001', a.aid);
      record(expectCase('bids_xah_no_mb_above_high_ok', rUp, {
        engine: 'tesSUCCESS',
        msgIncludes: 'prior refund',
        bidsOnly: true,
        emitMin: 1,
      }));
    }
  }

  /* Buy-now settle: refund prior + Remit URI to winner + ST=2 */
  {
    const a = auctions.xah_buynow;
    if (a) {
      const balA0 = await bal(client, bidderA.classicAddress);
      const r1 = await bidPay(client, bidderA, host, '1000000', a.aid);
      record(expectCase('bids_buynow_prep_seat', r1, {
        engine: 'tesSUCCESS',
        msg: 'Max bid accepted',
        bidsOnly: true,
      }));
      const r2 = await bidPay(client, bidderB, host, '8000000', a.aid);
      record(expectCase('bids_buynow_hit_refund', r2, {
        engine: 'tesSUCCESS',
        msgIncludes: 'Buy-now accepted',
        bidsOnly: true,
        emitMin: 2,
      }));
      let refunded = false;
      for (let i = 0; i < 20; i++) {
        await sleep(2000);
        const balA1 = await bal(client, bidderA.classicAddress);
        if (balA1 > balA0 + 500_000n) { refunded = true; break; }
      }
      record({
        name: 'bids_buynow_prior_refund_observed',
        pass: refunded,
        engine: refunded ? 'ok' : 'timeout',
        gotMsg: refunded ? 'A balance rose' : 'refund not observed',
        want: { refund: true },
      });
      const uriToB = await waitUriOwner(client, a.lot, bidderB.classicAddress);
      record({
        name: 'bids_buynow_uri_on_winner',
        pass: uriToB,
        engine: uriToB ? 'ok' : 'timeout',
        gotMsg: uriToB ? 'URI owned by B' : 'URI not on B',
        want: { owner: 'B' },
      });
      const keys = await readAidKeys(client, host.classicAddress, a.aid);
      record({
        name: 'bids_buynow_state_st2_win_b',
        pass: keys.ST === '02'
          && keys.BNW === '01'
          && keys.WIN === accHex(bidderB.classicAddress)
          && keys.HIGH === u64be(8_000_000)
          && !keys.URI,
        engine: 'ok',
        gotMsg: JSON.stringify({ BNW: keys.BNW, WIN: keys.WIN, HIGH: keys.HIGH, ST: keys.ST, URI: keys.URI }),
        want: { BNW: 1, ST: 2, WIN: 'B', URI: 'cleared' },
      });
      const r3 = await bidPay(client, bidderC, host, '9000000', a.aid);
      record(expectCase('bids_buynow_locked_reject', r3, {
        engine: 'tecHOOK_REJECTED',
        msgAnyOf: ['auction not open', 'buy-now already won'],
        bidsOnly: true,
      }));
    }
  }

  /* Buy-now exact on empty auction (Remit only, no prior refund) */
  {
    const a = auctions.xah_bn_only;
    if (a) {
      const r = await bidPay(client, bidderA, host, '5000000', a.aid);
      record(expectCase('bids_buynow_first_hit_ok', r, {
        engine: 'tesSUCCESS',
        msg: 'Buy-now accepted',
        bidsOnly: true,
        emitMin: 1,
        emitMax: 1,
      }));
      const uriToA = await waitUriOwner(client, a.lot, bidderA.classicAddress);
      record({
        name: 'bids_buynow_first_uri_on_winner',
        pass: uriToA,
        engine: uriToA ? 'ok' : 'timeout',
        gotMsg: uriToA ? 'URI owned by A' : 'URI not on A',
        want: { owner: 'A' },
      });
      const keys = await readAidKeys(client, host.classicAddress, a.aid);
      record({
        name: 'bids_buynow_first_st2_set',
        pass: keys.ST === '02' && keys.BNW === '01' && keys.BCNT === u32be(1) && !keys.URI,
        engine: 'ok',
        gotMsg: JSON.stringify({ ST: keys.ST, BNW: keys.BNW, BCNT: keys.BCNT, URI: keys.URI }),
        want: { ST: 2, BNW: 1, BCNT: 1, URI: 'cleared' },
      });
    }
  }

  /* High bidder buy-now. Max bid: a self-raise whose new max
   * reaches BN is a buy-now with NO self refund (was C03 refund-self). */
  {
    const a = auctions.xah_mb_only;
    if (a) {
      const r1 = await bidPay(client, bidderA, host, '500000', a.aid);
      record(expectCase('bids_self_bn_prep', r1, {
        engine: 'tesSUCCESS',
        msg: 'Max bid accepted',
        bidsOnly: true,
      }));
      // No BN on this auction - already-high reject already covered.
      // Create a dedicated with BN for self-hit:
    }
  }
  {
    const lot = await mintUT(client, seller);
    const cr = await createRemit(client, seller, host, lot, {
      DUR: u64be(DUR_S),
      SP: u64be(1_000_000),
      BN: u64be(3_000_000),
    });
    const aid = cr.engine === 'tesSUCCESS' ? aidFrom(cr.hash, lot) : null;
    record({
      name: 'setup_create_xah_self_bn',
      pass: cr.engine === 'tesSUCCESS',
      engine: cr.engine,
      hash: cr.hash,
      gotMsg: '',
      want: { engine: 'tesSUCCESS' },
    });
    if (aid) {
      const r1 = await bidPay(client, bidderA, host, '1000000', aid);
      record(expectCase('bids_self_bn_seat', r1, {
        engine: 'tesSUCCESS',
        msg: 'Max bid accepted',
        bidsOnly: true,
      }));
      const lckBeforeK = await readHostLocalKeys(client, host.classicAddress, NS_BIDS, ['LCK']);
      const lckBefore = lckBeforeK.LCK ? BigInt('0x' + lckBeforeK.LCK) : 0n;
      const r2 = await bidPay(client, bidderA, host, '3000000', aid);
      record(expectCase('bids_self_bn_no_refund_ok', r2, {
        engine: 'tesSUCCESS',
        msg: 'Buy-now accepted',
        bidsOnly: true,
        emitMin: 1,
        emitMax: 1,
      }));
      // max becomes 1e6 + 3e6 = 4e6 >= BN 3e6. No refund emit, only
      // the URI Remit. LCK += the 3e6 top-up and nothing comes back out here
      // (overpay 1e6 returns at Finalise through the remainder leg).
      let lckOk = false;
      let lckGot = null;
      const wantLck = lckBefore + 3000000n;
      const t0 = Date.now();
      while (Date.now() - t0 < 90000) {
        const lk = await readHostLocalKeys(client, host.classicAddress, NS_BIDS, ['LCK']);
        const now = lk.LCK ? BigInt('0x' + lk.LCK) : 0n;
        lckGot = now.toString();
        // Also need PEN cleared (refund cbak done)
        const ak = await readAidKeys(client, host.classicAddress, aid);
        if (!ak.PEN && now === wantLck) { lckOk = true; break; }
        await new Promise(r => setTimeout(r, 1500));
      }
      record({
        name: 'bids_self_bn_lck_delta',
        pass: lckOk,
        engine: lckOk ? 'ok' : 'timeout',
        gotMsg: JSON.stringify({ lckBefore: lckBefore.toString(), lckGot, want: wantLck.toString() }),
        want: { net: '+3000000 top-up, no refund' },
      });
      const uriToA = await waitUriOwner(client, lot, bidderA.classicAddress);
      record({
        name: 'bids_self_bn_uri_on_winner',
        pass: uriToA,
        engine: uriToA ? 'ok' : 'timeout',
        gotMsg: uriToA ? 'URI owned by A' : 'URI not on A',
        want: { owner: 'A' },
      });
    }
  }

  /* Bidder with asfDisallowIncomingRemit rejected */
  {
    const lot = await mintUT(client, seller);
    const cr = await createRemit(client, seller, host, lot, {
      DUR: u64be(DUR_S),
      SP: u64be(1_000_000),
    });
    const aid = cr.engine === 'tesSUCCESS' ? aidFrom(cr.hash, lot) : null;
    record({
      name: 'setup_create_xah_bidder_remit_gate',
      pass: !!aid,
      engine: cr.engine,
      hash: cr.hash,
      gotMsg: '',
      want: { engine: 'tesSUCCESS' },
    });
    if (aid) {
      const set = await softSubmit(submitAndWait(client, bidderC, {
        TransactionType: 'AccountSet',
        Account: bidderC.classicAddress,
        SetFlag: ASF_DISALLOW_INCOMING_REMIT,
      }));
      record(expectCase('setup_bidderC_set_disallow_remit', set, { engine: 'tesSUCCESS' }));
      if (set.engine === 'tesSUCCESS') {
        const r = await bidPay(client, bidderC, host, '1000000', aid);
        record(expectCase('bids_bidder_remits_disabled_reject', r, {
          engine: 'tecHOOK_REJECTED',
          msgIncludes: 'bidder remits disabled',
          bidsOnly: true,
        }));
        const clr = await softSubmit(submitAndWait(client, bidderC, {
          TransactionType: 'AccountSet',
          Account: bidderC.classicAddress,
          ClearFlag: ASF_DISALLOW_INCOMING_REMIT,
        }));
        record(expectCase('setup_bidderC_clear_disallow_remit', clr, { engine: 'tesSUCCESS' }));
      } else {
        record({
          name: 'bids_bidder_remits_disabled_reject',
          pass: false,
          engine: 'skipped',
          gotMsg: 'AccountSet SetFlag 16 failed: ' + set.engine,
          want: { engine: 'reject', msg: 'bidder remits disabled' },
        });
      }
    }
  }


  /* Bidder with DepositAuth rejected at entry */
  {
    const lot = await mintUT(client, seller);
    const cr = await createRemit(client, seller, host, lot, {
      DUR: u64be(DUR_S),
      SP: u64be(1_000_000),
    });
    const aid = cr.engine === 'tesSUCCESS' ? aidFrom(cr.hash, lot) : null;
    record({
      name: 'setup_create_xah_bidder_depositauth_gate',
      pass: !!aid,
      engine: cr.engine,
      hash: cr.hash,
      gotMsg: '',
      want: { engine: 'tesSUCCESS' },
    });
    if (aid) {
      const set = await softSubmit(submitAndWait(client, bidderC, {
        TransactionType: 'AccountSet',
        Account: bidderC.classicAddress,
        SetFlag: ASF_DEPOSIT_AUTH,
      }));
      record(expectCase('setup_bidderC_set_depositauth', set, { engine: 'tesSUCCESS' }));
      if (set.engine === 'tesSUCCESS') {
        const r = await bidPay(client, bidderC, host, '1000000', aid);
        record(expectCase('bids_bidder_depositauth_reject', r, {
          engine: 'tecHOOK_REJECTED',
          msgIncludes: 'bidder DepositAuth',
          bidsOnly: true,
        }));
        const clr = await softSubmit(submitAndWait(client, bidderC, {
          TransactionType: 'AccountSet',
          Account: bidderC.classicAddress,
          ClearFlag: ASF_DEPOSIT_AUTH,
        }));
        record(expectCase('setup_bidderC_clear_depositauth', clr, { engine: 'tesSUCCESS' }));
      }
    }
  }

  /* require-DT: reject without tag; accept with tag; WDT stored */
  {
    const lot = await mintUT(client, seller);
    const cr = await createRemit(client, seller, host, lot, {
      DUR: u64be(DUR_S),
      SP: u64be(1_000_000),
      MB: u64be(100_000),
    });
    const aid = cr.engine === 'tesSUCCESS' ? aidFrom(cr.hash, lot) : null;
    record({
      name: 'setup_create_xah_require_dt',
      pass: !!aid,
      engine: cr.engine,
      hash: cr.hash,
      gotMsg: '',
      want: { engine: 'tesSUCCESS' },
    });
    if (aid) {
      const set = await softSubmit(submitAndWait(client, bidderA, {
        TransactionType: 'AccountSet',
        Account: bidderA.classicAddress,
        SetFlag: ASF_REQUIRE_DEST_TAG,
      }));
      record(expectCase('setup_bidderA_set_require_dt', set, { engine: 'tesSUCCESS' }));
      if (set.engine === 'tesSUCCESS') {
        const r0 = await bidPay(client, bidderA, host, '1000000', aid);
        record(expectCase('bids_require_dt_missing_reject', r0, {
          engine: 'tecHOOK_REJECTED',
          msgIncludes: 'DestinationTag required',
          bidsOnly: true,
        }));
        const r1 = await bidPay(client, bidderA, host, '1000000', aid, {
          DestinationTag: 4242,
        });
        record(expectCase('bids_require_dt_with_tag_ok', r1, {
          engine: 'tesSUCCESS',
          msg: 'Max bid accepted',
          bidsOnly: true,
        }));
        const keys = await readAidKeys(client, host.classicAddress, aid);
        const wdtOk = keys.WDT === u32be(4242);
        record({
          name: 'bids_wdt_stored',
          pass: wdtOk,
          engine: 'ok',
          gotMsg: JSON.stringify({ WDT: keys.WDT || null }),
          want: { WDT: u32be(4242) },
        });
        /* Outbid by B - refund should carry DestinationTag 4242 */
        const balA0 = await bal(client, bidderA.classicAddress);
        const r2 = await bidPay(client, bidderB, host, '1200000', aid);
        record(expectCase('bids_require_dt_outbid_ok', r2, {
          engine: 'tesSUCCESS',
          msgIncludes: 'prior refund',
          bidsOnly: true,
          emitMin: 1,
        }));
        /* wait for refund to land */
        let landed = false;
        for (let i = 0; i < 30; i++) {
          await sleep(2000);
          const balA1 = await bal(client, bidderA.classicAddress);
          if (balA1 > balA0 + 500000n) { landed = true; break; }
        }
        record({
          name: 'bids_require_dt_refund_landed',
          pass: landed,
          engine: landed ? 'ok' : 'timeout',
          gotMsg: landed ? 'refund landed' : 'refund not seen',
          want: { landed: true },
        });
        const clr = await softSubmit(submitAndWait(client, bidderA, {
          TransactionType: 'AccountSet',
          Account: bidderA.classicAddress,
          ClearFlag: ASF_REQUIRE_DEST_TAG,
        }));
        record(expectCase('setup_bidderA_clear_require_dt', clr, { engine: 'tesSUCCESS' }));
      }
    }
  }

  /* Remit passthrough on Bids */
  {
    const lot = await mintUT(client, seller);
    const r = await createRemit(client, seller, host, lot, {
      DUR: u64be(DUR_S),
      SP: u64be(1_000_000),
    });
    const bMsgs = bidsMsgs(decodeHr(r.meta));
    record({
      name: 'bids_remit_passthrough_on_create',
      pass: r.engine === 'tesSUCCESS' && bMsgs.some((m) => m.includes('Passthrough')),
      engine: r.engine,
      hash: r.hash,
      gotMsg: bMsgs.join('|'),
      want: { bidsMsg: 'Passthrough', createOk: true },
    });
  }

  /* ===== IOU bids ===== */
  {
    const a = auctions.iou_full;
    if (a) {
      const rLow = await bidPay(client, bidderA, host, {
        currency: 'AUC',
        issuer: issuer.classicAddress,
        value: '5',
      }, a.aid);
      record(expectCase('bids_iou_below_sp_reject', rLow, {
        engine: 'tecHOOK_REJECTED',
        msgIncludes: 'below start price',
        bidsOnly: true,
      }));

      const r1 = await bidPay(client, bidderA, host, {
        currency: 'AUC',
        issuer: issuer.classicAddress,
        value: '10',
      }, a.aid);
      record(expectCase('bids_iou_first_at_sp_ok', r1, {
        engine: 'tesSUCCESS',
        msg: 'Max bid accepted',
        bidsOnly: true,
        emitMax: 0,
      }));
      const keys1 = await readAidKeys(client, host.classicAddress, a.aid);
      record({
        name: 'bids_iou_first_state',
        pass: keys1.WIN === accHex(bidderA.classicAddress)
          && keys1.HIGH === xflHex(10)
          && keys1.BCNT === u32be(1),
        engine: 'ok',
        gotMsg: JSON.stringify({ HIGH: keys1.HIGH, WIN: keys1.WIN, BCNT: keys1.BCNT, wantHigh: xflHex(10) }),
        want: { HIGH: xflHex(10), WIN: 'A', BCNT: 1 },
      });

      const rBelowMb = await bidPay(client, bidderB, host, {
        currency: 'AUC',
        issuer: issuer.classicAddress,
        value: '10.5',
      }, a.aid);
      record(expectCase('bids_iou_below_mb_reject', rBelowMb, {
        engine: 'tecHOOK_REJECTED',
        msgIncludes: 'below min increment',
        bidsOnly: true,
      }));

      const r2 = await bidPay(client, bidderB, host, {
        currency: 'AUC',
        issuer: issuer.classicAddress,
        value: '11',
      }, a.aid);
      record(expectCase('bids_iou_outbid_refund', r2, {
        engine: 'tesSUCCESS',
        msgIncludes: 'prior refund',
        bidsOnly: true,
        emitMin: 1,
      }));
      const keys2 = await readAidKeys(client, host.classicAddress, a.aid);
      record({
        name: 'bids_iou_outbid_state',
        pass: keys2.WIN === accHex(bidderB.classicAddress)
          && keys2.HIGH === xflHex(11)
          && keys2.BCNT === u32be(2),
        engine: 'ok',
        gotMsg: JSON.stringify({ HIGH: keys2.HIGH, WIN: keys2.WIN, BCNT: keys2.BCNT }),
        want: { HIGH: xflHex(11), WIN: 'B', BCNT: 2 },
      });

      /* Wait for outbid Remit refund cbak (PEN clear, no RFD). Same AID for buy-now. */
      {
        for (let i = 0; i < 40; i++) {
          await sleep(2000);
          const k = await readAidKeys(client, host.classicAddress, a.aid);
          if (!k.PEN && !k.IFR && !k.RFD) break;
          if (!k.PEN && !k.IFR && k.RFD) break;
        }
        const k = await readAidKeys(client, host.classicAddress, a.aid);
        record({
          name: 'bids_iou_outbid_refund_settled',
          pass: !k.PEN && !k.IFR && !k.RFD,
          engine: 'ok',
          gotMsg: JSON.stringify({ PEN: k.PEN || null, RFD: k.RFD || null }),
          want: { PEN: null, RFD: null },
        });
      }

      const rBn = await bidPay(client, bidderC, host, {
        currency: 'AUC',
        issuer: issuer.classicAddress,
        value: '100',
      }, a.aid);
      record(expectCase('bids_iou_buynow_ok', rBn, {
        engine: 'tesSUCCESS',
        msgIncludes: 'Buy-now accepted',
        bidsOnly: true,
        emitMin: 2,
      }));
      const uriToC = await waitUriOwner(client, a.lot, bidderC.classicAddress);
      record({
        name: 'bids_iou_buynow_uri_on_winner',
        pass: uriToC,
        engine: uriToC ? 'ok' : 'timeout',
        gotMsg: uriToC ? 'URI owned by C' : 'URI not on C',
        want: { owner: 'C' },
      });
      const keys3 = await readAidKeys(client, host.classicAddress, a.aid);
      record({
        name: 'bids_iou_buynow_state',
        pass: keys3.BNW === '01'
          && keys3.WIN === accHex(bidderC.classicAddress)
          && keys3.ST === '02'
          && !keys3.URI,
        engine: 'ok',
        gotMsg: JSON.stringify({ BNW: keys3.BNW, WIN: keys3.WIN, HIGH: keys3.HIGH, ST: keys3.ST, URI: keys3.URI }),
        want: { BNW: 1, ST: 2, WIN: 'C', URI: 'cleared' },
      });
    }
  }

  /* Wrong IOU currency */
  {
    const a = auctions.iou_full;
    if (a) {
      // auction already BNW-locked - expect buy-now already won (still a reject)
      const r = await bidPay(client, bidderA, host, {
        currency: 'USD',
        issuer: issuer.classicAddress,
        value: '10',
      }, a.aid);
      record(expectCase('bids_iou_locked_or_currency_reject', r, {
        engine: 'tecHOOK_REJECTED',
        msgAnyOf: ['auction not open', 'buy-now already won', 'payment currency mismatch', 'IOU auction requires IOU payment'],
        bidsOnly: true,
      }));
    }
  }

  /* Fresh IOU for currency mismatch */
  {
    const lot = await mintUT(client, seller);
    const cr = await createRemit(client, seller, host, lot, {
      DUR: u64be(DUR_S),
      SP: xflHex(5),
      CUR: curIso('AUC'),
      ISS: accHex(issuer.classicAddress),
    });
    const aid = cr.engine === 'tesSUCCESS' ? aidFrom(cr.hash, lot) : null;
    record({
      name: 'setup_create_iou_cur_check',
      pass: !!aid,
      engine: cr.engine,
      hash: cr.hash,
      gotMsg: '',
      want: { engine: 'tesSUCCESS' },
    });
    if (aid) {
      // issuer must also issue USD to bidder - or just attempt and expect path/hook reject
      const r = await bidPay(client, bidderA, host, {
        currency: 'USD',
        issuer: issuer.classicAddress,
        value: '10',
      }, aid);
      // May fail engine-side (no line) or hook-side
      const hrs = decodeHr(r.meta);
      const bMsgs = bidsMsgs(hrs);
      const pass = r.engine !== 'tesSUCCESS';
      record({
        name: 'bids_iou_wrong_currency_reject',
        pass,
        engine: r.engine,
        hash: r.hash,
        gotMsg: bMsgs.join('|') || r.engine,
        want: { reject: true },
        bidsMsgs: bMsgs,
      });

      const rOk = await bidPay(client, bidderA, host, {
        currency: 'AUC',
        issuer: issuer.classicAddress,
        value: '5',
      }, aid);
      record(expectCase('bids_iou_fresh_first_ok', rOk, {
        engine: 'tesSUCCESS',
        msg: 'Max bid accepted',
        bidsOnly: true,
      }));
    }
  }

  /* Zero / dust edge: amount 0 may fail before hook */
  {
    const a = auctions.xah_no_sp;
    if (a) {
      // already has a bid; try zero on a fresh auction
    }
    const lot = await mintUT(client, seller);
    const cr = await createRemit(client, seller, host, lot, { DUR: u64be(DUR_S) });
    const aid = cr.engine === 'tesSUCCESS' ? aidFrom(cr.hash, lot) : null;
    record({
      name: 'setup_create_xah_zero_probe',
      pass: !!aid,
      engine: cr.engine,
      hash: cr.hash,
      gotMsg: '',
      want: { engine: 'tesSUCCESS' },
    });
    if (aid) {
      const r = await bidPay(client, bidderA, host, '0', aid);
      record({
        name: 'bids_xah_zero_amount_reject',
        pass: r.engine !== 'tesSUCCESS',
        engine: r.engine,
        hash: r.hash,
        gotMsg: bidsMsgs(decodeHr(r.meta)).join('|') || r.engine,
        want: { reject: true },
      });
    }
  }

  /* AID present but payment not to host - use other as dest (Bids not installed there -> no bids msg; just ensure no crash on host-less) */
  {
    const a = auctions.xah_full;
    if (a) {
      const r = await softSubmit(submitAndWait(client, bidderA, {
        TransactionType: 'Payment',
        Account: bidderA.classicAddress,
        Destination: other.classicAddress,
        Amount: '1000000',
        HookParameters: [hp('AID', a.aid)],
      }));
      record({
        name: 'bids_aid_to_non_host_no_bids_hook',
        pass: r.engine === 'tesSUCCESS',
        engine: r.engine,
        hash: r.hash,
        gotMsg: 'payment to non-host ok',
        want: { engine: 'tesSUCCESS' },
      });
    }
  }

  /* Partial payment flag reject (if engine allows setting it) */
  {
    const lot = await mintUT(client, seller);
    const cr = await createRemit(client, seller, host, lot, {
      DUR: u64be(DUR_S),
      SP: u64be(1_000_000),
    });
    const aid = cr.engine === 'tesSUCCESS' ? aidFrom(cr.hash, lot) : null;
    record({
      name: 'setup_create_partial_probe',
      pass: !!aid,
      engine: cr.engine,
      hash: cr.hash,
      gotMsg: '',
      want: { engine: 'tesSUCCESS' },
    });
    if (aid) {
      const r = await softSubmit(submitAndWait(client, bidderA, {
        TransactionType: 'Payment',
        Account: bidderA.classicAddress,
        Destination: host.classicAddress,
        Amount: '2000000',
        Flags: 0x00020000,
        HookParameters: [hp('AID', aid)],
      }));
      const bMsgs = bidsMsgs(decodeHr(r.meta));
      record({
        name: 'bids_partial_payment_reject',
        pass: r.engine !== 'tesSUCCESS'
          && (bMsgs.some((m) => m.includes('partial'))
            || /temBAD_SEND_NATIVE_PARTIAL|tecHOOK_REJECTED|partial/i.test(String(r.engine + bMsgs.join('|')))),
        engine: r.engine,
        hash: r.hash,
        gotMsg: bMsgs.join('|') || r.engine,
        want: { reject: true },
      });
    }
  }

  /* Below MB on mb_only after seat */
  {
    const a = auctions.xah_mb_only;
    if (a) {
      // may already have seat from self_bn_prep
      const keys = await readAidKeys(client, host.classicAddress, a.aid);
      if (!keys.WIN) {
        const r0 = await bidPay(client, bidderA, host, '500000', a.aid);
        record(expectCase('bids_mb_only_seat', r0, {
          engine: 'tesSUCCESS',
          msg: 'Max bid accepted',
          bidsOnly: true,
        }));
      }
      const r = await bidPay(client, bidderB, host, '520000', a.aid); // need >= 550000
      record(expectCase('bids_mb_only_below_reject', r, {
        engine: 'tecHOOK_REJECTED',
        msgIncludes: 'below min increment',
        bidsOnly: true,
      }));
      const r2 = await bidPay(client, bidderB, host, '550000', a.aid);
      record(expectCase('bids_mb_only_exact_ok', r2, {
        engine: 'tesSUCCESS',
        msgIncludes: 'prior refund',
        bidsOnly: true,
        emitMin: 1,
      }));
    }
  }


  /* ---- PW-C01: Bids NOPE while TSF ---- */
  {
    const ghost = genWallet();
    const lot = await mintUT(client, seller);
    const cr = await createRemit(client, seller, host, lot, {
      DUR: u64be(DUR_S),
      CUR: curIso('TSF'),
      ISS: accHex(ghost.classicAddress),
    });
    const aid = cr.engine === 'tesSUCCESS' ? aidFrom(cr.hash, lot) : null;
    const bhrs = decodeHr(cr.meta);
    const bmsgs = bhrs.map((h) => h.msg || '');
    const bemit = bhrs.reduce((n, h) => n + Number(h.emit || 0), 0);
    const brefused = cr.engine !== 'tesSUCCESS'
      && bmsgs.some((m) => m.includes('issuer AccountRoot not found'))
      && bemit === 0;
    record({
      name: 'bids_tsf_create_emit',
      pass: brefused,
      engine: cr.engine,
      hash: cr.hash,
      gotMsg: bmsgs.join('|'),
      emit: bemit,
      want: { msg: 'issuer AccountRoot not found', emit: 0 },
    });
    if (aid) {
      let tsf = false;
      let keys = {};
      const t0 = Date.now();
      while (Date.now() - t0 < 120000) {
        keys = await readAidKeys(client, host.classicAddress, aid);
        if (keys.TSF === '01') { tsf = true; break; }
        if (Object.keys(keys).length === 0) break;
        await sleep(2000);
      }
      if (!tsf) {
        record({
          name: 'bids_while_tsf_nope',
          pass: true,
          engine: 'ok',
          gotMsg: 'TSF cleared by Create Remit-back before bid (acceptable)',
          want: { note: 'Create auto-reclaim raced ahead' },
        });
      } else {
        const r = await bidPay(client, bidderA, host, '1000000', aid);
        record(expectCase('bids_while_tsf_nope', r, {
          engine: 'reject',
          msgAnyOf: ['create TrustSet failed', 'auction not found'],
          bidsOnly: true,
        }));
      }
    }
  }

  /* ---- PW-H01: Remit Amounts over LCK NOPE (shared ns LCK from bids) ---- */
  {
    const g = await readHostLocalKeys(client, host.classicAddress, NS_BIDS, ['LCK']);
    const lck = g.LCK ? Buffer.from(g.LCK, 'hex').readBigUInt64BE(0) : 0n;
    const hostBal = await bal(client, host.classicAddress);
    const over = hostBal > 3_000_000n ? (hostBal - 500_000n) : (lck + 3_000_000n);
    const rOver = await softSubmit(submitAndWait(client, host, {
      TransactionType: 'Remit',
      Account: host.classicAddress,
      Destination: other.classicAddress,
      Amounts: [{ AmountEntry: { Amount: String(over) } }],
    }));
    record(expectCase('host_remit_amounts_xah_over_lck_nope', rOver, {
      engine: 'reject',
      msgIncludes: 'Insufficient spendable float',
      anyHook: true,
    }));
    const under = 50_000n;
    const rUnder = await softSubmit(submitAndWait(client, host, {
      TransactionType: 'Remit',
      Account: host.classicAddress,
      Destination: other.classicAddress,
      Amounts: [{ AmountEntry: { Amount: String(under) } }],
    }));
    record(expectCase('host_remit_amounts_xah_under_lck_ok', rUnder, {
      engine: 'tesSUCCESS',
      msgAnyOf: ['Outgoing ok', 'Passthrough'],
      anyHook: true,
    }));
  }

  /* ---- KVT #13: host gen-0 Remit carries at most 3 Amounts ----
   * Create sits ahead of Bids in the chain, so it refuses 4 Amounts first.
   * The 3-Amount case proves the Bids nested GUARD passes 2 and 3 IOU entries. */
  {
    const hIou = (code) => ({ currency: code, issuer: host.classicAddress, value: '1' });
    const remitN = (amounts) => softSubmit(submitAndWait(client, host, {
      TransactionType: 'Remit',
      Account: host.classicAddress,
      Destination: other.classicAddress,
      Amounts: amounts.map((a) => ({ AmountEntry: { Amount: a } })),
    }));
    record(expectCase('host_remit_4_amounts_nope', await remitN(['1000', hIou('AAA'), hIou('BBB'), hIou('CCC')]), {
      engine: 'reject',
      msgIncludes: 'too many Remit amounts',
      anyHook: true,
    }));
    record(expectCase('host_remit_3_amounts_2iou_bids_ok', await remitN(['1000', hIou('AAA'), hIou('BBB')]), {
      engine: 'tesSUCCESS',
      msg: 'Outgoing ok',
      bidsOnly: true,
    }));
    record(expectCase('host_remit_3_iou_bids_ok', await remitN([hIou('AAA'), hIou('BBB'), hIou('CCC')]), {
      engine: 'tesSUCCESS',
      msg: 'Outgoing ok',
      bidsOnly: true,
    }));
  }

  /* KVT #12: `IOU amount invalid` needs a bad stored IOU LCK float. A normal
   * Payment cannot write one (pay_xfl is checked > 0 and LCK only grows by it),
   * so this matrix does not forge it. Proved on testnet with a test-only state
   * seed hook (not shipped). */

  /* ---- PW-H03: buy-now URI fail -> BNW without ST=2 (Finalise retry: seller, ADMIN, or WIN) ----
   * Force URI Remit fail by flipping DisallowIncomingRemit immediately after
   * buy-now submit (best-effort race vs emit apply). */
  {
    const lot = await mintUT(client, seller);
    const cr = await createRemit(client, seller, host, lot, {
      DUR: u64be(DUR_S),
      SP: u64be(1_000_000),
      BN: u64be(5_000_000),
    });
    const aid = cr.engine === 'tesSUCCESS' ? aidFrom(cr.hash, lot) : null;
    record({
      name: 'bids_bn_uri_fail_create',
      pass: !!aid,
      engine: cr.engine,
      hash: cr.hash,
      gotMsg: '',
      want: { engine: 'tesSUCCESS' },
    });
    if (aid) {
      const bidP = softSubmit(submitAndWait(client, bidderB, {
        TransactionType: 'Payment',
        Account: bidderB.classicAddress,
        Destination: host.classicAddress,
        Amount: '5000000',
        HookParameters: [hp('AID', aid)],
      }));
      /* Race: flip Disallow as soon as possible */
      await sleep(50);
      await softSubmit(submitAndWait(client, bidderB, {
        TransactionType: 'AccountSet',
        Account: bidderB.classicAddress,
        SetFlag: ASF_DISALLOW_INCOMING_REMIT,
      })).catch(() => null);
      const br = await bidP;
      record({
        name: 'bids_bn_uri_fail_bid',
        pass: br.engine === 'tesSUCCESS' || br.engine === 'tecHOOK_REJECTED',
        engine: br.engine,
        hash: br.hash,
        gotMsg: decodeHr(br.meta).map((h) => h.msg).join('|'),
        want: { note: 'accept or entry reject' },
      });
      let keys = {};
      const t0 = Date.now();
      let stranded = false;
      while (Date.now() - t0 < 90000) {
        keys = await readAidKeys(client, host.classicAddress, aid);
        if (keys.SSF === '01' && keys.BNW === '01' && keys.ST === '01') {
          stranded = true;
          break;
        }
        if (keys.ST === '02' && keys.BNW === '01') break; /* happy path won race */
        await sleep(2000);
      }
      record({
        name: 'fin_bn_uri_fail_bnw_without_st2',
        pass: stranded || (keys.ST === '02' && keys.BNW === '01'),
        engine: stranded ? 'stranded' : (keys.ST === '02' ? 'race_won_uri_ok' : 'timeout'),
        gotMsg: JSON.stringify({ ST: keys.ST, BNW: keys.BNW, SSF: keys.SSF, URI: !!keys.URI }),
        want: { note: 'strand ST=1+BNW+SSF OR happy ST=2+BNW if race lost' },
      });
      if (stranded) {
        /* Bids IT has no Finalise. Strand flags only here. Retry auth
         * (seller, ADMIN, or WIN, KVT #15) is covered in IT_FINALISE. */
        record({
          name: 'fin_bn_uri_fail_seller_only_flags',
          pass: keys.BNW === '01' && keys.ST === '01' && keys.SSF === '01',
          engine: 'ok',
          gotMsg: JSON.stringify(keys),
          want: { BNW: 1, ST: 1, SSF: 1 },
        });
      } else {
        record({
          name: 'fin_bn_uri_fail_seller_only_flags',
          pass: true,
          engine: 'ok',
          gotMsg: 'URI race won (happy BN) - strand path covered by code+Finalise timed SSF',
          want: { soft: true },
        });
      }
      /* cleanup Disallow */
      await softSubmit(submitAndWait(client, bidderB, {
        TransactionType: 'AccountSet',
        Account: bidderB.classicAddress,
        ClearFlag: ASF_DISALLOW_INCOMING_REMIT,
      })).catch(() => null);
    }
  }

  /* =====================================================================
   * MAX BID (Bids self-raise). p2_* cases.
   * Own fresh host: pinned Sub + Create, new Bids, max bid Finalise, all in
   * one shared namespace (Finalise needs the shared LCK), plus the test-only
   * seed hook (SmokeStateSeed.wasm via SEED_WASM op 0x09) used only to
   * delete or forge PRC so the "no PRC", "stale PRC" and "valid PRC below
   * HIGH" self-raise paths can be checked. Seeded cases are skipped without it.
   * ===================================================================== */
  {
    const ISO = 'AUC';
    const curFull = Buffer.alloc(20);
    Buffer.from(ISO, 'ascii').copy(curFull, 12);
    const CUR20 = curFull.toString('hex').toUpperCase();
    const ISS20 = accHex(issuer.classicAddress);
    const IOU_LCK_KEY = iouLckKeyHex(CUR20, ISS20);
    const auc = (v) => ({ currency: ISO, issuer: issuer.classicAddress, value: String(v) });
    const asciiHex = (s) => Buffer.from(s, 'ascii').toString('hex').toUpperCase();
    const keyHex = (name) => asciiHex(name).padStart(64, '0');
    const NS_P2 = crypto.createHash('sha256').update('AuctionHouseV2-Bids-p2-' + Date.now()).digest().toString('hex').toUpperCase();
    const FEE_BPS = 500;
    function xflToNum(hex) {
      if (!hex || /^0+$/.test(hex)) return 0;
      const v = BigInt('0x' + hex);
      const mant = v & ((1n << 54n) - 1n);
      const exp = Number((v >> 54n) & 0xFFn) - 97;
      const neg = ((v >> 62n) & 1n) === 0n;
      const n = Number(mant) * 10 ** exp;
      return neg ? -n : n;
    }
    const prcIs = (hex, price, high) => !!hex && hex.length === 32 && near(xflToNum(hex.slice(0, 16)), price, 1e-12) && near(xflToNum(hex.slice(16)), high, 1e-12);
    const xflIs = (hex, v) => !!hex && hex.length === 16 && near(xflToNum(hex), v, 1e-12);
    const near = (a, b, tol = 1e-9) => Math.abs(Number(a) - Number(b)) <= tol * Math.max(1, Math.abs(Number(b)));
    async function nsEntries(hostAddr, nsId) {
      const r = await client.request({
        command: 'account_namespace', account: hostAddr, namespace_id: nsId, ledger_index: 'validated',
      }).catch(() => null);
      return r?.result?.namespace_entries || [];
    }
    const P2_KEYS = ['ST', 'HIGH', 'WIN', 'BCNT', 'BNW', 'WDT', 'PEN', 'SPEN', 'SSF', 'LCKU', 'PRC', 'WPAY', 'IFR', 'RFD', 'URI'];
    async function readK(hostAddr, aid) {
      const by = {};
      for (const o of await nsEntries(hostAddr, aid)) by[String(o.HookStateKey || '').toUpperCase()] = String(o.HookStateData || '').toUpperCase();
      const f = {};
      for (const n of P2_KEYS) if (by[keyHex(n)] != null) f[n] = by[keyHex(n)];
      f._count = Object.keys(by).length;
      return f;
    }
    async function lckX(hostAddr) {
      for (const o of await nsEntries(hostAddr, NS_P2)) {
        if (String(o.HookStateKey || '').toUpperCase() === keyHex('LCK')) return BigInt('0x' + o.HookStateData);
      }
      return 0n;
    }
    async function lckI(hostAddr) {
      for (const o of await nsEntries(hostAddr, NS_P2)) {
        if (String(o.HookStateKey || '').toUpperCase() === IOU_LCK_KEY) return xflToNum(String(o.HookStateData));
      }
      return 0;
    }
    async function waitPred(fn, maxMs = 90000, stepMs = 2000) {
      const t0 = Date.now();
      let v = { ok: false };
      while (Date.now() - t0 < maxMs) {
        v = await fn();
        if (v.ok) return v;
        await sleep(stepMs);
      }
      return v;
    }
    async function iouBal(addr) {
      const r = await client.request({ command: 'account_lines', account: addr, peer: issuer.classicAddress, ledger_index: 'validated' }).catch(() => null);
      for (const l of r?.result?.lines || []) if (l.currency === ISO) return Number(l.balance || 0);
      return 0;
    }
    async function txFee(hash) {
      const r = await client.request({ command: 'tx', transaction: hash }).catch(() => null);
      return BigInt(r?.result?.Fee || 0);
    }
    const RES_INC = await (async () => {
      const r = await client.request({ command: 'server_state' }).catch(() => null);
      const v = r?.result?.state?.validated_ledger?.reserve_inc;
      return v != null ? BigInt(v) : 200000n;
    })();
    const emitsOf = (r) => Number((bidsHr(decodeHr(r.meta)) || {}).emit || 0);
    const finHash = crypto.createHash('sha512').update(WASM_FIN).digest().slice(0, 32).toString('hex').toUpperCase();
    const finHr = (r) => decodeHr(r.meta).find((h) => String(h.hash || '').toUpperCase() === finHash) || {};
    const finMsg = (r) => String(finHr(r).msg || '');
    async function invokeFin(signer, h, aid) {
      return softSubmit(submitAndWait(client, signer, {
        TransactionType: 'Invoke', Account: signer.classicAddress, Destination: h.classicAddress, HookParameters: [hp('AID', aid)],
      }));
    }
    function rec(name, pass, gotMsg, want, extra = {}) {
      record({ name, pass: !!pass, engine: 'ok', gotMsg: typeof gotMsg === 'string' ? gotMsg : JSON.stringify(gotMsg), want, ...extra });
    }

    const p2Host = genWallet();
    const p2A = genWallet();
    const p2B = genWallet();
    const p2C = genWallet();
    OUT.accounts.p2 = { host: p2Host.classicAddress, A: p2A.classicAddress, B: p2B.classicAddress, C: p2C.classicAddress, namespace: NS_P2 };
    for (const [w, d] of [[p2Host, 400_000_000n], [p2A, 200_000_000n], [p2B, 100_000_000n], [p2C, 25_000_000n]]) {
      await ensureBank(d + 40_000_000n);
      await pay(client, bank, w, d);
    }
    const slot = (wasm, on, params) => ({
      Hook: {
        CreateCode: wasm.toString('hex').toUpperCase(), Flags: HSF_OVERRIDE, HookApiVersion: 0,
        HookNamespace: NS_P2, HookOn: on, ...(params ? { HookParameters: params } : {}),
      },
    });
    const ADMINP = [hp('ADMIN', accHex(admin.classicAddress))];
    const hooks = [
      slot(WASM_SUB, HOOK_ON_PAYMENT_INVOKE, ADMINP),
      slot(WASM_CREATE, HOOK_ON_CREATE, null),
      slot(WASM_BIDS, HOOK_ON_BIDS, ADMINP),
      slot(WASM_FIN, HOOK_ON_INVOKE, ADMINP),
    ];
    if (WASM_SEED) hooks.push(slot(WASM_SEED, HOOK_ON_INVOKE, null));
    {
      const r = await softSubmit(submitAndWait(client, p2Host, { TransactionType: 'SetHook', Account: p2Host.classicAddress, Hooks: hooks }));
      record(expectCase('p2_setup_sethook', r, { engine: 'tesSUCCESS', anyHook: true }));
    }
    for (const [name, hex] of [
      ['SUBPRICE', u64be(PRICE)], ['SUBPERIOD', u32be(PERIOD)], ['SUBSPLIT', u16be(SPLIT_PCT)],
      ['AUCCAP', u16be(40) /* p2 + p3 auctions on one host */], ['TREASURY', accHex(treasury.classicAddress)], ['FEE', u16be(FEE_BPS)],
    ]) {
      record(expectCase('p2_setup_admin_' + name, await invokeAdmin(client, admin, p2Host, name, hex), { engine: 'tesSUCCESS', anyHook: true }));
    }
    record(expectCase('p2_setup_sub_seller', await subPay(client, seller, p2Host, PRICE), { engine: 'tesSUCCESS', msgIncludes: 'Subscription', anyHook: true }));
    for (const [label, w] of [['A', p2A], ['B', p2B], ['seller', seller], ['treasury', treasury]]) {
      const ts = await softSubmit(submitAndWait(client, w, {
        TransactionType: 'TrustSet', Account: w.classicAddress, LimitAmount: { currency: ISO, issuer: issuer.classicAddress, value: '1000000' },
      }));
      record(expectCase('p2_setup_trust_' + label, ts, { engine: 'tesSUCCESS', anyHook: true }));
    }
    for (const [label, w] of [['A', p2A], ['B', p2B]]) {
      const ip = await softSubmit(submitAndWait(client, issuer, { TransactionType: 'Payment', Account: issuer.classicAddress, Destination: w.classicAddress, Amount: auc(1000) }));
      record(expectCase('p2_setup_issue_' + label, ip, { engine: 'tesSUCCESS', anyHook: true }));
    }
    record(expectCase('p2_setup_C_require_dt', await softSubmit(submitAndWait(client, p2C, {
      TransactionType: 'AccountSet', Account: p2C.classicAddress, SetFlag: ASF_REQUIRE_DEST_TAG,
    })), { engine: 'tesSUCCESS', anyHook: true }));

    async function seedRaw(keyH, valH, aid) {
      const params = [hp('SEED', '09'), hp('SKEY', keyH)];
      if (valH) params.push(hp('SVAL', valH));
      if (aid) params.push(hp('SAID', aid));
      const r = await softSubmit(submitAndWait(client, admin, { TransactionType: 'Invoke', Account: admin.classicAddress, Destination: p2Host.classicAddress, HookParameters: params }));
      return r.engine === 'tesSUCCESS' && anyMsgs(decodeHr(r.meta)).includes('state seeded');
    }
    async function createP2(label, params) {
      const lot = await mintUT(client, seller);
      const r = await createRemit(client, seller, p2Host, lot, params);
      const aid = r.engine === 'tesSUCCESS' ? aidFrom(r.hash, lot) : null;
      let exp = null;
      if (aid) {
        for (const o of await nsEntries(p2Host.classicAddress, aid)) {
          if (String(o.HookStateKey).toUpperCase() === keyHex('EXP')) exp = BigInt('0x' + o.HookStateData);
        }
      }
      record({ name: 'p2_setup_create_' + label, pass: !!aid, engine: r.engine, hash: r.hash, gotMsg: aid || '', want: { engine: 'tesSUCCESS' } });
      return { aid, lot, exp };
    }
    const bidOn = (w, amt, aid, extra) => bidPay(client, w, p2Host, amt, aid, extra);
    const H = p2Host.classicAddress;

    /* Timed auction first so its expiry overlaps the other cases */
    const T1 = await createP2('timed_xah', { DUR: u64be(300), SP: u64be(1_000_000) });
    const X1 = await createP2('xah_mb', { DUR: u64be(DUR_S), SP: u64be(1_000_000), MB: u64be(100_000) });
    const X2 = await createP2('xah_no_mb', { DUR: u64be(DUR_S) });
    const X3 = await createP2('xah_prc_paths', { DUR: u64be(DUR_S), SP: u64be(1_000_000) });
    const X4 = await createP2('xah_self_bn', { DUR: u64be(DUR_S), SP: u64be(1_000_000), BN: u64be(5_000_000) });
    const X5 = await createP2('xah_require_dt', { DUR: u64be(DUR_S), SP: u64be(1_000_000) });
    const I1 = await createP2('iou_mb', { DUR: u64be(DUR_S), SP: xflHex(10), MB: xflHex(1), CUR: curIso(ISO), ISS: ISS20 });
    const I2 = await createP2('iou_no_mb', { DUR: u64be(DUR_S), SP: xflHex(10), CUR: curIso(ISO), ISS: ISS20 });
    const I3 = await createP2('iou_self_bn', { DUR: u64be(DUR_S), SP: xflHex(10), BN: xflHex(100), CUR: curIso(ISO), ISS: ISS20 });
    {
      const lined = await waitHostTrustLine(client, H, issuer.classicAddress, ISO);
      rec('p2_setup_host_auc_trustline', lined, String(lined), { trustline: true });
      if (lined) {
        await softSubmit(submitAndWait(client, issuer, {
          TransactionType: 'TrustSet', Account: issuer.classicAddress, LimitAmount: { currency: ISO, issuer: H, value: '0' }, Flags: 262144,
        }));
      }
    }

    /* ---- T1: timed seat + self-raise (settled at the end) ---- */
    if (T1.aid) {
      record(expectCase('p2_timed_seat', await bidOn(p2A, '2000000', T1.aid), { engine: 'tesSUCCESS', msg: 'Max bid accepted', bidsOnly: true, emitMax: 0 }));
      const l0 = await lckX(H);
      const r = await bidOn(p2A, '1000000', T1.aid);
      record(expectCase('p2_timed_self_raise_ok', r, { engine: 'tesSUCCESS', msg: 'Max bid raised', bidsOnly: true, emitMax: 0 }));
      const k = await readK(H, T1.aid);
      const l1 = await lckX(H);
      /* the first bid is priced at SP (1M), escrow 2M is the max */
      rec('p2_timed_self_raise_state', k.HIGH === u64be(3_000_000) && k.PRC === u64be(1_000_000) + u64be(3_000_000) && (l1 - l0) === 1_000_000n,
        { HIGH: k.HIGH, PRC: k.PRC, lck: String(l1 - l0) }, { HIGH: 3000000, PRC: '1000000||3000000 (first bid at SP)', lck: '+1000000' });
    }

    /* ---- X1: XAH with MB 0.1 ---- */
    if (X1.aid) {
      const a = X1.aid;
      let l0 = await lckX(H);
      let r = await bidOn(p2A, '1000000', a);
      record(expectCase('p2_xah_first_seat', r, { engine: 'tesSUCCESS', msg: 'Max bid accepted', bidsOnly: true, emitMax: 0 }));
      let k = await readK(H, a);
      let l1 = await lckX(H);
      rec('p2_xah_first_seat_prc_p_p', k.PRC === u64be(1_000_000) + u64be(1_000_000) && k.HIGH === u64be(1_000_000) && (l1 - l0) === 1_000_000n,
        { PRC: k.PRC, HIGH: k.HIGH, lck: String(l1 - l0) }, { PRC: '1000000||1000000 (bid at SP)', lck: '+1000000' });
      r = await bidOn(p2A, '50000', a);
      record(expectCase('p2_xah_self_raise_below_inc_reject', r, { engine: 'tecHOOK_REJECTED', msgIncludes: 'raise below min increment', bidsOnly: true }));
      l0 = await lckX(H);
      const aBal0 = await bal(client, p2A.classicAddress);
      r = await bidOn(p2A, '2000000', a);
      record(expectCase('p2_xah_self_raise_ok', r, { engine: 'tesSUCCESS', msg: 'Max bid raised', bidsOnly: true, emitMax: 0 }));
      k = await readK(H, a);
      l1 = await lckX(H);
      const aBal1 = await bal(client, p2A.classicAddress);
      const f1 = await txFee(r.hash);
      rec('p2_xah_self_raise_state', k.HIGH === u64be(3_000_000) && k.PRC === u64be(1_000_000) + u64be(3_000_000)
        && k.WIN === accHex(p2A.classicAddress) && k.BCNT === u32be(2) && !k.PEN && !k.SPEN && emitsOf(r) === 0,
        { HIGH: k.HIGH, PRC: k.PRC, WIN: k.WIN, BCNT: k.BCNT, PEN: k.PEN || null, emit: emitsOf(r) },
        { HIGH: 3000000, PRC: '1000000||3000000 (price unchanged)', WIN: 'A', BCNT: 2, emit: 0 });
      rec('p2_xah_self_raise_lck_plus_extra', (l1 - l0) === 2_000_000n && (aBal0 - aBal1) === 2_000_000n + f1,
        { lck: String(l1 - l0), payerDelta: String(aBal0 - aBal1), fee: String(f1) }, { lck: '+2000000', payer: '-2000000 - fee, no refund' });
      l0 = l1;
      r = await bidOn(p2A, '100000', a);
      record(expectCase('p2_xah_second_self_raise_exact_mb', r, { engine: 'tesSUCCESS', msg: 'Max bid raised', bidsOnly: true, emitMax: 0 }));
      k = await readK(H, a);
      l1 = await lckX(H);
      rec('p2_xah_second_self_raise_state', k.HIGH === u64be(3_100_000) && k.PRC === u64be(1_000_000) + u64be(3_100_000) && (l1 - l0) === 100_000n,
        { HIGH: k.HIGH, PRC: k.PRC, lck: String(l1 - l0) }, { HIGH: 3100000, PRC: '1000000||3100000', lck: '+100000' });
      /* 3.05M is above price 1M + MB but not above the raised max
       * 3.1M: an underbid, refunded, price = min(P + MB, H) = 3.1M */
      r = await bidOn(p2B, '3050000', a);
      record(expectCase('p2_underbid_after_raise_ok', r, { engine: 'tesSUCCESS', msg: 'Bid did not exceed current max bid, payment returned', bidsOnly: true, emitMin: 1, emitMax: 1 }));
      {
        const wu = await waitPred(async () => { const kk = await readK(H, a); return { ok: !kk.IFR, kk }; }, 90000);
        const ku = wu.kk || {};
        rec('p2_underbid_after_raise_state', wu.ok && ku.WIN === accHex(p2A.classicAddress) && ku.HIGH === u64be(3_100_000) && ku.PRC === u64be(3_100_000) + u64be(3_100_000) && !ku.PEN,
          { WIN: ku.WIN, HIGH: ku.HIGH, PRC: ku.PRC, IFR: ku.IFR || null }, { WIN: 'A', HIGH: 3100000, PRC: '3100000||3100000 (capped at max)', IFR: 'drained' });
      }
      const aBal2 = await bal(client, p2A.classicAddress);
      l0 = await lckX(H);
      r = await bidOn(p2B, '3300000', a);
      record(expectCase('p2_outbid_after_raise_ok', r, { engine: 'tesSUCCESS', msg: 'Max bid accepted with prior refund', bidsOnly: true, emitMin: 1, emitMax: 1 }));
      const w = await waitPred(async () => {
        const kk = await readK(H, a);
        return { ok: !kk.PEN && !kk.IFR, kk };
      }, 90000);
      k = w.kk || await readK(H, a);
      l1 = await lckX(H);
      const aBal3 = await bal(client, p2A.classicAddress);
      rec('p2_outbid_after_raise_state', w.ok && k.WIN === accHex(p2B.classicAddress) && k.HIGH === u64be(3_300_000)
        && k.PRC === u64be(3_200_000) + u64be(3_300_000) && (aBal3 - aBal2) === 3_100_000n && (l1 - l0) === 200_000n,
        { WIN: k.WIN, HIGH: k.HIGH, PRC: k.PRC, aRefund: String(aBal3 - aBal2), lck: String(l1 - l0) },
        { WIN: 'B', HIGH: 3300000, PRC: '3200000||3300000 (H + MB)', aRefund: '3100000 (full raised max)', lck: '+3300000 -3100000' });
    }

    /* ---- X2: XAH no MB, 1 drop increment ---- */
    if (X2.aid) {
      const a = X2.aid;
      record(expectCase('p2_xah_no_mb_seat', await bidOn(p2A, '1000000', a), { engine: 'tesSUCCESS', msg: 'Max bid accepted', bidsOnly: true }));
      const l0 = await lckX(H);
      const r = await bidOn(p2A, '1', a);
      record(expectCase('p2_xah_no_mb_self_raise_1_drop_ok', r, { engine: 'tesSUCCESS', msg: 'Max bid raised', bidsOnly: true, emitMax: 0 }));
      const k = await readK(H, a);
      const l1 = await lckX(H);
      rec('p2_xah_no_mb_self_raise_state', k.HIGH === u64be(1_000_001) && k.PRC === u64be(1_000_000) + u64be(1_000_001) && (l1 - l0) === 1n,
        { HIGH: k.HIGH, PRC: k.PRC, lck: String(l1 - l0) }, { HIGH: 1000001, PRC: '1000000||1000001', lck: '+1' });
    }

    /* ---- X3: PRC paths (seeded) ---- */
    if (X3.aid) {
      const a = X3.aid;
      record(expectCase('p2_prc_seat', await bidOn(p2A, '2000000', a), { engine: 'tesSUCCESS', msg: 'Max bid accepted', bidsOnly: true }));
      if (WASM_SEED) {
        /* no PRC (legacy auction seated by the old Bids): C = old HIGH */
        let ok = await seedRaw(asciiHex('PRC'), null, a);
        let r = await bidOn(p2A, '1000000', a);
        record(expectCase('p2_prc_absent_self_raise_ok', r, { engine: 'tesSUCCESS', msg: 'Max bid raised', bidsOnly: true, emitMax: 0 }));
        let k = await readK(H, a);
        rec('p2_prc_absent_writes_old_high', ok && k.PRC === u64be(2_000_000) + u64be(3_000_000) && k.HIGH === u64be(3_000_000),
          { seeded: ok, PRC: k.PRC, HIGH: k.HIGH }, { PRC: '2000000||3000000 (price = old HIGH)' });
        /* stale PRC (snapshot != HIGH): ignored, C = HIGH */
        ok = await seedRaw(asciiHex('PRC'), u64be(500_000) + u64be(9_000_000), a);
        r = await bidOn(p2A, '1000000', a);
        record(expectCase('p2_prc_stale_self_raise_ok', r, { engine: 'tesSUCCESS', msg: 'Max bid raised', bidsOnly: true, emitMax: 0 }));
        k = await readK(H, a);
        rec('p2_prc_stale_ignored', ok && k.PRC === u64be(3_000_000) + u64be(4_000_000) && k.HIGH === u64be(4_000_000),
          { seeded: ok, PRC: k.PRC, HIGH: k.HIGH }, { PRC: '3000000||4000000 (stale price 500000 ignored)' });
        /* valid PRC with price below HIGH (what the proxy price writes): price kept */
        ok = await seedRaw(asciiHex('PRC'), u64be(1_500_000) + u64be(4_000_000), a);
        r = await bidOn(p2A, '1000000', a);
        record(expectCase('p2_prc_valid_self_raise_ok', r, { engine: 'tesSUCCESS', msg: 'Max bid raised', bidsOnly: true, emitMax: 0 }));
        k = await readK(H, a);
        rec('p2_prc_valid_price_kept', ok && k.PRC === u64be(1_500_000) + u64be(5_000_000) && k.HIGH === u64be(5_000_000),
          { seeded: ok, PRC: k.PRC, HIGH: k.HIGH }, { PRC: '1500000||5000000 (valid price kept)' });
      } else {
        rec('p2_prc_seeded_paths_skipped', true, 'no SmokeStateSeed.wasm', { note: 'seeded PRC cases need the test seed hook' }, { engine: 'skipped' });
      }
    }

    /* ---- X5: RequireDestTag self-raise ---- */
    if (X5.aid) {
      const a = X5.aid;
      record(expectCase('p2_require_dt_seat', await bidOn(p2C, '1000000', a, { DestinationTag: 77 }), { engine: 'tesSUCCESS', msg: 'Max bid accepted', bidsOnly: true }));
      record(expectCase('p2_require_dt_self_raise_missing_tag_reject', await bidOn(p2C, '500000', a), { engine: 'tecHOOK_REJECTED', msgIncludes: 'DestinationTag required', bidsOnly: true }));
      const r = await bidOn(p2C, '500000', a, { DestinationTag: 88 });
      record(expectCase('p2_require_dt_self_raise_ok', r, { engine: 'tesSUCCESS', msg: 'Max bid raised', bidsOnly: true, emitMax: 0 }));
      const k = await readK(H, a);
      rec('p2_require_dt_self_raise_wdt_updated', k.WDT === u32be(88) && k.HIGH === u64be(1_500_000) && k.PRC === u64be(1_000_000) + u64be(1_500_000),
        { WDT: k.WDT, HIGH: k.HIGH, PRC: k.PRC }, { WDT: 88, HIGH: 1500000, PRC: '1000000||1500000' });
    }

    /* ---- I1: IOU with MB 1 ---- */
    if (I1.aid) {
      const a = I1.aid;
      record(expectCase('p2_iou_seat', await bidOn(p2A, auc(10), a), { engine: 'tesSUCCESS', msg: 'Max bid accepted', bidsOnly: true }));
      let k = await readK(H, a);
      rec('p2_iou_seat_prc_p_p', prcIs(k.PRC, 10, 10), { PRC: k.PRC }, { PRC: '10||10' });
      record(expectCase('p2_iou_self_raise_below_mb_reject', await bidOn(p2A, auc(0.5), a), { engine: 'tecHOOK_REJECTED', msgIncludes: 'raise below min increment', bidsOnly: true }));
      const l0 = await lckI(H);
      const r = await bidOn(p2A, auc(5), a);
      record(expectCase('p2_iou_self_raise_ok', r, { engine: 'tesSUCCESS', msg: 'Max bid raised', bidsOnly: true, emitMax: 0 }));
      k = await readK(H, a);
      const l1 = await lckI(H);
      rec('p2_iou_self_raise_state', xflIs(k.HIGH, 15) && prcIs(k.PRC, 10, 15) && near(l1 - l0, 5) && emitsOf(r) === 0,
        { HIGH: k.HIGH, PRC: k.PRC, lck: l1 - l0, emit: emitsOf(r) }, { HIGH: 15, PRC: '10||15', iouLck: '+5', emit: 0 });
    }

    /* ---- I2: IOU without MB, strict greater for self-raise and outbid ---- */
    if (I2.aid) {
      const a = I2.aid;
      record(expectCase('p2_iou_no_mb_seat', await bidOn(p2A, auc(10), a), { engine: 'tesSUCCESS', msg: 'Max bid accepted', bidsOnly: true }));
      const l0 = await lckI(H);
      let r = await bidOn(p2A, auc('0.000001'), a);
      record(expectCase('p2_iou_no_mb_self_raise_tiny_ok', r, { engine: 'tesSUCCESS', msg: 'Max bid raised', bidsOnly: true, emitMax: 0 }));
      let k = await readK(H, a);
      const l1 = await lckI(H);
      rec('p2_iou_no_mb_self_raise_state', xflIs(k.HIGH, 10.000001) && prcIs(k.PRC, 10, 10.000001) && near(l1 - l0, 0.000001, 1e-6),
        { HIGH: k.HIGH, PRC: k.PRC, lck: l1 - l0 }, { HIGH: 10.000001, PRC: '10||10.000001' });
      /* an equal bid is above the price 10 but not above the max: a tie underbid */
      r = await bidOn(p2B, auc('10.000001'), a);
      record(expectCase('p2_iou_no_mb_equal_is_tie_underbid', r, { engine: 'tesSUCCESS', msg: 'Bid did not exceed current max bid, payment returned', bidsOnly: true, emitMin: 1 }));
      await waitPred(async () => { const kk = await readK(H, a); return { ok: !kk.IFR, kk }; }, 90000);
      r = await bidOn(p2B, auc('10.000002'), a);
      record(expectCase('p2_iou_no_mb_outbid_strict_ok', r, { engine: 'tesSUCCESS', msg: 'Max bid accepted with prior refund', bidsOnly: true, emitMin: 1 }));
      const w = await waitPred(async () => { const kk = await readK(H, a); return { ok: !kk.PEN && !kk.IFR, kk }; }, 90000);
      k = w.kk || {};
      rec('p2_iou_no_mb_outbid_state', w.ok && k.WIN === accHex(p2B.classicAddress) && prcIs(k.PRC, 10.000001, 10.000002),
        { WIN: k.WIN, PRC: k.PRC }, { WIN: 'B', PRC: '10.000001||10.000002 (no MB: price = old max)' });
    }

    /* ---- X4: self-raise reaching BN (XAH) then Finalise by WIN ---- */
    if (X4.aid) {
      const a = X4.aid;
      record(expectCase('p2_self_bn_xah_seat', await bidOn(p2A, '1000000', a), { engine: 'tesSUCCESS', msg: 'Max bid accepted', bidsOnly: true }));
      const l0 = await lckX(H);
      const r = await bidOn(p2A, '4500000', a);
      record(expectCase('p2_self_bn_xah_ok', r, { engine: 'tesSUCCESS', msg: 'Buy-now accepted', bidsOnly: true, emitMin: 1, emitMax: 1 }));
      const moved = await waitUriOwner(client, X4.lot, p2A.classicAddress);
      const w = await waitPred(async () => { const kk = await readK(H, a); return { ok: kk.ST === '02' && kk.BNW === '01' && !kk.SPEN, kk }; }, 90000);
      const k = w.kk || {};
      const l1 = await lckX(H);
      rec('p2_self_bn_xah_state', moved && w.ok && k.HIGH === u64be(5_500_000) && k.PRC === u64be(5_000_000) + u64be(5_500_000) && (l1 - l0) === 4_500_000n && !k.PEN,
        { moved, ST: k.ST, BNW: k.BNW, HIGH: k.HIGH, PRC: k.PRC, lck: String(l1 - l0) },
        { URI: 'A', ST: 2, HIGH: 5500000, PRC: '5000000||5500000', lck: '+4500000', refund: 'none' });
      const s0 = await bal(client, seller.classicAddress);
      const t0 = await bal(client, treasury.classicAddress);
      const w0 = await bal(client, p2A.classicAddress);
      const lf0 = await lckX(H);
      const f = await invokeFin(p2A, p2Host, a);
      rec('p2_self_bn_xah_finalise_ok', f.engine === 'tesSUCCESS' && finMsg(f).includes('Settlement pending') && Number(finHr(f).emit || 0) === 3,
        { engine: f.engine, msg: finMsg(f), emit: finHr(f).emit }, { msg: 'Settlement pending', emit: 3 });
      const cl = await waitPred(async () => { const kk = await readK(H, a); return { ok: kk._count === 0, kk }; }, 120000);
      await sleep(1500);
      const s1 = await bal(client, seller.classicAddress);
      const t1 = await bal(client, treasury.classicAddress);
      const w1 = await bal(client, p2A.classicAddress);
      const lf1 = await lckX(H);
      const fee = await txFee(f.hash);
      rec('p2_self_bn_xah_settle_split', cl.ok && (s1 - s0) === 4_750_000n && (t1 - t0) === 250_000n && (w1 - w0) === 500_000n - fee && (lf0 - lf1) === 5_500_000n,
        { cleared: cl.ok, seller: String(s1 - s0), treasury: String(t1 - t0), winner: String(w1 - w0), fee: String(fee), lck: String(lf0 - lf1) },
        { seller: 4750000, treasury: 250000, winner: '500000 - fee (remainder T - BN)', lck: -5500000 });
    }

    /* ---- I3: IOU self-raise reaching BN then Finalise ---- */
    if (I3.aid) {
      const a = I3.aid;
      record(expectCase('p2_self_bn_iou_seat', await bidOn(p2A, auc(10), a), { engine: 'tesSUCCESS', msg: 'Max bid accepted', bidsOnly: true }));
      const l0 = await lckI(H);
      const r = await bidOn(p2A, auc(95), a);
      record(expectCase('p2_self_bn_iou_ok', r, { engine: 'tesSUCCESS', msg: 'Buy-now accepted', bidsOnly: true, emitMin: 1, emitMax: 1 }));
      await waitUriOwner(client, I3.lot, p2A.classicAddress);
      const w = await waitPred(async () => { const kk = await readK(H, a); return { ok: kk.ST === '02' && !kk.SPEN, kk }; }, 90000);
      const k = w.kk || {};
      const l1 = await lckI(H);
      rec('p2_self_bn_iou_state', w.ok && xflIs(k.HIGH, 105) && prcIs(k.PRC, 100, 105) && near(l1 - l0, 95),
        { HIGH: k.HIGH, PRC: k.PRC, lck: l1 - l0 }, { HIGH: 105, PRC: '100||105', iouLck: '+95' });
      const s0 = await iouBal(seller.classicAddress);
      const t0 = await iouBal(treasury.classicAddress);
      const w0 = await iouBal(p2A.classicAddress);
      const lf0 = await lckI(H);
      const f = await invokeFin(p2A, p2Host, a);
      rec('p2_self_bn_iou_finalise_ok', f.engine === 'tesSUCCESS' && finMsg(f).includes('Settlement pending') && Number(finHr(f).emit || 0) === 3,
        { engine: f.engine, msg: finMsg(f), emit: finHr(f).emit }, { emit: 3 });
      const cl = await waitPred(async () => { const kk = await readK(H, a); return { ok: kk._count === 0, kk }; }, 120000);
      await sleep(1500);
      const s1 = await iouBal(seller.classicAddress);
      const t1 = await iouBal(treasury.classicAddress);
      const w1 = await iouBal(p2A.classicAddress);
      const lf1 = await lckI(H);
      rec('p2_self_bn_iou_settle_split', cl.ok && near(s1 - s0, 95) && near(t1 - t0, 5) && near(w1 - w0, 5) && near(lf0 - lf1, 105),
        { cleared: cl.ok, seller: s1 - s0, treasury: t1 - t0, winner: w1 - w0, lck: lf0 - lf1 }, { seller: 95, treasury: 5, winner: 5, iouLck: -105 });
    }

    /* ================= Max bid: proxy, IFR, buy-now ================= */
    const UB = 'Bid did not exceed current max bid, payment returned';
    const OB = 'Max bid accepted with prior refund';
    const MA = 'Max bid accepted';
    const waitIfr0 = (aid, ms = 90000) => waitPred(async () => { const kk = await readK(H, aid); return { ok: !kk.IFR, kk }; }, ms);
    const strandKey = (addr) => '52' + accHex(addr) + '00'.repeat(11);
    async function rawK(aid, keyH) {
      for (const o of await nsEntries(H, aid)) if (String(o.HookStateKey || '').toUpperCase() === keyH.toUpperCase()) return String(o.HookStateData || '').toUpperCase();
      return null;
    }
    const msgsOfR = (r) => anyMsgs(decodeHr(r.meta));
    /* Burst: sign several txs with consecutive sequences, submit them back to back
     * so they land in one ledger, then wait for each validated result. */
    async function burst(w, txs) {
      for (let i = 0; i < 60 && !client.isConnected(); i++) await sleep(1000);
      const ai = await client.request({ command: 'account_info', account: w.classicAddress, ledger_index: 'current' });
      let seq = ai.result.account_data.Sequence;
      const lc = await client.request({ command: 'ledger_current' });
      const lls = lc.result.ledger_current_index + 20;
      const signed = [];
      for (const tx of txs) {
        const p = await client.autofill({ ...tx, Account: w.classicAddress, NetworkID: NETWORK_ID, Sequence: seq++, LastLedgerSequence: lls });
        signed.push(w.sign(p));
      }
      for (const sgn of signed) {
        await client.request({ command: 'submit', tx_blob: sgn.tx_blob }).catch((e) => log('burst submit err', String(e?.message || e)));
      }
      const out = [];
      for (const sgn of signed) {
        const v = await waitPred(async () => {
          const t = await client.request({ command: 'tx', transaction: sgn.hash }).catch(() => null);
          return { ok: !!t?.result?.validated, t };
        }, 120000, 1500);
        const res = v.t?.result || {};
        out.push({ hash: sgn.hash, engine: res.meta?.TransactionResult || 'unknown', meta: res.meta, ledger: res.ledger_index });
      }
      return out;
    }
    const payTx = (amt, aid, extra = {}) => ({ TransactionType: 'Payment', Destination: H, Amount: amt, HookParameters: [hp('AID', aid)], ...extra });
    async function clrInvoke(v, aid) {
      return softSubmit(submitAndWait(client, admin, { TransactionType: 'Invoke', Account: admin.classicAddress, Destination: H, HookParameters: [hp('CLR', v), hp('AID', aid)] }));
    }

    const PA = await createP2('p3_proxy_climb', { DUR: u64be(DUR_S), SP: u64be(1_000_000), MB: u64be(100_000) });
    const PT = await createP2('p3_tie_outbid', { DUR: u64be(DUR_S), SP: u64be(1_000_000), MB: u64be(100_000) });
    const PC = await createP2('p3_outbid_capped', { DUR: u64be(DUR_S), SP: u64be(1_000_000), MB: u64be(100_000) });
    const PN = await createP2('p3_no_sp', { DUR: u64be(DUR_S) });
    const PX = await createP2('p3_xah_no_mb', { DUR: u64be(DUR_S), SP: u64be(1_000_000) });
    const PB = await createP2('p3_cap_burst', { DUR: u64be(DUR_S), SP: u64be(1_000_000), MB: u64be(100_000) });
    const PS = await createP2('p3_cap_seeded', { DUR: u64be(DUR_S), SP: u64be(1_000_000), MB: u64be(100_000) });
    const PF = await createP2('p3_float_low', { DUR: u64be(DUR_S), SP: u64be(1_000_000), MB: u64be(100_000) });
    const PD = await createP2('p3_strand', { DUR: u64be(DUR_S), SP: u64be(1_000_000), MB: u64be(100_000) });
    const PBN = await createP2('p3_buynow_overpay', { DUR: u64be(DUR_S), SP: u64be(1_000_000), MB: u64be(100_000), BN: u64be(5_000_000) });
    const I5 = await createP2('p3_iou_mb', { DUR: u64be(DUR_S), SP: xflHex(10), MB: xflHex(1), CUR: curIso(ISO), ISS: ISS20 });
    const I6 = await createP2('p3_iou_no_mb', { DUR: u64be(DUR_S), SP: xflHex(10), CUR: curIso(ISO), ISS: ISS20 });
    const I7 = await createP2('p3_iou_float_low', { DUR: u64be(DUR_S), SP: xflHex(10), MB: xflHex(1), CUR: curIso(ISO), ISS: ISS20 });
    /* Timed p3 auctions last, bid on at once (300 s window), settled at the end */
    const T2 = await createP2('p3_timed_xah', { DUR: u64be(300), SP: u64be(1_000_000), MB: u64be(100_000) });
    const I4 = await createP2('p3_timed_iou', { DUR: u64be(300), SP: xflHex(10), MB: xflHex(1), CUR: curIso(ISO), ISS: ISS20 });
    /* ---- Timed p3 auctions: build a proxy state to settle later ---- */
    if (T2.aid) {
      record(expectCase('p3_timed_seat', await bidOn(p2A, '5000000', T2.aid), { engine: 'tesSUCCESS', msg: MA, bidsOnly: true }));
      record(expectCase('p3_timed_underbid', await bidOn(p2B, '3000000', T2.aid), { engine: 'tesSUCCESS', msg: UB, bidsOnly: true }));
      const w = await waitIfr0(T2.aid);
      rec('p3_timed_proxy_state', w.ok && w.kk.PRC === u64be(3_100_000) + u64be(5_000_000), { PRC: w.kk?.PRC }, { PRC: '3100000||5000000' });
    }
    if (I4.aid) {
      record(expectCase('p3_timed_iou_seat', await bidOn(p2A, auc(50), I4.aid), { engine: 'tesSUCCESS', msg: MA, bidsOnly: true }));
      record(expectCase('p3_timed_iou_underbid', await bidOn(p2B, auc(20), I4.aid), { engine: 'tesSUCCESS', msg: UB, bidsOnly: true }));
      await waitIfr0(I4.aid);
      record(expectCase('p3_timed_iou_outbid', await bidOn(p2B, auc(60), I4.aid), { engine: 'tesSUCCESS', msg: OB, bidsOnly: true }));
      const w = await waitIfr0(I4.aid);
      rec('p3_timed_iou_proxy_state', w.ok && prcIs(w.kk.PRC, 51, 60) && w.kk.WIN === accHex(p2B.classicAddress), { PRC: w.kk?.PRC }, { PRC: '51||60' });
    }


    /* ---- Proxy price climb: SP 1, MB 0.1, A max 5 ---- */
    if (PA.aid) {
      const a = PA.aid;
      let l0 = await lckX(H);
      let r = await bidOn(p2A, '5000000', a);
      record(expectCase('p3_first_bid_priced_at_sp', r, { engine: 'tesSUCCESS', msg: MA, bidsOnly: true, emitMax: 0 }));
      let k = await readK(H, a);
      let l1 = await lckX(H);
      rec('p3_first_bid_state', k.PRC === u64be(1_000_000) + u64be(5_000_000) && k.HIGH === u64be(5_000_000) && (l1 - l0) === 5_000_000n,
        { PRC: k.PRC, HIGH: k.HIGH, lck: String(l1 - l0) }, { PRC: '1000000||5000000', HIGH: 5000000, lck: '+5000000' });
      l0 = l1;
      const b0 = await bal(client, p2B.classicAddress);
      r = await bidOn(p2B, '2000000', a);
      record(expectCase('p3_underbid_refund_accept', r, { engine: 'tesSUCCESS', msg: UB, bidsOnly: true, emitMin: 1, emitMax: 1 }));
      const kIn = await readK(H, a);
      let w = await waitIfr0(a);
      k = w.kk || {};
      l1 = await lckX(H);
      const b1 = await bal(client, p2B.classicAddress);
      const fb = await txFee(r.hash);
      rec('p3_underbid_price_raised', w.ok && k.PRC === u64be(2_100_000) + u64be(5_000_000) && k.WIN === accHex(p2A.classicAddress) && k.HIGH === u64be(5_000_000) && !k.PEN && (l1 - l0) === 0n && (b0 - b1) === fb,
        { PRC: k.PRC, WIN: k.WIN, HIGH: k.HIGH, ifrAfterBid: kIn.IFR || null, PEN: k.PEN || null, lck: String(l1 - l0), bNet: String(b0 - b1), fee: String(fb) },
        { PRC: '2100000||5000000', WIN: 'A', IFR: '1 then 0', PEN: 'never', lck: 'net 0 after cbak', B: 'only the fee' });
      r = await bidOn(p2B, '4950000', a);
      record(expectCase('p3_underbid_capped_at_max', r, { engine: 'tesSUCCESS', msg: UB, bidsOnly: true, emitMin: 1 }));
      w = await waitIfr0(a);
      k = w.kk || {};
      rec('p3_underbid_capped_state', w.ok && k.PRC === u64be(5_000_000) + u64be(5_000_000) && k.WIN === accHex(p2A.classicAddress),
        { PRC: k.PRC, WIN: k.WIN }, { PRC: '5000000||5000000 (min(P + MB, H))' });
      r = await bidOn(p2B, '5050000', a);
      record(expectCase('p3_below_price_inc_reject', r, { engine: 'tecHOOK_REJECTED', msgIncludes: 'below min increment', bidsOnly: true }));
      k = await readK(H, a);
      rec('p3_reject_leaves_state', k.PRC === u64be(5_000_000) + u64be(5_000_000) && k.HIGH === u64be(5_000_000) && k.WIN === accHex(p2A.classicAddress),
        { PRC: k.PRC, HIGH: k.HIGH, WIN: k.WIN }, { unchanged: true });
    }

    /* ---- Tie goes to the incumbent, then outbid price rule ---- */
    if (PT.aid) {
      const a = PT.aid;
      record(expectCase('p3_tie_seat', await bidOn(p2A, '5000000', a), { engine: 'tesSUCCESS', msg: MA, bidsOnly: true }));
      let r = await bidOn(p2B, '5000000', a);
      record(expectCase('p3_tie_incumbent_keeps_seat', r, { engine: 'tesSUCCESS', msg: UB, bidsOnly: true, emitMin: 1 }));
      let w = await waitIfr0(a);
      let k = w.kk || {};
      rec('p3_tie_state', w.ok && k.WIN === accHex(p2A.classicAddress) && k.PRC === u64be(5_000_000) + u64be(5_000_000),
        { WIN: k.WIN, PRC: k.PRC }, { WIN: 'A (earlier bidder)', PRC: '5000000||5000000' });
      const aBal0 = await bal(client, p2A.classicAddress);
      const l0 = await lckX(H);
      r = await bidOn(p2B, '8000000', a);
      record(expectCase('p3_outbid_refund_string', r, { engine: 'tesSUCCESS', msg: OB, bidsOnly: true, emitMin: 1, emitMax: 1 }));
      const kIn = await readK(H, a);
      w = await waitIfr0(a);
      k = w.kk || {};
      const aBal1 = await bal(client, p2A.classicAddress);
      const l1 = await lckX(H);
      rec('p3_outbid_price_rule', w.ok && k.WIN === accHex(p2B.classicAddress) && k.HIGH === u64be(8_000_000) && k.PRC === u64be(5_100_000) + u64be(8_000_000)
        && (aBal1 - aBal0) === 5_000_000n && (l1 - l0) === 3_000_000n && !kIn.PEN && !k.PEN,
        { WIN: k.WIN, HIGH: k.HIGH, PRC: k.PRC, aRefund: String(aBal1 - aBal0), lck: String(l1 - l0), ifrAfterBid: kIn.IFR || null, PEN: kIn.PEN || null },
        { WIN: 'B', PRC: '5100000||8000000', aRefund: 5000000, lck: '+8000000 -5000000', PEN: 'never (IFR instead)' });
    }

    /* ---- Outbid price capped at the challenger max ---- */
    if (PC.aid) {
      const a = PC.aid;
      record(expectCase('p3_capped_seat', await bidOn(p2A, '5000000', a), { engine: 'tesSUCCESS', msg: MA, bidsOnly: true }));
      const r = await bidOn(p2B, '5050000', a);
      record(expectCase('p3_outbid_capped_ok', r, { engine: 'tesSUCCESS', msg: OB, bidsOnly: true, emitMin: 1 }));
      const w = await waitIfr0(a);
      const k = w.kk || {};
      rec('p3_outbid_price_capped', w.ok && k.WIN === accHex(p2B.classicAddress) && k.PRC === u64be(5_050_000) + u64be(5_050_000),
        { WIN: k.WIN, PRC: k.PRC }, { PRC: '5050000||5050000 (min(P, H + MB))' });
    }

    /* ---- No SP: first price = the bid ---- */
    if (PN.aid) {
      const a = PN.aid;
      record(expectCase('p3_first_no_sp_ok', await bidOn(p2A, '2000000', a), { engine: 'tesSUCCESS', msg: MA, bidsOnly: true }));
      const k = await readK(H, a);
      rec('p3_first_no_sp_price', k.PRC === u64be(2_000_000) + u64be(2_000_000), { PRC: k.PRC }, { PRC: '2000000||2000000' });
    }

    /* ---- XAH no MB: 1 drop increment ---- */
    if (PX.aid) {
      const a = PX.aid;
      record(expectCase('p3_no_mb_seat', await bidOn(p2A, '5000000', a), { engine: 'tesSUCCESS', msg: MA, bidsOnly: true }));
      let r = await bidOn(p2B, '1000000', a);
      record(expectCase('p3_no_mb_not_above_price_reject', r, { engine: 'tecHOOK_REJECTED', msgIncludes: 'bid not above price', bidsOnly: true }));
      r = await bidOn(p2B, '2000000', a);
      record(expectCase('p3_no_mb_underbid_ok', r, { engine: 'tesSUCCESS', msg: UB, bidsOnly: true, emitMin: 1 }));
      let w = await waitIfr0(a);
      let k = w.kk || {};
      rec('p3_no_mb_underbid_price', w.ok && k.PRC === u64be(2_000_001) + u64be(5_000_000), { PRC: k.PRC }, { PRC: '2000001||5000000 (P + 1 drop)' });
      r = await bidOn(p2B, '6000000', a);
      record(expectCase('p3_no_mb_outbid_ok', r, { engine: 'tesSUCCESS', msg: OB, bidsOnly: true, emitMin: 1 }));
      w = await waitIfr0(a);
      k = w.kk || {};
      rec('p3_no_mb_outbid_price', w.ok && k.PRC === u64be(5_000_001) + u64be(6_000_000) && k.WIN === accHex(p2B.classicAddress),
        { PRC: k.PRC, WIN: k.WIN }, { PRC: '5000001||6000000 (H + 1 drop)' });
    }

    /* ---- Cap of 4 refunds in flight: same-ledger burst of 5 underbids ---- */
    if (PB.aid) {
      const a = PB.aid;
      record(expectCase('p3_burst_seat', await bidOn(p2A, '20000000', a), { engine: 'tesSUCCESS', msg: MA, bidsOnly: true }));
      let done = false;
      let base = 1_000_000;
      for (let attempt = 1; attempt <= 3 && !done; attempt++) {
        await waitIfr0(a);
        const amts = [1, 3, 5, 7, 9].map((x) => String(base + x * 100_000));
        const res = await burst(p2B, amts.map((v) => payTx(v, a)));
        const ledgers = new Set(res.map((x) => x.ledger));
        const ok4 = res.slice(0, 4).every((x) => x.engine === 'tesSUCCESS' && msgsOfR(x).includes(UB));
        const fifth = res[4];
        const fifthMsgs = msgsOfR(fifth);
        log('p3 burst attempt', attempt, JSON.stringify(res.map((x) => ({ e: x.engine, l: x.ledger, m: bidsMsgs(decodeHr(x.meta))[0] || '' }))));
        if (ledgers.size === 1) {
          done = true;
          rec('p3_refunds_do_not_block', ok4, res.slice(0, 4).map((x) => x.engine + ' ' + (bidsMsgs(decodeHr(x.meta))[0] || '')), { first4: 'all accepted in one ledger while earlier refunds are in flight' });
          rec('p3_cap_5th_underbid_reject', fifth.engine === 'tecHOOK_REJECTED' && fifthMsgs.includes('too many refunds in flight'), { engine: fifth.engine, msgs: fifthMsgs }, { msg: 'too many refunds in flight' });
          const w = await waitIfr0(a);
          const k = w.kk || {};
          const wantPrice = base + 800_000;
          rec('p3_cap_state_after_drain', w.ok && k.PRC === u64be(wantPrice) + u64be(20_000_000) && k.WIN === accHex(p2A.classicAddress),
            { PRC: k.PRC, WIN: k.WIN, IFR: k.IFR || null }, { PRC: wantPrice + '||20000000', IFR: 'drained to 0' });
        } else {
          log('p3 burst split across ledgers', [...ledgers].join(','), 'retrying');
          const w = await waitIfr0(a);
          const p = w.kk?.PRC ? Number(BigInt('0x' + w.kk.PRC.slice(0, 16))) : base;
          base = p;
        }
      }
      if (!done) rec('p3_cap_burst_one_ledger', false, 'burst never landed in one ledger in 3 attempts', { note: 'seeded cap case below still covers the gate' });
    }

    /* ---- Cap seeded: IFR 4 blocks underbids, never outbids, then admin CLR 0x04 ---- */
    if (PS.aid && WASM_SEED) {
      const a = PS.aid;
      record(expectCase('p3_seeded_seat', await bidOn(p2A, '10000000', a), { engine: 'tesSUCCESS', msg: MA, bidsOnly: true }));
      const ok = await seedRaw(asciiHex('IFR'), '0004', a);
      let r = await bidOn(p2B, '2000000', a);
      record(expectCase('p3_cap_seeded_underbid_reject', r, { engine: 'tecHOOK_REJECTED', msgIncludes: 'too many refunds in flight', bidsOnly: true }));
      let k = await readK(H, a);
      rec('p3_cap_seeded_state_unchanged', ok && k.IFR === '0004' && k.PRC === u64be(1_000_000) + u64be(10_000_000) && k.WIN === accHex(p2A.classicAddress),
        { seeded: ok, IFR: k.IFR, PRC: k.PRC }, { IFR: '0004', PRC: '1000000||10000000' });
      const aBal0 = await bal(client, p2A.classicAddress);
      r = await bidOn(p2B, '11000000', a);
      record(expectCase('p3_outbid_not_capped', r, { engine: 'tesSUCCESS', msg: OB, bidsOnly: true, emitMin: 1 }));
      const kIn = await readK(H, a);
      const w = await waitPred(async () => {
        const kk = await readK(H, a);
        const ab = await bal(client, p2A.classicAddress);
        return { ok: kk.IFR === '0004' && (ab - aBal0) === 10_000_000n, kk };
      }, 90000);
      rec('p3_outbid_not_capped_ifr', w.ok && ['0005', '0004'].includes(kIn.IFR), { ifrAfterBid: kIn.IFR, ifrAfterCbak: w.kk?.IFR }, { IFR: '5 then back to 4 (seeded phantom stays)' });
      r = await clrInvoke('04', a);
      k = await readK(H, a);
      const cm = msgsOfR(r);
      rec('p3_clr_ifr_bids_and_finalise', r.engine === 'tesSUCCESS' && cm.filter((m) => m === 'marker cleared').length >= 2 && !k.IFR,
        { engine: r.engine, msgs: cm, IFR: k.IFR || null }, { msgs: 'marker cleared from Bids and Finalise', IFR: 'deleted' });
      r = await clrInvoke('08', a);
      record(expectCase('p3_clr_08_bad', r, { engine: 'tecHOOK_REJECTED', msgIncludes: 'CLR bad', anyHook: true }));
      /* PEN still blocks bids while a claim is in flight (seeded PEN) */
      const okP = await seedRaw(asciiHex('PEN'), 'AB'.repeat(32), a);
      r = await bidOn(p2A, '12000000', a);
      record(expectCase('p3_pen_blocks_bid', r, { engine: 'tecHOOK_REJECTED', msgIncludes: 'refund in flight', bidsOnly: true }));
      r = await clrInvoke('05', a);
      k = await readK(H, a);
      rec('p3_clr_05_pen_ifr', okP && r.engine === 'tesSUCCESS' && !k.PEN && !k.IFR, { engine: r.engine, PEN: k.PEN || null }, { PEN: 'deleted' });
      r = await bidOn(p2A, '12000000', a);
      record(expectCase('p3_bid_after_pen_cleared', r, { engine: 'tesSUCCESS', msg: OB, bidsOnly: true }));
      await waitIfr0(a);
    } else if (PS.aid) {
      rec('p3_seeded_cases_skipped', true, 'no SmokeStateSeed.wasm', { note: 'seeded cap/PEN/CLR need the seed hook' }, { engine: 'skipped' });
    }

    /* ---- Proxy IOU with MB 1 ---- */
    if (I5.aid) {
      const a = I5.aid;
      record(expectCase('p3_iou_seat', await bidOn(p2A, auc(50), a), { engine: 'tesSUCCESS', msg: MA, bidsOnly: true }));
      let k = await readK(H, a);
      rec('p3_iou_first_at_sp', prcIs(k.PRC, 10, 50), { PRC: k.PRC }, { PRC: '10||50' });
      const li0 = await lckI(H);
      const bi0 = await iouBal(p2B.classicAddress);
      let r = await bidOn(p2B, auc(20), a);
      record(expectCase('p3_iou_underbid_ok', r, { engine: 'tesSUCCESS', msg: UB, bidsOnly: true, emitMin: 1 }));
      let w = await waitIfr0(a);
      k = w.kk || {};
      const li1 = await lckI(H);
      const bi1 = await iouBal(p2B.classicAddress);
      rec('p3_iou_underbid_state', w.ok && prcIs(k.PRC, 21, 50) && k.WIN === accHex(p2A.classicAddress) && near(li1 - li0, 0) && near(bi1 - bi0, 0),
        { PRC: k.PRC, WIN: k.WIN, lck: li1 - li0, b: bi1 - bi0 }, { PRC: '21||50', iouLck: 'net 0', B: 'refunded 20' });
      r = await bidOn(p2B, auc(21.5), a);
      record(expectCase('p3_iou_below_price_inc_reject', r, { engine: 'tecHOOK_REJECTED', msgIncludes: 'below min increment', bidsOnly: true }));
      const ai0 = await iouBal(p2A.classicAddress);
      r = await bidOn(p2B, auc(60), a);
      record(expectCase('p3_iou_outbid_ok', r, { engine: 'tesSUCCESS', msg: OB, bidsOnly: true, emitMin: 1 }));
      w = await waitIfr0(a);
      k = w.kk || {};
      const ai1 = await iouBal(p2A.classicAddress);
      rec('p3_iou_outbid_state', w.ok && prcIs(k.PRC, 51, 60) && k.WIN === accHex(p2B.classicAddress) && near(ai1 - ai0, 50),
        { PRC: k.PRC, WIN: k.WIN, aRefund: ai1 - ai0 }, { PRC: '51||60', aRefund: 50 });
    }

    /* ---- Proxy IOU without MB: strict greater, zero increment ---- */
    if (I6.aid) {
      const a = I6.aid;
      record(expectCase('p3_iou_no_mb_seat', await bidOn(p2A, auc(50), a), { engine: 'tesSUCCESS', msg: MA, bidsOnly: true }));
      let r = await bidOn(p2B, auc(10), a);
      record(expectCase('p3_iou_no_mb_not_above_price_reject', r, { engine: 'tecHOOK_REJECTED', msgIncludes: 'bid not above price', bidsOnly: true }));
      r = await bidOn(p2B, auc(20), a);
      record(expectCase('p3_iou_no_mb_underbid_ok', r, { engine: 'tesSUCCESS', msg: UB, bidsOnly: true, emitMin: 1 }));
      let w = await waitIfr0(a);
      let k = w.kk || {};
      rec('p3_iou_no_mb_underbid_price', w.ok && prcIs(k.PRC, 20, 50), { PRC: k.PRC }, { PRC: '20||50 (price = P)' });
      r = await bidOn(p2B, auc(60), a);
      record(expectCase('p3_iou_no_mb_outbid_ok', r, { engine: 'tesSUCCESS', msg: OB, bidsOnly: true, emitMin: 1 }));
      w = await waitIfr0(a);
      k = w.kk || {};
      rec('p3_iou_no_mb_outbid_price', w.ok && prcIs(k.PRC, 50, 60) && k.WIN === accHex(p2B.classicAddress), { PRC: k.PRC, WIN: k.WIN }, { PRC: '50||60 (price = old max)' });
    }

    /* ---- Low host float: refund-causing bids rejected, state unchanged ---- */
    if (PF.aid && I7.aid && WASM_SEED) {
      record(expectCase('p3_float_seat_xah', await bidOn(p2A, '5000000', PF.aid), { engine: 'tesSUCCESS', msg: MA, bidsOnly: true }));
      record(expectCase('p3_float_seat_iou', await bidOn(p2A, auc(50), I7.aid), { engine: 'tesSUCCESS', msg: MA, bidsOnly: true }));
      /* no refunds in flight anywhere on this host before LCK is forged */
      for (const x of [PA, PT, PC, PX, PB, PS, I5, I6]) if (x.aid) await waitIfr0(x.aid);
      const lOrig = await lckX(H);
      const hb = await bal(client, H);
      const okS = await seedRaw(asciiHex('LCK'), u64be(hb), null);
      const kx0 = await readK(H, PF.aid);
      const ki0 = await readK(H, I7.aid);
      let r = await bidOn(p2B, '2000000', PF.aid);
      record(expectCase('p3_float_low_underbid_reject', r, { engine: 'tecHOOK_REJECTED', msgIncludes: 'host float low', bidsOnly: true }));
      r = await bidOn(p2B, '6000000', PF.aid);
      record(expectCase('p3_float_low_outbid_reject', r, { engine: 'tecHOOK_REJECTED', msgIncludes: 'host float low', bidsOnly: true }));
      r = await bidOn(p2B, auc(20), I7.aid);
      record(expectCase('p3_float_low_iou_underbid_reject', r, { engine: 'tecHOOK_REJECTED', msgIncludes: 'host float low', bidsOnly: true }));
      const kx1 = await readK(H, PF.aid);
      const ki1 = await readK(H, I7.aid);
      const okR = await seedRaw(asciiHex('LCK'), u64be(lOrig), null);
      const lBack = await lckX(H);
      rec('p3_float_low_state_unchanged', okS && okR && lBack === lOrig && kx1.HIGH === kx0.HIGH && kx1.PRC === kx0.PRC && kx1.WIN === kx0.WIN && ki1.PRC === ki0.PRC && ki1.WIN === ki0.WIN && !kx1.IFR && !ki1.IFR,
        { forged: okS, restored: okR, lck: String(lBack), lOrig: String(lOrig), PRC: kx1.PRC, IFR: kx1.IFR || null }, { HIGH: 'same', PRC: 'same', WIN: 'same', LCK: 'restored' });
      r = await bidOn(p2B, '2000000', PF.aid);
      record(expectCase('p3_float_ok_after_restore', r, { engine: 'tesSUCCESS', msg: UB, bidsOnly: true }));
      await waitIfr0(PF.aid);
    }

    /* ---- Underbid refund fails (DepositAuth set in the same ledger): strand, IFR drains, claim ---- */
    if (PD.aid) {
      const a = PD.aid;
      const p2D = genWallet();
      await ensureBank(70_000_000n);
      await pay(client, bank, p2D, 30_000_000n);
      record(expectCase('p3_strand_seat', await bidOn(p2A, '10000000', a), { engine: 'tesSUCCESS', msg: MA, bidsOnly: true }));
      let stranded = false;
      let amt = 2_000_000;
      let lBefore = 0n;
      for (let attempt = 1; attempt <= 3 && !stranded; attempt++) {
        lBefore = await lckX(H);
        const res = await burst(p2D, [payTx(String(amt), a), { TransactionType: 'AccountSet', SetFlag: 9 }]);
        log('p3 strand attempt', attempt, JSON.stringify(res.map((x) => ({ e: x.engine, l: x.ledger, m: bidsMsgs(decodeHr(x.meta))[0] || '' }))));
        await waitIfr0(a);
        await sleep(1500);
        const st = await rawK(a, strandKey(p2D.classicAddress));
        if (st) { stranded = true; break; }
        await softSubmit(submitAndWait(client, p2D, { TransactionType: 'AccountSet', Account: p2D.classicAddress, ClearFlag: 9 }));
        amt += 200_000;
      }
      const k = await readK(H, a);
      const st = await rawK(a, strandKey(p2D.classicAddress));
      const lAfter = await lckX(H);
      rec('p3_ifr_strand_written', stranded && !!st && st.slice(0, 16) === u64be(amt) && !k.IFR && !k.PEN && !k.LCKU && (lAfter - lBefore) === BigInt(amt) && k.WIN === accHex(p2A.classicAddress),
        { stranded, strand: st, IFR: k.IFR || null, PEN: k.PEN || null, LCKU: k.LCKU || null, lck: String(lAfter - lBefore) },
        { strand: amt + ' for D', IFR: 'drained', PEN: 'none', lck: '+amount stays locked as strand' });
      if (stranded) {
        const rb = await bidOn(p2B, '3000000', a);
        record(expectCase('p3_strand_does_not_block_bids', rb, { engine: 'tesSUCCESS', msg: UB, bidsOnly: true }));
        await waitIfr0(a);
        record(expectCase('p3_strand_clear_depositauth', await softSubmit(submitAndWait(client, p2D, { TransactionType: 'AccountSet', Account: p2D.classicAddress, ClearFlag: 9 })), { engine: 'tesSUCCESS', anyHook: true }));
        const d0 = await bal(client, p2D.classicAddress);
        const l0 = await lckX(H);
        const c = await invokeFin(p2D, p2Host, a);
        rec('p3_strand_claim_ok', c.engine === 'tesSUCCESS' && finMsg(c).includes('Stranded refund claimed'), { engine: c.engine, msg: finMsg(c) }, { msg: 'Stranded refund claimed' });
        const gone = await waitPred(async () => ({ ok: !(await rawK(a, strandKey(p2D.classicAddress))) && !(await readK(H, a)).PEN }), 90000);
        await sleep(1500);
        const d1 = await bal(client, p2D.classicAddress);
        const l1 = await lckX(H);
        const cf = await txFee(c.hash);
        rec('p3_strand_claim_paid', gone.ok && (d1 - d0) === BigInt(amt) - cf && (l0 - l1) === BigInt(amt),
          { gone: gone.ok, d: String(d1 - d0), fee: String(cf), lck: String(l0 - l1) }, { D: amt + ' - fee', lck: '-' + amt });
      }
    }

    /* ---- Buy-now against a standing max: refund prior, overpay back at Finalise ---- */
    if (PBN.aid) {
      const a = PBN.aid;
      record(expectCase('p3_bn_seat', await bidOn(p2A, '4000000', a), { engine: 'tesSUCCESS', msg: MA, bidsOnly: true }));
      const aBal0 = await bal(client, p2A.classicAddress);
      const r = await bidOn(p2B, '6000000', a);
      record(expectCase('p3_bn_vs_standing_max', r, { engine: 'tesSUCCESS', msg: 'Buy-now accepted with prior refund', bidsOnly: true, emitMin: 2, emitMax: 2 }));
      const moved = await waitUriOwner(client, PBN.lot, p2B.classicAddress);
      const w = await waitPred(async () => { const kk = await readK(H, a); return { ok: kk.ST === '02' && !kk.SPEN, kk }; }, 90000);
      const k = w.kk || {};
      const aBal1 = await bal(client, p2A.classicAddress);
      rec('p3_bn_state', moved && w.ok && k.PRC === u64be(5_000_000) + u64be(6_000_000) && k.HIGH === u64be(6_000_000) && (aBal1 - aBal0) === 4_000_000n && !k.IFR,
        { moved, PRC: k.PRC, HIGH: k.HIGH, aRefund: String(aBal1 - aBal0) }, { PRC: '5000000||6000000', aRefund: 4000000, IFR: 'not used for buy-now' });
      const s0 = await bal(client, seller.classicAddress);
      const t0 = await bal(client, treasury.classicAddress);
      const b0 = await bal(client, p2B.classicAddress);
      const f = await invokeFin(p2B, p2Host, a);
      rec('p3_bn_finalise_ok', f.engine === 'tesSUCCESS' && finMsg(f).includes('Settlement pending'), { engine: f.engine, msg: finMsg(f), emit: finHr(f).emit }, { msg: 'Settlement pending' });
      const cl = await waitPred(async () => { const kk = await readK(H, a); return { ok: kk._count === 0, kk }; }, 120000);
      await sleep(1500);
      const s1 = await bal(client, seller.classicAddress);
      const t1 = await bal(client, treasury.classicAddress);
      const b1 = await bal(client, p2B.classicAddress);
      const fee = await txFee(f.hash);
      rec('p3_bn_overpay_refunded', cl.ok && (s1 - s0) === 4_750_000n && (t1 - t0) === 250_000n && (b1 - b0) === 1_000_000n - fee,
        { cleared: cl.ok, seller: String(s1 - s0), treasury: String(t1 - t0), buyer: String(b1 - b0), fee: String(fee) }, { seller: 4750000, treasury: 250000, buyer: '1000000 overpay - fee' });
    }

    /* ---- T1 settle: self-raised timed auction settles at the price, remainder via WPAY ---- */
    if (T1.aid && T1.exp != null) {
      const ok = await waitPred(async () => {
        const lr = await client.request({ command: 'ledger', ledger_index: 'validated' }).catch(() => null);
        return { ok: Number(lr?.result?.ledger?.close_time || 0) >= Number(T1.exp) + 2 };
      }, 420000, 5000);
      rec('p2_timed_expired', ok.ok, String(T1.exp), { expired: true });
      const s0 = await bal(client, seller.classicAddress);
      const t0 = await bal(client, treasury.classicAddress);
      const w0 = await bal(client, p2A.classicAddress);
      const l0 = await lckX(H);
      const f = await invokeFin(admin, p2Host, T1.aid);
      rec('p2_timed_raise_finalise_ok', f.engine === 'tesSUCCESS' && finMsg(f).includes('Settlement pending') && Number(finHr(f).emit || 0) === 4,
        { engine: f.engine, msg: finMsg(f), emit: finHr(f).emit }, { emit: 4, legs: 'URI, treasury, seller, remainder' });
      const cl = await waitPred(async () => { const kk = await readK(H, T1.aid); return { ok: kk._count === 0, kk }; }, 120000);
      await sleep(1500);
      const s1 = await bal(client, seller.classicAddress);
      const t1 = await bal(client, treasury.classicAddress);
      const w1 = await bal(client, p2A.classicAddress);
      const l1 = await lckX(H);
      const owner = await uriOwner(client, T1.lot);
      rec('p2_timed_raise_settles_at_price', cl.ok && (s1 - s0) === 950_000n && (t1 - t0) === 50_000n && (w1 - w0) === 2_000_000n + RES_INC
        && (l0 - l1) === 3_000_000n && owner === p2A.classicAddress,
        { cleared: cl.ok, keys: cl.kk, seller: String(s1 - s0), treasury: String(t1 - t0), winner: String(w1 - w0), lck: String(l0 - l1), owner },
        { seller: 950000, treasury: 50000, winner: '2000000 remainder + reserve_inc (URI Remit)', lck: -3000000, note: 'price SP 1 XAH, raised max 3 XAH' });
    }

    /* ---- p3 timed settle: Finalise at PRC, remainder back to the winner ---- */
    for (const [tag, A, isIou] of [['xah', T2, false], ['iou', I4, true]]) {
      if (!A.aid || A.exp == null) continue;
      const okE = await waitPred(async () => {
        const lr = await client.request({ command: 'ledger', ledger_index: 'validated' }).catch(() => null);
        return { ok: Number(lr?.result?.ledger?.close_time || 0) >= Number(A.exp) + 2 };
      }, 420000, 5000);
      rec('p3_timed_' + tag + '_expired', okE.ok, String(A.exp), { expired: true });
      const winner = isIou ? p2B : p2A;
      const balF = isIou ? iouBal : (x) => bal(client, x);
      const lF = isIou ? () => lckI(H) : () => lckX(H);
      const s0 = await balF(seller.classicAddress);
      const t0 = await balF(treasury.classicAddress);
      const w0 = await balF(winner.classicAddress);
      const l0 = await lF();
      const f = await invokeFin(admin, p2Host, A.aid);
      rec('p3_timed_' + tag + '_finalise_ok', f.engine === 'tesSUCCESS' && finMsg(f).includes('Settlement pending') && Number(finHr(f).emit || 0) === 4,
        { engine: f.engine, msg: finMsg(f), emit: finHr(f).emit }, { emit: 4, legs: 'URI, treasury, seller, remainder' });
      const cl = await waitPred(async () => { const kk = await readK(H, A.aid); return { ok: kk._count === 0, kk }; }, 120000);
      await sleep(1500);
      const s1 = await balF(seller.classicAddress);
      const t1 = await balF(treasury.classicAddress);
      const w1 = await balF(winner.classicAddress);
      const l1 = await lF();
      const owner = await uriOwner(client, A.lot);
      if (!isIou) {
        rec('p3_timed_xah_settles_at_prc', cl.ok && (s1 - s0) === 2_945_000n && (t1 - t0) === 155_000n && (w1 - w0) === 1_900_000n + RES_INC && (l0 - l1) === 5_000_000n && owner === p2A.classicAddress,
          { cleared: cl.ok, seller: String(s1 - s0), treasury: String(t1 - t0), winner: String(w1 - w0), lck: String(l0 - l1), owner },
          { seller: 2945000, treasury: 155000, winner: '1900000 remainder + reserve_inc', lck: -5000000, note: 'price 3.1 of max 5' });
      } else {
        rec('p3_timed_iou_settles_at_prc', cl.ok && near(s1 - s0, 48.45) && near(t1 - t0, 2.55) && near(w1 - w0, 9) && near(l0 - l1, 60) && owner === p2B.classicAddress,
          { cleared: cl.ok, seller: s1 - s0, treasury: t1 - t0, winner: w1 - w0, lck: l0 - l1, owner },
          { seller: 48.45, treasury: 2.55, winner: '9 remainder', iouLck: -60, note: 'price 51 of max 60' });
      }
    }
  }

  /* ---- Global TBD/TBN/LCK asserts (Bids host local ns) ---- */
  {
    const g = await readHostLocalKeys(client, host.classicAddress, NS_BIDS, ['TBD', 'TBN', 'LCK']);
    const tbd = g.TBD ? Buffer.from(g.TBD, 'hex').readUInt32BE(0) : 0;
    const tbn = g.TBN ? Buffer.from(g.TBN, 'hex').readUInt32BE(0) : 0;
    const lck = g.LCK ? Buffer.from(g.LCK, 'hex').readBigUInt64BE(0) : 0n;
    OUT.globals = { TBD: g.TBD || null, TBN: g.TBN || null, LCK: g.LCK || null, tbd, tbn, lck: lck.toString() };
    record({
      name: 'bids_global_tbd_tbn_lck_moved',
      pass: tbd > 0 && tbn > 0 && lck > 0n,
      engine: 'ok',
      gotMsg: JSON.stringify(OUT.globals),
      want: { tbd: '>0', tbn: '>0', lck: '>0' },
    });
  }

  OUT.auctions = Object.fromEntries(
    Object.entries(auctions).map(([k, v]) => [k, { aid: v.aid, lot: v.lot, hash: v.hash }]),
  );
  OUT.summary = { pass: OUT.pass, fail: OUT.fail, total: OUT.pass + OUT.fail };
  save();
  log('SUMMARY', JSON.stringify(OUT.summary));
  await client.disconnect();
  process.exit(OUT.fail > 0 ? 1 : 0);
}

main().catch((e) => {
  console.error(e);
  try {
    fs.writeFileSync(
      path.join(OUTDIR, 'IT_BIDS.json'),
      JSON.stringify({ error: String(e?.stack || e), log: logLines }, null, 2),
    );
  } catch { /* */ }
  process.exit(2);
});
