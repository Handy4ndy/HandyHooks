/**
 * Auction House V2 AuctionSub - full variation matrix (xahau.js).
 * Writes only IT_SUB.json next to this script.
 *
 * Run: node IT_SUB.js
 * Needs: npm i xahau  (or NODE_PATH to a folder that has it)
 */
import fs from 'fs';
import path from 'path';
import crypto from 'crypto';
import { fileURLToPath } from 'url';
import { Client, Wallet, decodeAccountID } from 'xahau';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const OUTDIR = __dirname;
const WS = process.env.XAHAU_WS || 'wss://xahau-test.net';
const FAUCET_URL = process.env.XAHAU_FAUCET || 'https://xahau-test.net/accounts';
const NETWORK_ID = 21338;

const WASM = fs.readFileSync(path.join(OUTDIR, 'AuctionSub.wasm'));
const ROOT = path.resolve(__dirname, '..');
const WASM_CREATE = fs.readFileSync(
  fs.existsSync(path.join(ROOT, 'Create', 'AuctionCreate.wasm'))
    ? path.join(ROOT, 'Create', 'AuctionCreate.wasm')
    : path.join(OUTDIR, 'AuctionCreate.wasm'),
);
const HOOK_HASH = crypto.createHash('sha512').update(WASM).digest().slice(0, 32).toString('hex').toUpperCase();
const NS = crypto.createHash('sha256').update('AuctionHouseV2Sub-full-' + Date.now()).digest().toString('hex').toUpperCase();
const NS_CREATE = crypto.createHash('sha256').update('AuctionHouseV2Create-c04-' + Date.now()).digest().toString('hex').toUpperCase();
const HOOK_ON = 'FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF7FFFFFFFFFFFFFFFFFFBFFFFE';
const HOOK_ON_CREATE = 'FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF77FFFFFFFFFFFFFFFFFBFFFFE';
const HSF_OVERRIDE = 1;
const DUR_S = 3600;

const PRICE = 10_000_000n;
const PERIOD = 3600;
const SPLIT_PCT = 50;
const AUCCAP = 5;
const MAX_SUBPRICE = 1_000_000_000_000n;
const SUBPERIOD_MIN = 60;
const SUBPERIOD_MAX = 86400 * 366;

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
    return { result: e.HookResult, code: e.HookReturnCode, msg, emit: e.HookEmitCount };
  });
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

async function mintUT(client, seller) {
  const uri = `aucv3c04:${Date.now()}:${Math.random().toString(16).slice(2)}`;
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

function hookMsgs(meta) {
  return decodeHr(meta).map((h) => h.msg || '');
}

async function readSellerState(client, hostAddr, sellerAddr) {
  /* Seller foreign ns = account ID (20) zero-padded to 32; keys SUBEXP/ACTIVE/CAP (right-aligned). */
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

function expectCase(name, r, want) {
  const hrs = decodeHr(r.meta);
  const msg = hrs[0]?.msg || '';
  const engineOk = want.engine === 'tesSUCCESS'
    ? r.engine === 'tesSUCCESS'
    : r.engine !== 'tesSUCCESS';
  const msgOk = want.msg == null ? true : msg === want.msg;
  return {
    name,
    pass: engineOk && msgOk,
    engine: r.engine,
    hash: r.hash,
    hook: hrs,
    want,
    gotMsg: msg,
  };
}

async function main() {
  const OUT = {
    when: new Date().toISOString(),
    hookHash: HOOK_HASH,
    wasmBytes: WASM.length,
    ns: NS,
    client: 'xahau.js',
    cases: [],
    summary: {},
  };
  const save = () => {
    OUT.log = logLines.slice();
    fs.writeFileSync(path.join(OUTDIR, 'IT_SUB.json'), JSON.stringify(OUT, null, 2));
  };
  const record = (c) => {
    OUT.cases.push(c);
    log((c.pass ? 'PASS' : 'FAIL'), c.name, c.engine, c.gotMsg || '', c.hash || '');
    save();
  };

  log('hash', HOOK_HASH, 'bytes', WASM.length);
  log('faucet bank...');
  const bank = await faucetWallet();
  log('bank', bank.classicAddress);
  OUT.bank = bank.classicAddress;
  await sleep(3000);

  const client = new Client(WS);
  await client.connect();

  for (let i = 0; i < 40; i++) {
    try {
      const b = await bal(client, bank.classicAddress);
      if (b > 80_000_000n) { OUT.bankBal = String(b); break; }
    } catch { /* wait */ }
    await sleep(1000);
  }

  const host = genWallet();
  const admin = genWallet();
  const treasury = genWallet();
  const seller = genWallet();
  const seller2 = genWallet();
  const stranger = genWallet();
  OUT.wallets = {
    host: host.classicAddress,
    admin: admin.classicAddress,
    treasury: treasury.classicAddress,
    seller: seller.classicAddress,
    seller2: seller2.classicAddress,
    stranger: stranger.classicAddress,
  };
  OUT.walletSeeds = {
    bank: { account: bank.classicAddress, seed: bank.seed },
    host: { account: host.classicAddress, seed: host.seed },
    admin: { account: admin.classicAddress, seed: admin.seed },
    treasury: { account: treasury.classicAddress, seed: treasury.seed },
    seller: { account: seller.classicAddress, seed: seller.seed },
    seller2: { account: seller2.classicAddress, seed: seller2.seed },
    stranger: { account: stranger.classicAddress, seed: stranger.seed },
  };

  log('funding...');
  await pay(client, bank, host, 250_000_000n);
  await pay(client, bank, admin, 40_000_000n);
  await pay(client, bank, treasury, 25_000_000n);
  await pay(client, bank, seller, 80_000_000n);
  await pay(client, bank, seller2, 40_000_000n);
  await pay(client, bank, stranger, 30_000_000n);

  {
    const r = await softSubmit(submitAndWait(client, host, {
      TransactionType: 'SetHook',
      Account: host.classicAddress,
      Hooks: [{
        Hook: {
          CreateCode: WASM.toString('hex').toUpperCase(),
          Flags: HSF_OVERRIDE,
          HookApiVersion: 0,
          HookNamespace: NS,
          HookOn: HOOK_ON,
          HookParameters: [hp('ADMIN', accHex(admin.classicAddress))],
        },
      }],
    }));
    record(expectCase('sethook', r, { engine: 'tesSUCCESS' }));
    if (r.engine !== 'tesSUCCESS') { OUT.pass = false; save(); await client.disconnect(); process.exit(2); }
  }

  record(expectCase('sub_before_settings', await subPay(client, seller, host, PRICE), {
    engine: 'reject', msg: 'Settings incomplete',
  }));
  record(expectCase('grant_before_settings', await invokeAdmin(client, admin, host, 'GRANT', accHex(seller2.classicAddress)), {
    engine: 'reject', msg: 'Settings incomplete',
  }));

  record(expectCase('subprice_bad_len', await invokeAdmin(client, admin, host, 'SUBPRICE', '01'), {
    engine: 'reject', msg: 'SUBPRICE must be 8 bytes',
  }));
  record(expectCase('subprice_zero', await invokeAdmin(client, admin, host, 'SUBPRICE', u64be(0n)), {
    engine: 'reject', msg: 'SUBPRICE must be > 0',
  }));
  record(expectCase('subprice_above_max', await invokeAdmin(client, admin, host, 'SUBPRICE', u64be(MAX_SUBPRICE + 1n)), {
    engine: 'reject', msg: 'SUBPRICE above maximum',
  }));
  record(expectCase('subperiod_bad_len', await invokeAdmin(client, admin, host, 'SUBPERIOD', '01'), {
    engine: 'reject', msg: 'SUBPERIOD must be 4 bytes',
  }));
  record(expectCase('subperiod_too_low', await invokeAdmin(client, admin, host, 'SUBPERIOD', u32be(SUBPERIOD_MIN - 1)), {
    engine: 'reject', msg: 'SUBPERIOD out of range',
  }));
  record(expectCase('subperiod_too_high', await invokeAdmin(client, admin, host, 'SUBPERIOD', u32be(SUBPERIOD_MAX + 1)), {
    engine: 'reject', msg: 'SUBPERIOD out of range',
  }));
  record(expectCase('subsplit_bad_len', await invokeAdmin(client, admin, host, 'SUBSPLIT', '01'), {
    engine: 'reject', msg: 'SUBSPLIT must be 2 bytes',
  }));
  record(expectCase('subsplit_over_100', await invokeAdmin(client, admin, host, 'SUBSPLIT', u16be(101)), {
    engine: 'reject', msg: 'SUBSPLIT must be 0..100',
  }));
  record(expectCase('auccap_bad_len', await invokeAdmin(client, admin, host, 'AUCCAP', '01'), {
    engine: 'reject', msg: 'AUCCAP must be 2 bytes',
  }));
  record(expectCase('auccap_zero', await invokeAdmin(client, admin, host, 'AUCCAP', u16be(0)), {
    engine: 'reject', msg: 'AUCCAP out of range',
  }));
  record(expectCase('auccap_too_high', await invokeAdmin(client, admin, host, 'AUCCAP', u16be(1001)), {
    engine: 'reject', msg: 'AUCCAP out of range',
  }));
  record(expectCase('treasury_bad_len', await invokeAdmin(client, admin, host, 'TREASURY', '01'), {
    engine: 'reject', msg: 'TREASURY must be 20 bytes',
  }));
  record(expectCase('treasury_is_host', await invokeAdmin(client, admin, host, 'TREASURY', accHex(host.classicAddress)), {
    engine: 'reject', msg: 'TREASURY must not be the host',
  }));
  record(expectCase('grant_bad_len', await invokeAdmin(client, admin, host, 'GRANT', '01'), {
    engine: 'reject', msg: 'GRANT must be 20 bytes (seller)',
  }));
  record(expectCase('revoke_bad_len', await invokeAdmin(client, admin, host, 'REVOKE', '01'), {
    engine: 'reject', msg: 'REVOKE must be 20 bytes',
  }));
  /* Non-admin Invoke with admin-looking params must pass through (not reject). */
  record(expectCase('non_admin_set', await invokeAdmin(client, stranger, host, 'SUBPRICE', u64be(PRICE)), {
    engine: 'tesSUCCESS', msg: 'Invoke passthrough',
  }));
  record(expectCase('invoke_passthrough', await softSubmit(submitAndWait(client, admin, {
    TransactionType: 'Invoke',
    Account: admin.classicAddress,
    Destination: host.classicAddress,
  })), { engine: 'tesSUCCESS', msg: 'Invoke passthrough' }));

  for (const [name, val, msg] of [
    ['SUBPRICE', u64be(PRICE), 'SUBPRICE updated'],
    ['SUBPERIOD', u32be(PERIOD), 'SUBPERIOD updated'],
    ['SUBSPLIT', u16be(SPLIT_PCT), 'SUBSPLIT updated'],
    ['AUCCAP', u16be(AUCCAP), 'AUCCAP updated'],
    ['TREASURY', accHex(treasury.classicAddress), 'TREASURY updated'],
  ]) {
    record(expectCase('set_' + name, await invokeAdmin(client, admin, host, name, val), {
      engine: 'tesSUCCESS', msg,
    }));
  }

  record(expectCase('set_SUBPERIOD_min', await invokeAdmin(client, admin, host, 'SUBPERIOD', u32be(SUBPERIOD_MIN)), {
    engine: 'tesSUCCESS', msg: 'SUBPERIOD updated',
  }));
  record(expectCase('set_SUBPERIOD_restore', await invokeAdmin(client, admin, host, 'SUBPERIOD', u32be(PERIOD)), {
    engine: 'tesSUCCESS', msg: 'SUBPERIOD updated',
  }));
  record(expectCase('set_SUBSPLIT_0', await invokeAdmin(client, admin, host, 'SUBSPLIT', u16be(0)), {
    engine: 'tesSUCCESS', msg: 'SUBSPLIT updated',
  }));
  record(expectCase('set_SUBSPLIT_100', await invokeAdmin(client, admin, host, 'SUBSPLIT', u16be(100)), {
    engine: 'tesSUCCESS', msg: 'SUBSPLIT updated',
  }));
  record(expectCase('set_SUBSPLIT_restore', await invokeAdmin(client, admin, host, 'SUBSPLIT', u16be(SPLIT_PCT)), {
    engine: 'tesSUCCESS', msg: 'SUBSPLIT updated',
  }));
  record(expectCase('set_AUCCAP_1', await invokeAdmin(client, admin, host, 'AUCCAP', u16be(1)), {
    engine: 'tesSUCCESS', msg: 'AUCCAP updated',
  }));
  record(expectCase('set_AUCCAP_1000', await invokeAdmin(client, admin, host, 'AUCCAP', u16be(1000)), {
    engine: 'tesSUCCESS', msg: 'AUCCAP updated',
  }));
  record(expectCase('set_AUCCAP_restore', await invokeAdmin(client, admin, host, 'AUCCAP', u16be(AUCCAP)), {
    engine: 'tesSUCCESS', msg: 'AUCCAP updated',
  }));

  record(expectCase('sub_wrong_amount_over', await subPay(client, seller, host, PRICE + 1n), {
    engine: 'reject', msg: 'Amount must equal SUBPRICE',
  }));
  record(expectCase('sub_wrong_amount_under', await subPay(client, seller, host, PRICE - 1n), {
    engine: 'reject', msg: 'Amount must equal SUBPRICE',
  }));
  record(expectCase('pay_no_sub', await softSubmit(submitAndWait(client, seller, {
    TransactionType: 'Payment',
    Account: seller.classicAddress,
    Destination: host.classicAddress,
    Amount: '1000000',
  })), { engine: 'tesSUCCESS', msg: 'Payment passthrough' }));

  /* KVT Finding 3: SUB + AID on same Payment -> reject (one purpose) */
  record(expectCase('sub_and_aid_both_set', await softSubmit(submitAndWait(client, seller, {
    TransactionType: 'Payment',
    Account: seller.classicAddress,
    Destination: host.classicAddress,
    Amount: String(PRICE),
    HookParameters: [hp('SUB', '01'), hp('AID', 'AA'.repeat(32))],
  })), { engine: 'reject', msg: 'SUB and AID both set' }));

  {
    const iouCode = 'USD';
    await softSubmit(submitAndWait(client, seller, {
      TransactionType: 'TrustSet',
      Account: seller.classicAddress,
      LimitAmount: { currency: iouCode, issuer: stranger.classicAddress, value: '1000' },
    }));
    await softSubmit(submitAndWait(client, stranger, {
      TransactionType: 'Payment',
      Account: stranger.classicAddress,
      Destination: seller.classicAddress,
      Amount: { currency: iouCode, issuer: stranger.classicAddress, value: '50' },
    }));
    const r = await softSubmit(submitAndWait(client, seller, {
      TransactionType: 'Payment',
      Account: seller.classicAddress,
      Destination: host.classicAddress,
      Amount: { currency: iouCode, issuer: stranger.classicAddress, value: '1' },
      HookParameters: [hp('SUB', '01')],
    }));
    record(expectCase('sub_iou_rejected', r, { engine: 'reject', msg: 'Subscription must be XAH' }));
  }

  record(expectCase('host_outgoing', await softSubmit(submitAndWait(client, host, {
    TransactionType: 'Payment',
    Account: host.classicAddress,
    Destination: stranger.classicAddress,
    Amount: '1000000',
  })), { engine: 'tesSUCCESS', msg: 'Outgoing ok' }));

  const tresBefore = await bal(client, treasury.classicAddress);
  const hostBefore = await bal(client, host.classicAddress);
  record(expectCase('sub_ok', await subPay(client, seller, host, PRICE), {
    engine: 'tesSUCCESS', msg: 'Subscription active',
  }));
  await sleep(6000);
  const tresAfter = await bal(client, treasury.classicAddress);
  const expectedSplit = (PRICE * BigInt(SPLIT_PCT)) / 100n;
  const delta = tresAfter - tresBefore;
  const splitPass = delta === expectedSplit;
  OUT.balances = {
    tresBefore: String(tresBefore),
    tresAfter: String(tresAfter),
    tresDelta: String(delta),
    expectedSplit: String(expectedSplit),
    hostBefore: String(hostBefore),
    hostAfter: String(await bal(client, host.classicAddress)),
  };
  log('treasury_split', String(delta), 'expected', String(expectedSplit), splitPass ? 'PASS' : 'FAIL');

  let emitProof = { pass: false };
  {
    const at = await client.request({
      command: 'account_tx', account: treasury.classicAddress,
      ledger_index_min: -1, ledger_index_max: -1, limit: 10,
    });
    let found = null;
    for (const t of at.result.transactions || []) {
      const txj = t.tx || t.tx_json || t;
      if (txj.TransactionType === 'Payment' && txj.Account === host.classicAddress) {
        found = txj; break;
      }
    }
    if (found) {
      const memos = found.Memos || [];
      let type = '', data = '';
      if (memos[0]?.Memo) {
        type = Buffer.from(memos[0].Memo.MemoType || '', 'hex').toString('utf8');
        data = Buffer.from(memos[0].Memo.MemoData || '', 'hex').toString('utf8');
      }
      const txr = await client.request({ command: 'tx', transaction: found.hash });
      const hrs = decodeHr(txr.result.meta || {});
      const cbak = (hrs[0]?.msg || '').replace(/\0+$/, '');
      emitProof = {
        hash: found.hash,
        memoType: type,
        memoData: data,
        noSourceTag: !('SourceTag' in found),
        noDestinationTag: !('DestinationTag' in found),
        cbak,
        pass: type === 'Note' && data === 'Treasury split'
          && !('SourceTag' in found) && !('DestinationTag' in found)
          && cbak === 'Auction subscription successful',
      };
    }
    OUT.emit = emitProof;
    log('emit_proof', emitProof.pass ? 'PASS' : 'FAIL', JSON.stringify(emitProof));
    record({
      name: 'emit_memo_note_cbak_notags',
      pass: !!emitProof.pass,
      engine: emitProof.pass ? 'tesSUCCESS' : 'fail',
      hash: emitProof.hash,
      hook: [],
      want: { memo: 'Note/Treasury split', cbak: 'Auction subscription successful', tags: false },
      gotMsg: JSON.stringify(emitProof),
    });
  }

  const sellerState1 = parseSeller(await readSellerState(client, host.classicAddress, seller.classicAddress));
  OUT.sellerStateAfterSub = sellerState1;
  log('sellerState', JSON.stringify(sellerState1));
  record({
    name: 'seller_state_after_sub',
    pass: !!sellerState1 && sellerState1.cap === AUCCAP && sellerState1.active === 0,
    engine: sellerState1 ? 'ok' : 'missing',
    hash: '',
    hook: [],
    want: { cap: AUCCAP, active: 0 },
    gotMsg: JSON.stringify(sellerState1),
  });

  {
    const before = sellerState1?.subexp ? BigInt(sellerState1.subexp) : 0n;
    record(expectCase('sub_renew', await subPay(client, seller, host, PRICE), {
      engine: 'tesSUCCESS', msg: 'Subscription active',
    }));
    await sleep(2000);
    const after = parseSeller(await readSellerState(client, host.classicAddress, seller.classicAddress));
    OUT.sellerStateAfterRenew = after;
    const extended = after && BigInt(after.subexp) >= before + BigInt(PERIOD);
    record({
      name: 'sub_renew_extends_expiry',
      pass: !!extended,
      engine: extended ? 'ok' : 'fail',
      hash: '',
      hook: [],
      want: { extendBy: PERIOD },
      gotMsg: JSON.stringify({ before: String(before), after: after?.subexp }),
    });
  }

  record(expectCase('grant', await invokeAdmin(client, admin, host, 'GRANT', accHex(seller2.classicAddress)), {
    engine: 'tesSUCCESS', msg: 'GRANT applied',
  }));
  {
    const st = parseSeller(await readSellerState(client, host.classicAddress, seller2.classicAddress));
    OUT.seller2AfterGrant = st;
    record({
      name: 'grant_writes_seller_state',
      pass: !!st && st.cap === AUCCAP,
      engine: st ? 'ok' : 'missing',
      hash: '',
      hook: [],
      want: { cap: AUCCAP },
      gotMsg: JSON.stringify(st),
    });
  }
  record(expectCase('revoke', await invokeAdmin(client, admin, host, 'REVOKE', accHex(seller2.classicAddress)), {
    engine: 'tesSUCCESS', msg: 'REVOKE applied',
  }));
  {
    const st = await readSellerState(client, host.classicAddress, seller2.classicAddress);
    const cleared = !st || (!st.SUBEXP && !st.ACTIVE && !st.CAP);
    record({
      name: 'revoke_clears_seller_state',
      pass: cleared,
      engine: cleared ? 'cleared' : 'still_present',
      hash: '',
      hook: [],
      want: { cleared: true, keys: ['SUBEXP', 'ACTIVE', 'CAP'] },
      gotMsg: JSON.stringify(st),
    });
  }
  record(expectCase('revoke_missing_seller', await invokeAdmin(client, admin, host, 'REVOKE', accHex(stranger.classicAddress)), {
    engine: 'tesSUCCESS', msg: 'REVOKE applied',
  }));

  /* ---- C04: REVOKE blocked while ACTIVE > 0 (needs Create on host) ---- */
  {
    const r = await softSubmit(submitAndWait(client, host, {
      TransactionType: 'SetHook',
      Account: host.classicAddress,
      Hooks: [
        {
          Hook: {
            CreateCode: WASM.toString('hex').toUpperCase(),
            Flags: HSF_OVERRIDE,
            HookApiVersion: 0,
            HookNamespace: NS,
            HookOn: HOOK_ON,
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
    record(expectCase('c04_sethook_sub_create', r, { engine: 'tesSUCCESS' }));
    if (r.engine !== 'tesSUCCESS') {
      OUT.c04_blocker = 'SetHook Sub+Create failed: ' + r.engine;
    } else {
      const lot = await mintUT(client, seller);
      const cr = await createRemit(client, seller, host, lot, { DUR: u64be(DUR_S) });
      const crMsgs = hookMsgs(cr.meta);
      const created = cr.engine === 'tesSUCCESS' && crMsgs.some((m) => /Auction created/i.test(m));
      record({
        name: 'c04_create_one_auction',
        pass: created,
        engine: cr.engine,
        hash: cr.hash,
        hook: decodeHr(cr.meta),
        want: { msgIncludes: 'Auction created' },
        gotMsg: crMsgs.join(' | '),
      });
      const stActive = parseSeller(await readSellerState(client, host.classicAddress, seller.classicAddress));
      OUT.c04_sellerAfterCreate = stActive;
      record({
        name: 'c04_active_bumped',
        pass: !!stActive && Number(stActive.active) >= 1,
        engine: stActive ? 'ok' : 'missing',
        hash: '',
        hook: [],
        want: { active: '>=1' },
        gotMsg: JSON.stringify(stActive),
      });
      const rev = await invokeAdmin(client, admin, host, 'REVOKE', accHex(seller.classicAddress));
      const revMsgs = hookMsgs(rev.meta);
      const nope = rev.engine !== 'tesSUCCESS' && revMsgs.some((m) => m === 'ACTIVE auctions open');
      record({
        name: 'revoke_while_active_nope',
        pass: nope,
        engine: rev.engine,
        hash: rev.hash,
        hook: decodeHr(rev.meta),
        want: { engine: 'reject', msg: 'ACTIVE auctions open' },
        gotMsg: revMsgs.join(' | '),
      });
      const stStill = parseSeller(await readSellerState(client, host.classicAddress, seller.classicAddress));
      record({
        name: 'revoke_while_active_state_intact',
        pass: !!stStill && !!stStill.subexp && Number(stStill.active) >= 1 && stStill.cap != null,
        engine: stStill ? 'ok' : 'missing',
        hash: '',
        hook: [],
        want: { SUBEXP: 1, ACTIVE: '>=1', CAP: 1 },
        gotMsg: JSON.stringify(stStill),
      });
      /* Finalise not in Sub harness - prove ACTIVE==0 path (GRANT sets active 0). */
      OUT.c04_note = 'revoke_after_finalise_ok: Finalise not installed in Sub integration harness; used ACTIVE=0 GRANT path';
      const gr = await invokeAdmin(client, admin, host, 'GRANT', accHex(seller2.classicAddress));
      record(expectCase('c04_grant_seller2_active0', gr, { engine: 'tesSUCCESS', msg: 'GRANT applied' }));
      const st2 = parseSeller(await readSellerState(client, host.classicAddress, seller2.classicAddress));
      record({
        name: 'c04_seller2_active_zero',
        pass: !!st2 && Number(st2.active) === 0,
        engine: st2 ? 'ok' : 'missing',
        hash: '',
        hook: [],
        want: { active: 0 },
        gotMsg: JSON.stringify(st2),
      });
      const rev0 = await invokeAdmin(client, admin, host, 'REVOKE', accHex(seller2.classicAddress));
      record(expectCase('revoke_after_finalise_ok', rev0, {
        engine: 'tesSUCCESS', msg: 'REVOKE applied',
      }));
      const st2c = await readSellerState(client, host.classicAddress, seller2.classicAddress);
      const cleared2 = !st2c || (!st2c.SUBEXP && !st2c.ACTIVE && !st2c.CAP);
      record({
        name: 'revoke_after_active_zero_clears',
        pass: cleared2,
        engine: cleared2 ? 'cleared' : 'still_present',
        hash: '',
        hook: [],
        want: { cleared: true },
        gotMsg: JSON.stringify(st2c),
      });
    }
  }

  {
    await invokeAdmin(client, admin, host, 'SUBSPLIT', u16be(0));
    const t0 = await bal(client, treasury.classicAddress);
    record(expectCase('sub_split_0_pct', await subPay(client, seller, host, PRICE), {
      engine: 'tesSUCCESS', msg: 'Subscription active',
    }));
    await sleep(5000);
    const t1 = await bal(client, treasury.classicAddress);
    const noEmit = t1 === t0;
    OUT.split0 = { before: String(t0), after: String(t1) };
    record({
      name: 'sub_split_0_no_treasury_credit',
      pass: noEmit,
      engine: noEmit ? 'ok' : 'unexpected_credit',
      hash: '',
      hook: [],
      want: { delta: 0 },
      gotMsg: String(t1 - t0),
    });
  }

  {
    await invokeAdmin(client, admin, host, 'SUBSPLIT', u16be(100));
    const t0 = await bal(client, treasury.classicAddress);
    record(expectCase('sub_split_100_pct', await subPay(client, seller, host, PRICE), {
      engine: 'tesSUCCESS', msg: 'Subscription active',
    }));
    await sleep(6000);
    const t1 = await bal(client, treasury.classicAddress);
    const ok = (t1 - t0) === PRICE;
    OUT.split100 = { before: String(t0), after: String(t1), delta: String(t1 - t0) };
    record({
      name: 'sub_split_100_full_to_treasury',
      pass: ok,
      engine: ok ? 'ok' : 'wrong_delta',
      hash: '',
      hook: [],
      want: { delta: String(PRICE) },
      gotMsg: String(t1 - t0),
    });
    await invokeAdmin(client, admin, host, 'SUBSPLIT', u16be(SPLIT_PCT));
  }

  const failed = OUT.cases.filter((c) => !c.pass);
  OUT.summary = {
    total: OUT.cases.length,
    passed: OUT.cases.length - failed.length,
    failed: failed.length,
    failedNames: failed.map((c) => c.name),
    treasurySplitOk: splitPass,
  };
  OUT.pass = failed.length === 0 && splitPass;
  save();
  log('SUMMARY', JSON.stringify(OUT.summary));
  log('OVERALL', OUT.pass ? 'PASS' : 'FAIL');
  await client.disconnect();
  process.exit(OUT.pass ? 0 : 3);
}

main().catch((e) => {
  console.error(e);
  try {
    fs.writeFileSync(path.join(OUTDIR, 'IT_SUB.json'), JSON.stringify({
      error: String(e && e.stack || e),
      log: logLines.slice(),
    }, null, 2));
  } catch { /* ignore */ }
  process.exit(1);
});
