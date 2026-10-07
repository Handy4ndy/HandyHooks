/**
 * Auction House V2 Create - full gap-covering matrix (xahau.js).
 * Sub setup + Create rejects/happy paths. Labels: setup_* vs create_*.
 *
 * Run: node IT_CREATE.js
 * Writes IT_CREATE.json next to this script.
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

const WASM_CREATE = fs.readFileSync(path.join(OUTDIR, 'AuctionCreate.wasm'));
const WASM_SUB = fs.readFileSync(
  fs.existsSync(path.join(ROOT, 'Subscription', 'AuctionSub.wasm'))
    ? path.join(ROOT, 'Subscription', 'AuctionSub.wasm')
    : path.join(ROOT, 'AuctionSub.wasm'),
);

const CREATE_HASH = crypto.createHash('sha512').update(WASM_CREATE).digest().slice(0, 32).toString('hex').toUpperCase();
const SUB_HASH = crypto.createHash('sha512').update(WASM_SUB).digest().slice(0, 32).toString('hex').toUpperCase();
const NS_SUB = crypto.createHash('sha256').update('AuctionHouseV2Sub-create-matrix-' + Date.now()).digest().toString('hex').toUpperCase();
const NS_CREATE = crypto.createHash('sha256').update('AuctionHouseV2Create-matrix-' + Date.now()).digest().toString('hex').toUpperCase();

/* Sub: Payment+Invoke. Create: Payment+Invoke+Remit (so Passthrough cases hit Create). */
const HOOK_ON_PAYMENT_INVOKE = 'FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFFFFFFFFFFFFFBFFFFE';
const HOOK_ON_CREATE = 'FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF77FFFFFFFFFFFFFFFFFBFFFFE';
const HSF_OVERRIDE = 1;
const ASF_DISALLOW_INCOMING_REMIT = 16;
const ASF_DEPOSIT_AUTH = 9; /* asfDepositAuth */

const PRICE = 10_000_000n;
const PERIOD = 3600;
const PERIOD_SHORT = 60;
const SPLIT_PCT = 0;
const AUCCAP = 10;
const DUR_S = 3600;
const DUR_MIN = 300;
const DUR_MAX = 2592000;

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

function createHookMsgs(hrs) {
  return hrs
    .filter((h) => String(h.hash || '').toUpperCase() === CREATE_HASH)
    .map((h) => h.msg)
    .filter(Boolean);
}

function anyMsgs(hrs) {
  return hrs.map((h) => h.msg).filter(Boolean);
}

async function bal(client, acct) {
  const r = await client.request({ command: 'account_info', account: acct, ledger_index: 'validated' });
  return BigInt(r.result.account_data.Balance);
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

async function mintUT(client, seller, { burnable = false } = {}) {
  const uri = `aucv3:${Date.now()}:${Math.random().toString(16).slice(2)}`;
  const uriHex = Buffer.from(uri, 'utf8').toString('hex').toUpperCase();
  const digest = crypto.createHash('sha256').update(uri, 'utf8').digest('hex').toUpperCase();
  const tx = {
    TransactionType: 'URITokenMint',
    Account: seller.classicAddress,
    URI: uriHex,
    Digest: digest,
  };
  if (burnable) tx.Flags = 1;
  const r = await softSubmit(submitAndWait(client, seller, tx));
  if (r.engine !== 'tesSUCCESS') throw new Error(`mint fail ${r.engine} ${r.hash}`);
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

async function createRemit(client, seller, host, lot, params, extra = {}) {
  const tx = {
    TransactionType: 'Remit',
    Account: seller.classicAddress,
    Destination: host.classicAddress,
    HookParameters: Object.entries(params).map(([k, v]) => hp(k, v)),
    ...extra,
  };
  if (lot != null) {
    tx.URITokenIDs = Array.isArray(lot) ? lot : [lot];
  }
  return softSubmit(submitAndWait(client, seller, tx));
}

async function readSellerState(client, hostAddr, sellerAddr) {
  const sellerNs = (accHex(sellerAddr) + '00'.repeat(12)).toUpperCase();
  const suffixes = {
    SUBEXP: Buffer.from('SUBEXP', 'ascii').toString('hex').toUpperCase(),
    ACTIVE: Buffer.from('ACTIVE', 'ascii').toString('hex').toUpperCase(),
    CAP: Buffer.from('CAP', 'ascii').toString('hex').toUpperCase(),
  };
  const ns = await client.request({
    command: 'account_namespace',
    account: hostAddr,
    namespace_id: sellerNs,
    ledger_index: 'validated',
  }).catch(() => null);
  const found = {};
  for (const o of ns?.result?.namespace_entries || []) {
    const k = String(o.HookStateKey || '').toUpperCase();
    for (const [name, suf] of Object.entries(suffixes)) {
      if (k.endsWith(suf)) found[name] = o.HookStateData;
    }
  }
  if (!found.SUBEXP && !found.ACTIVE && !found.CAP) return null;
  return found;
}

function parseSeller(keys) {
  if (!keys) return null;
  const out = { subexp: null, cap: null, active: null };
  if (keys.SUBEXP && keys.SUBEXP.length >= 16) {
    out.subexp = Buffer.from(keys.SUBEXP, 'hex').readBigUInt64BE(0).toString();
  }
  if (keys.CAP && keys.CAP.length >= 4) {
    out.cap = Buffer.from(keys.CAP, 'hex').readUInt16BE(0);
  }
  if (keys.ACTIVE && keys.ACTIVE.length >= 4) {
    out.active = Buffer.from(keys.ACTIVE, 'hex').readUInt16BE(0);
  }
  if (out.subexp == null && out.cap == null && out.active == null) return null;
  return out;
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
      if (k.endsWith(suf) && k.length >= suf.length) found[name] = data;
    }
  }
  return found;
}

async function readAidKeys(client, hostAddr, aid) {
  const ns = await client.request({
    command: 'account_namespace',
    account: hostAddr,
    namespace_id: aid,
    ledger_index: 'validated',
  }).catch(() => null);
  const want = ['DUR', 'SP', 'MB', 'BN', 'CUR', 'ISS', 'SLR', 'URI', 'EXP', 'ST', 'CPR', 'TSF'];
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

async function ledgerCloseTime(client) {
  const r = await client.request({ command: 'ledger', ledger_index: 'validated' });
  return Number(r.result.ledger.close_time || r.result.ledger?.close_time || 0);
}

async function waitHostTrustLine(client, hostAddr, issuerAddr, currencyIso, maxMs = 90000) {
  const curHex = Buffer.alloc(20);
  Buffer.from(currencyIso, 'ascii').copy(curHex, 12);
  const curStr = curHex.toString('hex').toUpperCase();
  const start = Date.now();
  while (Date.now() - start < maxMs) {
    const r = await client.request({
      command: 'account_lines',
      account: hostAddr,
      peer: issuerAddr,
      ledger_index: 'validated',
    }).catch(() => null);
    for (const line of r?.result?.lines || []) {
      const c = String(line.currency || '');
      const matchIso = c === currencyIso;
      const matchHex = c.length === 40 && c.toUpperCase() === curStr;
      if ((matchIso || matchHex) && Number(line.limit || 0) > 0) return true;
    }
    await sleep(2000);
  }
  return false;
}

function expectCase(name, r, want) {
  const hrs = decodeHr(r.meta);
  const msgs = anyMsgs(hrs);
  const cMsgs = createHookMsgs(hrs);
  const primary = cMsgs[0] || msgs[0] || '';
  const engineOk = want.engine === 'tesSUCCESS'
    ? r.engine === 'tesSUCCESS'
    : r.engine !== 'tesSUCCESS';
  let msgOk = true;
  const pool = want.createOnly ? cMsgs : msgs;
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
    const createHr = hrs.find((h) => String(h.hash || '').toUpperCase() === CREATE_HASH) || hrs[0];
    const emit = Number(createHr?.emit || 0);
    if (!(emit >= want.emitMin)) msgOk = false;
  }
  if (want.emitMax != null) {
    const createHr = hrs.find((h) => String(h.hash || '').toUpperCase() === CREATE_HASH) || hrs[0];
    const emit = Number(createHr?.emit || 0);
    if (!(emit <= want.emitMax)) msgOk = false;
  }
  if (want.engineAnyOf) {
    return {
      name,
      pass: want.engineAnyOf.includes(r.engine) && (want.msg == null && want.msgIncludes == null && !want.msgAnyOf ? true : msgOk),
      engine: r.engine,
      hash: r.hash,
      hook: hrs,
      want,
      gotMsg: primary,
      gotMsgs: msgs,
      createMsgs: cMsgs,
    };
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
    createMsgs: cMsgs,
  };
}

async function main() {
  const OUT = {
    when: new Date().toISOString(),
    createHookHash: CREATE_HASH,
    subHookHash: SUB_HASH,
    createWasmBytes: WASM_CREATE.length,
    subWasmBytes: WASM_SUB.length,
    nsSub: NS_SUB,
    nsCreate: NS_CREATE,
    client: 'xahau.js',
    cases: [],
    summary: {},
    gaps: [],
  };
  const save = () => {
    OUT.log = logLines.slice();
    const passed = OUT.cases.filter((c) => c.pass).length;
    const failed = OUT.cases.filter((c) => !c.pass).length;
    OUT.summary = { passed, failed, total: OUT.cases.length, pass: failed === 0 };
    OUT.pass = failed === 0;
    fs.writeFileSync(path.join(OUTDIR, 'IT_CREATE.json'), JSON.stringify(OUT, null, 2));
  };
  const record = (c) => {
    OUT.cases.push(c);
    log((c.pass ? 'PASS' : 'FAIL'), c.name, c.engine, c.gotMsg || '', c.hash || '');
    save();
  };

  log('Create HookHash', CREATE_HASH, 'bytes', WASM_CREATE.length);
  log('Sub HookHash', SUB_HASH, 'bytes', WASM_SUB.length);
  log('faucet bank...');
  const bank = await faucetWallet();
  log('bank', bank.classicAddress);
  OUT.bank = bank.classicAddress;

  const client = new Client(WS);
  await client.connect();

  for (let i = 0; i < 40; i++) {
    try {
      const b = await bal(client, bank.classicAddress);
      if (b > 200_000_000n) { OUT.bankBal = String(b); break; }
    } catch { /* wait */ }
    await sleep(1000);
  }

  const host = genWallet();
  const admin = genWallet();
  const treasury = genWallet();
  const issuer = genWallet();
  const seller1 = genWallet();
  const seller2 = genWallet();
  const seller3 = genWallet();
  const seller4 = genWallet(); /* CAP */
  const seller5 = genWallet(); /* expire / revoke */
  const seller6 = genWallet(); /* never subscribed */
  const other = genWallet();   /* non-host remit dest */

  OUT.wallets = {
    host: host.classicAddress,
    admin: admin.classicAddress,
    treasury: treasury.classicAddress,
    issuer: issuer.classicAddress,
    seller1: seller1.classicAddress,
    seller2: seller2.classicAddress,
    seller3: seller3.classicAddress,
    seller4: seller4.classicAddress,
    seller5: seller5.classicAddress,
    seller6: seller6.classicAddress,
    other: other.classicAddress,
  };
  OUT.walletSeeds = Object.fromEntries(
    Object.entries({
      bank, host, admin, treasury, issuer,
      seller1, seller2, seller3, seller4, seller5, seller6, other,
    }).map(([k, w]) => [k, { account: w.classicAddress, seed: w.seed }]),
  );

  async function ensureBank(minDrops) {
    for (let attempt = 0; attempt < 12; attempt++) {
      let b = 0n;
      try { b = await bal(client, bank.classicAddress); } catch { /* not funded yet */ }
      if (b >= minDrops) return b;
      log('bank top-up via faucet, bal', String(b), 'need', String(minDrops));
      const donor = await faucetWallet();
      // wait donor funded
      for (let i = 0; i < 40; i++) {
        try {
          const db = await bal(client, donor.classicAddress);
          if (db > 50_000_000n) break;
        } catch { /* wait */ }
        await sleep(1000);
      }
      const db = await bal(client, donor.classicAddress);
      const send = db > 20_000_000n ? db - 15_000_000n : 0n;
      if (send > 0n) {
        const r = await softSubmit(submitAndWait(client, donor, {
          TransactionType: 'Payment',
          Account: donor.classicAddress,
          Destination: bank.classicAddress,
          Amount: String(send),
        }));
        log('bank top-up result', r.engine, String(send));
      }
      await sleep(1500);
    }
    return bal(client, bank.classicAddress);
  }

  log('funding...');
  const fundPlan = [
    [host, 400_000_000n],
    [admin, 50_000_000n],
    [treasury, 30_000_000n],
    [issuer, 50_000_000n],
    [seller1, 120_000_000n],
    [seller2, 120_000_000n],
    [seller3, 120_000_000n],
    [seller4, 100_000_000n],
    [seller5, 100_000_000n],
    [seller6, 80_000_000n],
    [other, 40_000_000n],
  ];
  for (const [w, drops] of fundPlan) {
    await ensureBank(drops + 30_000_000n);
    await pay(client, bank, w, drops);
  }

  /* ---- setup: SetHook Sub + Create ---- */
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
      ],
    }));
    record(expectCase('setup_sethook_sub_create', r, { engine: 'tesSUCCESS' }));
    if (r.engine !== 'tesSUCCESS') {
      OUT.blocker = 'SetHook failed: ' + r.engine;
      save();
      await client.disconnect();
      process.exit(2);
    }
  }

  /* ---- setup: Sub settings ---- */
  for (const [name, hex] of [
    ['SUBPRICE', u64be(PRICE)],
    ['SUBPERIOD', u32be(PERIOD)],
    ['SUBSPLIT', u16be(SPLIT_PCT)],
    ['AUCCAP', u16be(AUCCAP)],
    ['TREASURY', accHex(treasury.classicAddress)],
  ]) {
    record(expectCase('setup_sub_set_' + name, await invokeAdmin(client, admin, host, name, hex), {
      engine: 'tesSUCCESS',
    }));
  }

  /* ---- setup: Subscribe sellers 1-3,5 ---- */
  for (const [label, seller] of [
    ['seller1', seller1],
    ['seller2', seller2],
    ['seller3', seller3],
    ['seller5', seller5],
  ]) {
    const r = await subPay(client, seller, host, PRICE);
    record(expectCase('setup_sub_' + label, r, { engine: 'tesSUCCESS', msgIncludes: 'Subscription' }));
    const st = parseSeller(await readSellerState(client, host.classicAddress, seller.classicAddress));
    OUT['state_' + label] = st;
    record({
      name: 'setup_sub_state_' + label,
      pass: !!st && st.cap === AUCCAP && Number(st.active) === 0 && !!st.subexp,
      engine: st ? 'ok' : 'missing',
      gotMsg: JSON.stringify(st),
      want: { cap: AUCCAP, active: 0 },
    });
  }

  /* ================================================================== */
  /* CREATE MATRIX                                                       */
  /* ================================================================== */

  /* ---- Passthrough: Payment / Invoke ---- */
  {
    const r = await softSubmit(submitAndWait(client, seller6, {
      TransactionType: 'Payment',
      Account: seller6.classicAddress,
      Destination: host.classicAddress,
      Amount: '1000000',
    }));
    record(expectCase('create_payment_passthrough', r, {
      engine: 'tesSUCCESS',
      msg: 'Passthrough',
      createOnly: true,
    }));
  }
  {
    const r = await softSubmit(submitAndWait(client, seller6, {
      TransactionType: 'Invoke',
      Account: seller6.classicAddress,
      Destination: host.classicAddress,
    }));
    record(expectCase('create_invoke_passthrough', r, {
      engine: 'tesSUCCESS',
      msg: 'Passthrough',
      createOnly: true,
    }));
  }

  /* ---- Burnable + asfDisallowIncomingRemit (seller1) ---- */
  {
    const lot = await mintUT(client, seller1, { burnable: true });
    const r = await createRemit(client, seller1, host, lot, {
      DUR: u64be(DUR_S),
      SP: u64be(1_000_000),
    });
    record(expectCase('create_burnable_reject', r, {
      engine: 'reject',
      msg: 'Burnable URITokens not allowed',
    }));
  }
  {
    const set = await softSubmit(submitAndWait(client, seller1, {
      TransactionType: 'AccountSet',
      Account: seller1.classicAddress,
      SetFlag: ASF_DISALLOW_INCOMING_REMIT,
    }));
    record(expectCase('setup_seller1_set_disallow_remit', set, { engine: 'tesSUCCESS' }));
    if (set.engine === 'tesSUCCESS') {
      const lot = await mintUT(client, seller1, { burnable: false });
      const r = await createRemit(client, seller1, host, lot, {
        DUR: u64be(DUR_S),
        SP: u64be(1_000_000),
      });
      record(expectCase('create_disallow_remit_reject', r, {
        engine: 'reject',
        msg: 'seller remits disabled',
      }));
    } else {
      record({
        name: 'create_disallow_remit_reject',
        pass: false,
        engine: 'skipped',
        gotMsg: 'AccountSet SetFlag 16 failed: ' + set.engine,
        want: { engine: 'reject', msg: 'seller remits disabled' },
      });
    }
  }
  /* Clear flag -> seller remits ok */
  {
    const clr = await softSubmit(submitAndWait(client, seller1, {
      TransactionType: 'AccountSet',
      Account: seller1.classicAddress,
      ClearFlag: ASF_DISALLOW_INCOMING_REMIT,
    }));
    record(expectCase('setup_seller1_clear_disallow_remit', clr, { engine: 'tesSUCCESS' }));
    if (clr.engine === 'tesSUCCESS') {
      const lot = await mintUT(client, seller1, { burnable: false });
      const r = await createRemit(client, seller1, host, lot, {
        DUR: u64be(DUR_S),
        SP: u64be(1_500_000),
        MB: u64be(50_000),
      });
      record(expectCase('create_seller1_after_clear_flag_ok', r, {
        engine: 'tesSUCCESS',
        msgIncludes: 'Auction created',
      }));
    }
  }


  /* ---- Seller DepositAuth entry gate (seller1) ---- */
  {
    const set = await softSubmit(submitAndWait(client, seller1, {
      TransactionType: 'AccountSet',
      Account: seller1.classicAddress,
      SetFlag: ASF_DEPOSIT_AUTH,
    }));
    record(expectCase('setup_seller1_set_depositauth', set, { engine: 'tesSUCCESS' }));
    if (set.engine === 'tesSUCCESS') {
      const lot = await mintUT(client, seller1, { burnable: false });
      const r = await createRemit(client, seller1, host, lot, {
        DUR: u64be(DUR_S),
        SP: u64be(1_000_000),
      });
      record(expectCase('create_depositauth_reject', r, {
        engine: 'reject',
        msg: 'seller DepositAuth',
      }));
      const clr = await softSubmit(submitAndWait(client, seller1, {
        TransactionType: 'AccountSet',
        Account: seller1.classicAddress,
        ClearFlag: ASF_DEPOSIT_AUTH,
      }));
      record(expectCase('setup_seller1_clear_depositauth', clr, { engine: 'tesSUCCESS' }));
    } else {
      record({
        name: 'create_depositauth_reject',
        pass: false,
        engine: 'skipped',
        gotMsg: 'AccountSet SetFlag 9 failed: ' + set.engine,
        want: { engine: 'reject', msg: 'seller DepositAuth' },
      });
    }
  }

  /* ---- DUR validation (seller2) ---- */
  {
    const lot = await mintUT(client, seller2, { burnable: false });
    const r = await createRemit(client, seller2, host, lot, {
      SP: u64be(1_000_000),
    });
    record(expectCase('create_dur_missing_reject', r, {
      engine: 'reject',
      msg: 'DUR required',
    }));
  }
  {
    const lot = await mintUT(client, seller2, { burnable: false });
    const r = await createRemit(client, seller2, host, lot, {
      DUR: u32be(DUR_S),
      SP: u64be(1_000_000),
    });
    record(expectCase('create_dur_4byte_reject', r, {
      engine: 'reject',
      msg: 'DUR must be 8 bytes',
    }));
  }
  {
    const lot = await mintUT(client, seller2, { burnable: false });
    const r = await createRemit(client, seller2, host, lot, {
      DUR: u64be(DUR_MIN - 1),
      SP: u64be(1_000_000),
    });
    record(expectCase('create_dur_below_min_reject', r, {
      engine: 'reject',
      msg: 'DUR out of bounds',
    }));
  }
  {
    const lot = await mintUT(client, seller2, { burnable: false });
    const r = await createRemit(client, seller2, host, lot, {
      DUR: u64be(DUR_MAX + 1),
      SP: u64be(1_000_000),
    });
    record(expectCase('create_dur_above_max_reject', r, {
      engine: 'reject',
      msg: 'DUR out of bounds',
    }));
  }
  {
    const lot = await mintUT(client, seller2, { burnable: false });
    const r = await createRemit(client, seller2, host, lot, {
      DUR: u64be(DUR_MIN),
      SP: u64be(1_000_000),
    });
    record(expectCase('create_dur_min_boundary_ok', r, {
      engine: 'tesSUCCESS',
      msgIncludes: 'Auction created',
    }));
  }
  {
    const lot = await mintUT(client, seller2, { burnable: false });
    const r = await createRemit(client, seller2, host, lot, {
      DUR: u64be(DUR_MAX),
      SP: u64be(1_000_000),
    });
    record(expectCase('create_dur_max_boundary_ok', r, {
      engine: 'tesSUCCESS',
      msgIncludes: 'Auction created',
    }));
  }

  /* ---- MB / BN rules ---- */
  {
    const lot = await mintUT(client, seller2, { burnable: false });
    const r = await createRemit(client, seller2, host, lot, {
      DUR: u64be(DUR_S),
      SP: u64be(2_000_000),
      MB: u64be(0),
    });
    record(expectCase('create_mb_zero_reject', r, {
      engine: 'reject',
      msg: 'MB must be > 0',
    }));
  }
  {
    const lot = await mintUT(client, seller2, { burnable: false });
    const r = await createRemit(client, seller2, host, lot, {
      DUR: u64be(DUR_S),
      SP: u64be(2_000_000),
      BN: u64be(2_000_000),
    });
    record(expectCase('create_bn_eq_sp_reject', r, {
      engine: 'reject',
      msg: 'BN must be > SP',
    }));
  }
  {
    const lot = await mintUT(client, seller2, { burnable: false });
    const r = await createRemit(client, seller2, host, lot, {
      DUR: u64be(DUR_S),
      SP: u64be(2_000_000),
      BN: u64be(1_999_999),
    });
    record(expectCase('create_bn_lt_sp_reject', r, {
      engine: 'reject',
      msg: 'BN must be > SP',
    }));
  }
  {
    const lot = await mintUT(client, seller2, { burnable: false });
    const r = await createRemit(client, seller2, host, lot, {
      DUR: u64be(DUR_S),
      MB: u32be(100),
    });
    record(expectCase('create_mb_bad_len_reject', r, {
      engine: 'reject',
      msg: 'MB must be 8 bytes',
    }));
  }
  {
    const lot = await mintUT(client, seller2, { burnable: false });
    const r = await createRemit(client, seller2, host, lot, {
      DUR: u64be(DUR_S),
      SP: u32be(100),
    });
    record(expectCase('create_sp_bad_len_reject', r, {
      engine: 'reject',
      msg: 'SP must be 8 bytes',
    }));
  }
  {
    const lot = await mintUT(client, seller2, { burnable: false });
    const r = await createRemit(client, seller2, host, lot, {
      DUR: u64be(DUR_S),
      BN: u32be(100),
    });
    record(expectCase('create_bn_bad_len_reject', r, {
      engine: 'reject',
      msg: 'BN must be 8 bytes',
    }));
  }

  /* ---- CUR / ISS ---- */
  {
    const lot = await mintUT(client, seller2, { burnable: false });
    const r = await createRemit(client, seller2, host, lot, {
      DUR: u64be(DUR_S),
      CUR: curIso('AUC'),
    });
    record(expectCase('create_cur_without_iss_reject', r, {
      engine: 'reject',
      msg: 'ISS required with CUR',
    }));
  }
  {
    const lot = await mintUT(client, seller2, { burnable: false });
    const r = await createRemit(client, seller2, host, lot, {
      DUR: u64be(DUR_S),
      ISS: accHex(issuer.classicAddress),
    });
    record(expectCase('create_iss_without_cur_reject', r, {
      engine: 'reject',
      msg: 'CUR required with ISS',
    }));
  }
  {
    const lot = await mintUT(client, seller2, { burnable: false });
    const r = await createRemit(client, seller2, host, lot, {
      DUR: u64be(DUR_S),
      CUR: Buffer.from('AU', 'ascii').toString('hex').toUpperCase(),
      ISS: accHex(issuer.classicAddress),
    });
    record(expectCase('create_cur_bad_len_reject', r, {
      engine: 'reject',
      msg: 'CUR bad length',
    }));
  }
  {
    const lot = await mintUT(client, seller2, { burnable: false });
    const r = await createRemit(client, seller2, host, lot, {
      DUR: u64be(DUR_S),
      CUR: curIso('AUC'),
      ISS: u64be(1), /* 8 bytes, not 20 */
    });
    record(expectCase('create_iss_bad_len_reject', r, {
      engine: 'reject',
      msg: 'ISS must be 20 bytes',
    }));
  }
  {
    const lot = await mintUT(client, seller2, { burnable: false });
    const r = await createRemit(client, seller2, host, lot, {
      DUR: u64be(DUR_S),
      CUR: '00'.repeat(20),
      ISS: accHex(issuer.classicAddress),
    });
    record(expectCase('create_cur_zero_reject', r, {
      engine: 'reject',
      msg: 'CUR/ISS must be non-zero',
    }));
  }

  /* ---- URIToken count / ownership / dest ---- */
  {
    const r = await createRemit(client, seller2, host, null, {
      DUR: u64be(DUR_S),
      SP: u64be(1_000_000),
    });
    record(expectCase('create_uritoken_missing_reject', r, {
      engine: 'reject',
      msgAnyOf: [
        'URITokenIDs missing or too short',
        'URITokenIDs must be a single 32-byte id',
        'URITokenIDs must contain exactly one token',
      ],
    }));
  }
  {
    const a = await mintUT(client, seller2, { burnable: false });
    const b = await mintUT(client, seller2, { burnable: false });
    const r = await createRemit(client, seller2, host, [a, b], {
      DUR: u64be(DUR_S),
      SP: u64be(1_000_000),
    });
    record(expectCase('create_uritoken_two_reject', r, {
      engine: 'reject',
      msgAnyOf: [
        'URITokenIDs must contain exactly one token',
        'URITokenIDs must be a single 32-byte id',
      ],
    }));
  }
  {
    const lot = await mintUT(client, seller3, { burnable: false });
    const r = await createRemit(client, seller2, host, lot, {
      DUR: u64be(DUR_S),
      SP: u64be(1_000_000),
    });
    /* Engine may reject before hook, or hook "not URI owner" */
    const hrs = decodeHr(r.meta);
    const msgs = anyMsgs(hrs);
    const hookOwner = msgs.includes('not URI owner');
    const engFail = r.engine !== 'tesSUCCESS';
    record({
      name: 'create_wrong_uri_owner_reject',
      pass: hookOwner || engFail,
      engine: r.engine,
      hash: r.hash,
      hook: hrs,
      want: { engine: 'reject', msgAnyOf: ['not URI owner'] },
      gotMsg: msgs[0] || r.engine,
      gotMsgs: msgs,
      note: hookOwner ? 'hook gate' : 'engine rejected non-owner Remit',
    });
  }
  {
    const lot = await mintUT(client, seller2, { burnable: false });
    const r = await softSubmit(submitAndWait(client, seller2, {
      TransactionType: 'Remit',
      Account: seller2.classicAddress,
      Destination: other.classicAddress,
      URITokenIDs: [lot],
      HookParameters: [
        hp('DUR', u64be(DUR_S)),
        hp('SP', u64be(1_000_000)),
      ],
    }));
    /* Host Create does not fire; Remit to non-host is outside Create gate.
       Pass if Create did not claim success / did not run. */
    const hrs = decodeHr(r.meta);
    const cMsgs = createHookMsgs(hrs);
    const created = cMsgs.some((m) => m.includes('Auction created'));
    record({
      name: 'create_remit_not_to_host',
      pass: !created,
      engine: r.engine,
      hash: r.hash,
      hook: hrs,
      want: { note: 'Create must not mint AID for Remit not destined to host' },
      gotMsg: cMsgs[0] || '(no Create execution)',
      createMsgs: cMsgs,
      note: 'Host-installed Create unreachable for Remit dest!=host; defensive NOPE unexercised',
    });
    if (!cMsgs.some((m) => m === 'Remit must be to host')) {
      OUT.gaps.push('create_remit_not_to_host: defensive "Remit must be to host" unreachable with host-only install');
    }
  }
  {
    /* PW-H01: gen-0 host Remit with URIToken must NOPE */
    const lot = await mintUT(client, host, { burnable: false });
    const r = await softSubmit(submitAndWait(client, host, {
      TransactionType: 'Remit',
      Account: host.classicAddress,
      Destination: other.classicAddress,
      URITokenIDs: [lot],
    }));
    record(expectCase('host_remit_uritoken_nope', r, {
      engine: 'reject',
      msgIncludes: 'Host gen-0 Remit must not carry URIToken',
      createOnly: true,
    }));
  }

  /* ---- not subscribed / expired / CAP ---- */
  {
    const lot = await mintUT(client, seller6, { burnable: false });
    const r = await createRemit(client, seller6, host, lot, {
      DUR: u64be(DUR_S),
      SP: u64be(1_000_000),
    });
    record(expectCase('create_not_subscribed_reject', r, {
      engine: 'reject',
      msg: 'not subscribed',
    }));
  }
  {
    /* REVOKE seller5 -> same gate as expired/cleared */
    const rev = await invokeAdmin(client, admin, host, 'REVOKE', accHex(seller5.classicAddress));
    record(expectCase('setup_revoke_seller5', rev, { engine: 'tesSUCCESS', msgIncludes: 'REVOKE' }));
    const lot = await mintUT(client, seller5, { burnable: false });
    const r = await createRemit(client, seller5, host, lot, {
      DUR: u64be(DUR_S),
      SP: u64be(1_000_000),
    });
    record(expectCase('create_revoked_sub_reject', r, {
      engine: 'reject',
      msg: 'not subscribed',
    }));
  }
  {
    /* True expiry: short SUBPERIOD, re-subscribe seller5, wait, create */
    record(expectCase('setup_subperiod_short', await invokeAdmin(client, admin, host, 'SUBPERIOD', u32be(PERIOD_SHORT)), {
      engine: 'tesSUCCESS',
    }));
    const sub = await subPay(client, seller5, host, PRICE);
    record(expectCase('setup_sub_seller5_short', sub, { engine: 'tesSUCCESS', msgIncludes: 'Subscription' }));
    record(expectCase('setup_subperiod_restore', await invokeAdmin(client, admin, host, 'SUBPERIOD', u32be(PERIOD)), {
      engine: 'tesSUCCESS',
    }));
    log('waiting for seller5 SUB to expire (~65s)...');
    await sleep(65_000);
    const lot = await mintUT(client, seller5, { burnable: false });
    const r = await createRemit(client, seller5, host, lot, {
      DUR: u64be(DUR_S),
      SP: u64be(1_000_000),
    });
    record(expectCase('create_expired_sub_reject', r, {
      engine: 'reject',
      msg: 'not subscribed',
    }));
  }
  {
    /* CAP=1 snapshotted on subscribe seller4 */
    record(expectCase('setup_auccap_1', await invokeAdmin(client, admin, host, 'AUCCAP', u16be(1)), {
      engine: 'tesSUCCESS',
    }));
    const sub = await subPay(client, seller4, host, PRICE);
    record(expectCase('setup_sub_seller4_cap1', sub, { engine: 'tesSUCCESS', msgIncludes: 'Subscription' }));
    const st0 = parseSeller(await readSellerState(client, host.classicAddress, seller4.classicAddress));
    record({
      name: 'setup_sub_state_seller4_cap1',
      pass: !!st0 && st0.cap === 1,
      engine: st0 ? 'ok' : 'missing',
      gotMsg: JSON.stringify(st0),
      want: { cap: 1 },
    });
    record(expectCase('setup_auccap_restore', await invokeAdmin(client, admin, host, 'AUCCAP', u16be(AUCCAP)), {
      engine: 'tesSUCCESS',
    }));
    const lot1 = await mintUT(client, seller4, { burnable: false });
    const r1 = await createRemit(client, seller4, host, lot1, {
      DUR: u64be(DUR_S),
      SP: u64be(1_000_000),
    });
    record(expectCase('create_at_cap_first_ok', r1, {
      engine: 'tesSUCCESS',
      msgIncludes: 'Auction created',
    }));
    const lot2 = await mintUT(client, seller4, { burnable: false });
    const r2 = await createRemit(client, seller4, host, lot2, {
      DUR: u64be(DUR_S),
      SP: u64be(1_000_000),
    });
    record(expectCase('create_at_cap_further_reject', r2, {
      engine: 'reject',
      msg: 'at CAP',
    }));
  }

  /* ---- Happy paths: XAH omit / with SP+MB+BN / BN-only / SP=0 ---- */
  let xahAid = null;
  let xahHash = null;
  let xahLot = null;
  let activeBeforeSeller2 = null;
  {
    activeBeforeSeller2 = parseSeller(await readSellerState(client, host.classicAddress, seller2.classicAddress));
    const lot = await mintUT(client, seller2, { burnable: false });
    const beforeClose = await ledgerCloseTime(client);
    const r = await createRemit(client, seller2, host, lot, {
      DUR: u64be(DUR_S),
    });
    record(expectCase('create_xah_omit_sp_mb_bn_ok', r, {
      engine: 'tesSUCCESS',
      msgIncludes: 'Auction created',
    }));
    if (r.engine === 'tesSUCCESS') {
      xahHash = r.hash;
      xahLot = lot;
      xahAid = aidFrom(r.hash, lot);
      OUT.xahAid = xahAid;
      const keys = await readAidKeys(client, host.classicAddress, xahAid);
      OUT.xahAidKeys = keys;
      const hasReq = keys.DUR && keys.SLR && keys.URI && keys.EXP && keys.ST;
      const stOk = keys.ST === '01';
      const noOpt = !keys.SP && !keys.MB && !keys.BN && !keys.CUR && !keys.ISS;
      record({
        name: 'create_aid_keys_required_present',
        pass: !!(hasReq && stOk && noOpt),
        engine: 'ok',
        gotMsg: JSON.stringify(keys),
        want: { DUR: 1, SLR: 1, URI: 1, EXP: 1, ST: '01', noSPMBBNCURISS: true },
      });
      if (keys.EXP && keys.EXP.length >= 16) {
        const exp = Buffer.from(keys.EXP, 'hex').readBigUInt64BE(0);
        const dur = Buffer.from(keys.DUR, 'hex').readBigUInt64BE(0);
        const expNum = Number(exp);
        const expectLo = beforeClose + Number(dur) - 5;
        const expectHi = beforeClose + Number(dur) + 120;
        record({
          name: 'create_exp_now_plus_dur_sanity',
          pass: expNum >= expectLo && expNum <= expectHi,
          engine: 'ok',
          gotMsg: JSON.stringify({ exp: expNum, beforeClose, dur: Number(dur), expectLo, expectHi }),
          want: { exp: 'now+DUR' },
        });
      } else {
        record({
          name: 'create_exp_now_plus_dur_sanity',
          pass: false,
          engine: 'missing',
          gotMsg: 'no EXP',
          want: { exp: 'now+DUR' },
        });
      }
      const stAfter = parseSeller(await readSellerState(client, host.classicAddress, seller2.classicAddress));
      const bumped = !!stAfter && Number(stAfter.active) === Number(activeBeforeSeller2?.active || 0) + 1;
      record({
        name: 'create_active_bump',
        pass: bumped,
        engine: stAfter ? 'ok' : 'missing',
        gotMsg: JSON.stringify({ before: activeBeforeSeller2, after: stAfter }),
        want: { active_delta: 1 },
      });
    }
  }
  {
    const lot = await mintUT(client, seller2, { burnable: false });
    const r = await createRemit(client, seller2, host, lot, {
      DUR: u64be(DUR_S),
      SP: u64be(2_000_000),
      MB: u64be(100_000),
      BN: u64be(5_000_000),
    });
    record(expectCase('create_xah_with_sp_mb_bn_ok', r, {
      engine: 'tesSUCCESS',
      msgIncludes: 'Auction created',
    }));
    if (r.engine === 'tesSUCCESS') {
      const aid = aidFrom(r.hash, lot);
      const keys = await readAidKeys(client, host.classicAddress, aid);
      record({
        name: 'create_aid_optional_sp_mb_bn_present',
        pass: !!(keys.SP && keys.MB && keys.BN && keys.ST === '01'),
        engine: 'ok',
        gotMsg: JSON.stringify(keys),
        want: { SP: 1, MB: 1, BN: 1, ST: '01' },
      });
    }
  }
  {
    const lot = await mintUT(client, seller2, { burnable: false });
    const r = await createRemit(client, seller2, host, lot, {
      DUR: u64be(DUR_S),
      BN: u64be(9_000_000),
    });
    record(expectCase('create_buynow_only_ok', r, {
      engine: 'tesSUCCESS',
      msgIncludes: 'Auction created',
    }));
  }
  {
    const lot = await mintUT(client, seller2, { burnable: false });
    const r = await createRemit(client, seller2, host, lot, {
      DUR: u64be(DUR_S),
      SP: u64be(0),
      MB: u64be(10_000),
    });
    record(expectCase('create_start_zero_ok', r, {
      engine: 'tesSUCCESS',
      msgIncludes: 'Auction created',
    }));
  }

  /* ---- IOU + TrustSet emit; second same CUR skips TrustSet ---- */
  {
    const lot = await mintUT(client, seller3, { burnable: false });
    const r = await createRemit(client, seller3, host, lot, {
      DUR: u64be(DUR_S),
      CUR: curIso('AUC'),
      ISS: accHex(issuer.classicAddress),
    });
    const hrs = decodeHr(r.meta);
    const createHr = hrs.find((h) => String(h.hash || '').toUpperCase() === CREATE_HASH) || hrs[0];
    const emit = Number(createHr?.emit || 0);
    const okMsg = (createHr?.msg || '').includes('Auction created');
    const pass = r.engine === 'tesSUCCESS' && okMsg && emit >= 1;
    record({
      name: 'create_iou_trustset_emit',
      pass,
      engine: r.engine,
      hash: r.hash,
      hook: hrs,
      want: { engine: 'tesSUCCESS', msgIncludes: 'Auction created', emitMin: 1 },
      gotMsg: createHr?.msg || '',
      emit,
    });
    const st = parseSeller(await readSellerState(client, host.classicAddress, seller3.classicAddress));
    OUT.state_seller3_after_iou = st;
    record({
      name: 'create_seller3_active_bumped',
      pass: !!st && Number(st.active) >= 1,
      engine: st ? 'ok' : 'missing',
      gotMsg: JSON.stringify(st),
      want: { active_ge: 1 },
    });

    log('waiting for host AUC trustline...');
    const lined = await waitHostTrustLine(client, host.classicAddress, issuer.classicAddress, 'AUC');
    record({
      name: 'setup_host_auc_trustline_ready',
      pass: lined,
      engine: lined ? 'ok' : 'timeout',
      gotMsg: lined ? 'line present' : 'no line after wait',
      want: { trustline: true },
    });

    const lot2 = await mintUT(client, seller3, { burnable: false });
    const r2 = await createRemit(client, seller3, host, lot2, {
      DUR: u64be(DUR_S),
      CUR: curIso('AUC'),
      ISS: accHex(issuer.classicAddress),
    });
    const hrs2 = decodeHr(r2.meta);
    const createHr2 = hrs2.find((h) => String(h.hash || '').toUpperCase() === CREATE_HASH) || hrs2[0];
    const emit2 = Number(createHr2?.emit || 0);
    const msg2 = createHr2?.msg || '';
    const pass2 = r2.engine === 'tesSUCCESS'
      && msg2.includes('Auction created')
      && !msg2.includes('with TrustSet')
      && emit2 === 0;
    record({
      name: 'create_iou_second_skips_trustset',
      pass: pass2,
      engine: r2.engine,
      hash: r2.hash,
      hook: hrs2,
      want: { engine: 'tesSUCCESS', msg: 'Auction created', emitMax: 0 },
      gotMsg: msg2,
      emit: emit2,
    });
  }

  /* ---- non-Create Remit reject (to host, has token, no DUR already covered;
         also Remit to host with empty params after token) ---- */
  {
    const lot = await mintUT(client, seller2, { burnable: false });
    const r = await createRemit(client, seller2, host, lot, {});
    record(expectCase('create_noncreate_remit_reject', r, {
      engine: 'reject',
      msg: 'DUR required',
    }));
  }

  /* ---- BN > SP happy ---- */
  {
    const lot = await mintUT(client, seller2, { burnable: false });
    const r = await createRemit(client, seller2, host, lot, {
      DUR: u64be(DUR_S),
      SP: u64be(1_000_000),
      BN: u64be(1_000_001),
    });
    record(expectCase('create_bn_gt_sp_ok', r, {
      engine: 'tesSUCCESS',
      msgIncludes: 'Auction created',
    }));
  }

  /* ---- CUR 20-byte raw form ---- */
  {
    const lot = await mintUT(client, seller3, { burnable: false });
    const cur20 = Buffer.alloc(20);
    cur20[12] = 0x42; cur20[13] = 0x54; cur20[14] = 0x43; /* BTC */
    const r = await createRemit(client, seller3, host, lot, {
      DUR: u64be(DUR_S),
      CUR: cur20.toString('hex').toUpperCase(),
      ISS: accHex(issuer.classicAddress),
    });
    record(expectCase('create_iou_cur20_ok', r, {
      engine: 'tesSUCCESS',
      msgIncludes: 'Auction created',
    }));
  }


  /* ---- PW-H01: Remit Amounts vs LCK / head ---- */
  {
    const hostBal = await bal(client, host.classicAddress);
    /* Over spendable: Remit bal - 1 drop (violates fixed 2 XAH head with LCK=0) */
    const over = hostBal > 2_000_001n ? (hostBal - 1n) : hostBal;
    const rOver = await softSubmit(submitAndWait(client, host, {
      TransactionType: 'Remit',
      Account: host.classicAddress,
      Destination: other.classicAddress,
      Amounts: [{ AmountEntry: { Amount: String(over) } }],
    }));
    record(expectCase('host_remit_amounts_xah_over_lck_nope', rOver, {
      engine: 'reject',
      msgIncludes: 'Insufficient spendable float',
      createOnly: true,
    }));
    const under = 100_000n;
    const rUnder = await softSubmit(submitAndWait(client, host, {
      TransactionType: 'Remit',
      Account: host.classicAddress,
      Destination: other.classicAddress,
      Amounts: [{ AmountEntry: { Amount: String(under) } }],
    }));
    record(expectCase('host_remit_amounts_xah_under_lck_ok', rUnder, {
      engine: 'tesSUCCESS',
      msg: 'Outgoing ok',
      createOnly: true,
    }));
  }

  /* ---- KVT #13: host gen-0 Remit carries at most 3 Amounts ----
   * Host-issued IOUs (AAA/BBB/CCC) have no LCK, so only the count gate and
   * the nested GUARD budget are under test. */
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
      createOnly: true,
    }));
    record(expectCase('host_remit_3_amounts_2iou_ok', await remitN(['1000', hIou('AAA'), hIou('BBB')]), {
      engine: 'tesSUCCESS',
      msg: 'Outgoing ok',
      createOnly: true,
    }));
    record(expectCase('host_remit_3_iou_ok', await remitN([hIou('AAA'), hIou('BBB'), hIou('CCC')]), {
      engine: 'tesSUCCESS',
      msg: 'Outgoing ok',
      createOnly: true,
    }));
  }

  /* ---- PW-C01: TrustSet fail -> TSF + Remit URI back ---- */
  {
    const ghost = genWallet(); /* never funded -> TrustSet tecNO_DST */
    const activeBefore = parseSeller(await readSellerState(client, host.classicAddress, seller3.classicAddress));
    const lot = await mintUT(client, seller3, { burnable: false });
    const r = await createRemit(client, seller3, host, lot, {
      DUR: u64be(DUR_S),
      CUR: curIso('TSF'),
      ISS: accHex(ghost.classicAddress),
    });
    const hrs = decodeHr(r.meta);
    const createHr = hrs.find((h) => String(h.hash || '').toUpperCase() === CREATE_HASH) || hrs[0];
    const emit = Number(createHr?.emit || 0);
    const msg = String(createHr?.msg || '');
    const okCreate = r.engine === 'tesSUCCESS'
      && msg.includes('Auction created')
      && emit >= 1;
    /* Unreadable issuer: refuse before TrustSet. Do not expect the old emit path. */
    const refused = r.engine !== 'tesSUCCESS'
      && msg.includes('issuer AccountRoot not found')
      && emit === 0;
    record({
      name: 'create_trustset_cbak_fail_emit',
      pass: refused,
      engine: r.engine,
      hash: r.hash,
      hook: hrs,
      want: { engine: 'tecHOOK_REJECTED', msg: 'issuer AccountRoot not found', emit: 0 },
      gotMsg: msg,
      emit,
    });
    let aid = null;
    if (okCreate) {
      aid = aidFrom(r.hash, lot);
      /* Wait for TrustSet fail cbak + Remit-back */
      let keys = {};
      let tsfSeen = false;
      let cleared = false;
      const t0 = Date.now();
      while (Date.now() - t0 < 120_000) {
        keys = await readAidKeys(client, host.classicAddress, aid);
        if (keys.TSF === '01' && keys.SLR && keys.URI && !keys.ST && !keys.EXP && !keys.CPR) {
          tsfSeen = true;
          record({
            name: 'create_trustset_cbak_fail_tsf',
            pass: true,
            engine: 'ok',
            gotMsg: JSON.stringify(keys),
            want: { TSF: 1, SLR: 1, URI: 1, ST: 'gone', EXP: 'gone', CPR: 'gone' },
          });
          break;
        }
        if (Object.keys(keys).length === 0) {
          /* Remit-back already cleared AID */
          cleared = true;
          break;
        }
        await sleep(2000);
      }
      if (!tsfSeen && !cleared) {
        record({
          name: 'create_trustset_cbak_fail_tsf',
          pass: false,
          engine: 'timeout',
          gotMsg: JSON.stringify(keys),
          want: { TSF: 1 },
        });
      }
      /* ACTIVE should have been decremented on fail (then Remit does not re-bump) */
      const activeAfter = parseSeller(await readSellerState(client, host.classicAddress, seller3.classicAddress));
      const a0 = activeBefore ? Number(activeBefore.active) : null;
      const a1 = activeAfter ? Number(activeAfter.active) : null;
      record({
        name: 'create_trustset_fail_active_dec',
        pass: a0 != null && a1 != null && a1 === a0,
        engine: 'ok',
        gotMsg: JSON.stringify({ before: a0, after: a1, note: 'bump then fail-dec -> net 0' }),
        want: { active_net: 0 },
      });
      /* Wait for Remit-back success: URI owner = seller, AID clear */
      let uriBack = false;
      const t1 = Date.now();
      while (Date.now() - t1 < 90_000) {
        keys = await readAidKeys(client, host.classicAddress, aid);
        const ns = await client.request({
          command: 'account_objects',
          account: seller3.classicAddress,
          type: 'uri_token',
          ledger_index: 'validated',
        }).catch(() => null);
        const owns = (ns?.result?.account_objects || []).some(
          (o) => String(o.index || o.URITokenID || '').toUpperCase() === lot.toUpperCase()
            || String(o.index || '').toUpperCase() === lot.toUpperCase(),
        );
        /* Also check via uri_token object owner */
        let ownerOk = owns;
        if (!ownerOk) {
          const ut = await client.request({
            command: 'ledger_entry',
            index: lot,
            ledger_index: 'validated',
          }).catch(() => null);
          const node = ut?.result?.node || {};
          const owner = node.Owner || node.owner;
          if (owner === seller3.classicAddress) ownerOk = true;
        }
        if (ownerOk && Object.keys(keys).length === 0) {
          uriBack = true;
          break;
        }
        if (ownerOk && keys.TSF !== '01') {
          uriBack = true;
          break;
        }
        await sleep(2000);
      }
      record({
        name: 'create_trustset_fail_uri_remit_ok',
        pass: uriBack,
        engine: uriBack ? 'ok' : 'timeout',
        gotMsg: uriBack ? 'URI back to seller AID clear' : JSON.stringify(await readAidKeys(client, host.classicAddress, aid)),
        want: { owner: 'seller', aid: 'cleared' },
      });
    }
  }

  /* ---- Global TAC assert (Create host local ns) ---- */
  {
    const createdOk = OUT.cases.filter((c) => {
      if (!(c.pass && c.engine === 'tesSUCCESS')) return false;
      const pool = [...(c.createMsgs || []), c.gotMsg || '', ...(c.gotMsgs || [])];
      return pool.some((m) => String(m).includes('Auction created'));
    }).length;
    const g = await readHostLocalKeys(client, host.classicAddress, NS_CREATE, ['TAC']);
    OUT.globals = { TAC: g.TAC || null, createdOk };
    const tac = g.TAC ? Buffer.from(g.TAC, 'hex').readUInt32BE(0) : 0;
    record({
      name: 'create_global_tac_bumped',
      pass: tac === createdOk && tac > 0,
      engine: 'ok',
      gotMsg: JSON.stringify({ tac, createdOk, raw: g.TAC || null }),
      want: { tac: createdOk },
    });
  }

  save();
  log('SUMMARY', JSON.stringify(OUT.summary));
  if (OUT.gaps.length) log('GAPS', JSON.stringify(OUT.gaps));
  await client.disconnect();
  process.exit(OUT.pass ? 0 : 1);
}

main().catch((e) => {
  console.error('FATAL', e);
  try {
    fs.writeFileSync(path.join(OUTDIR, 'IT_CREATE.json'), JSON.stringify({
      fatal: String(e?.stack || e),
      when: new Date().toISOString(),
      log: logLines,
    }, null, 2));
  } catch { /* ignore */ }
  process.exit(3);
});
