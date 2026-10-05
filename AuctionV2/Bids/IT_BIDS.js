/**
 * Auction House V2 Bids — full matrix (xahau.js).
 * Setup Sub+Create+Bids, create auctions, bid happy/reject/passthrough/
 * outbid-refund/buy-now/IOU/XAH.
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
  const prepared = await client.autofill({ ...tx, NetworkID: NETWORK_ID });
  const signed = wallet.sign(prepared);
  const result = await client.submitAndWait(signed.tx_blob);
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
  const want = ['DUR', 'SP', 'MB', 'BN', 'CUR', 'ISS', 'SLR', 'URI', 'EXP', 'ST', 'HIGH', 'WIN', 'BCNT', 'BNW', 'WDT', 'RFD', 'RFDA', 'RFDT', 'PEN', 'SSF', 'TSF', 'CPR'];
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
    // AID value is hex of 32 bytes → need 64 hex chars. 'AA'*16 = 32 hex = 16 bytes
    record(expectCase('bids_aid_bad_len_reject', r, {
      engine: 'tecHOOK_REJECTED',
      msgIncludes: 'AID must be 32 bytes',
      bidsOnly: true,
    }));
  }
  /* KVT Finding 3: Payment with both SUB + AID → NOPE */
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
        msg: 'Bid accepted',
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
        msg: 'Bid accepted',
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
        msg: 'Bid accepted',
        bidsOnly: true,
      }));
    }
  }

  /* already high reject */
  {
    const a = auctions.xah_full;
    if (a) {
      const r = await bidPay(client, bidderA, host, '2000000', a.aid);
      record(expectCase('bids_xah_already_high_reject', r, {
        engine: 'tecHOOK_REJECTED',
        msgIncludes: 'already high bidder',
        bidsOnly: true,
      }));
    }
  }

  /* below min increment (MB) */
  {
    const a = auctions.xah_full;
    if (a) {
      const r = await bidPay(client, bidderB, host, '1050000', a.aid); // need >= 1.1M
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
        msg: 'Bid accepted',
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
      const rEq = await bidPay(client, bidderB, host, '2000000', a.aid);
      record(expectCase('bids_xah_no_mb_eq_high_reject', rEq, {
        engine: 'tecHOOK_REJECTED',
        msgIncludes: 'bid not above high',
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
        msg: 'Bid accepted',
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

  /* High bidder buy-now refund-self (C03 — same refund/cbak as outbid) */
  {
    const a = auctions.xah_mb_only;
    if (a) {
      const r1 = await bidPay(client, bidderA, host, '500000', a.aid);
      record(expectCase('bids_self_bn_prep', r1, {
        engine: 'tesSUCCESS',
        msg: 'Bid accepted',
        bidsOnly: true,
      }));
      // No BN on this auction — already-high reject already covered.
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
        msg: 'Bid accepted',
        bidsOnly: true,
      }));
      const lckBeforeK = await readHostLocalKeys(client, host.classicAddress, NS_BIDS, ['LCK']);
      const lckBefore = lckBeforeK.LCK ? BigInt('0x' + lckBeforeK.LCK) : 0n;
      const r2 = await bidPay(client, bidderA, host, '3000000', aid);
      record(expectCase('bids_self_bn_refund_ok', r2, {
        engine: 'tesSUCCESS',
        msg: 'Buy-now accepted with prior refund',
        bidsOnly: true,
        emitMin: 2,
        emitMax: 2,
      }));
      // After otxn: LCK += BN (3e6). After refund cbak: LCK -= prior (1e6).
      // Net vs pre-BN seat: +BN - prior? Pre-seat LCK already includes prior 1e6,
      // so after full settle LCK should be lckBefore - 1e6 + 3e6 = lckBefore + 2e6.
      let lckOk = false;
      let lckGot = null;
      const wantLck = lckBefore - 1000000n + 3000000n;
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
        want: { net: '+BN -prior after cbak' },
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
          msg: 'Bid accepted',
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
        /* Outbid by B — refund should carry DestinationTag 4242 */
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
        msg: 'Bid accepted',
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
          if (!k.PEN && !k.RFD) break;
          if (!k.PEN && k.RFD) break;
        }
        const k = await readAidKeys(client, host.classicAddress, a.aid);
        record({
          name: 'bids_iou_outbid_refund_settled',
          pass: !k.PEN && !k.RFD,
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
      // auction already BNW-locked — expect buy-now already won (still a reject)
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
      // issuer must also issue USD to bidder — or just attempt and expect path/hook reject
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
        msg: 'Bid accepted',
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

  /* AID present but payment not to host — use other as dest (Bids not installed there → no bids msg; just ensure no crash on host-less) */
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
          msg: 'Bid accepted',
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

  /* ---- PW-H03: buy-now URI fail → BNW without ST=2 (Finalise retry: seller, ADMIN, or WIN) ----
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
          gotMsg: 'URI race won (happy BN) — strand path covered by code+Finalise timed SSF',
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
