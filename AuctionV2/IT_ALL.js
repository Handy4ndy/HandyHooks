/**
 * Auction House V2 — ONE runner for everything (integration tests).
 * Sequentially runs:
 *   1) Subscription/IT_SUB.js
 *   2) Create/IT_CREATE.js
 *   3) Bids/IT_BIDS.js
 *   4) Finalise/IT_FINALISE.js  (includes shared-NS combined chain cases)
 * Aggregates PASS/FAIL + HookHashes into IT_ALL.json.
 *
 * Run from AuctionV2: node IT_ALL.js
 */
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import { spawn } from 'child_process';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const ROOT = __dirname;

const SUITES = [
  { name: 'Subscription', cwd: path.join(ROOT, 'Subscription'), script: 'IT_SUB.js', json: 'IT_SUB.json' },
  { name: 'Create', cwd: path.join(ROOT, 'Create'), script: 'IT_CREATE.js', json: 'IT_CREATE.json' },
  { name: 'Bids', cwd: path.join(ROOT, 'Bids'), script: 'IT_BIDS.js', json: 'IT_BIDS.json' },
  { name: 'Finalise', cwd: path.join(ROOT, 'Finalise'), script: 'IT_FINALISE.js', json: 'IT_FINALISE.json' },
];

function runNode(cwd, script) {
  return new Promise((resolve) => {
    const start = Date.now();
    console.log('\n========== RUN', script, 'in', cwd, '==========\n');
    const child = spawn(process.execPath, [script], {
      cwd,
      stdio: ['ignore', 'pipe', 'pipe'],
      env: { ...process.env },
    });
    let out = '';
    let err = '';
    child.stdout.on('data', (d) => {
      const s = d.toString();
      out += s;
      process.stdout.write(s);
    });
    child.stderr.on('data', (d) => {
      const s = d.toString();
      err += s;
      process.stderr.write(s);
    });
    child.on('close', (code) => {
      resolve({
        code: code ?? 1,
        ms: Date.now() - start,
        outTail: out.slice(-4000),
        errTail: err.slice(-2000),
      });
    });
  });
}

function loadJson(p) {
  try {
    return JSON.parse(fs.readFileSync(p, 'utf8'));
  } catch {
    return null;
  }
}

async function main() {
  const OUT = {
    when: new Date().toISOString(),
    note: 'Runs every individual integration suite then Finalise (shared host/NS + full Finalise matrix / combined chain).',
    suites: [],
    pass: 0,
    fail: 0,
    hook_hashes: {},
  };

  for (const s of SUITES) {
    const scriptPath = path.join(s.cwd, s.script);
    if (!fs.existsSync(scriptPath)) {
      OUT.suites.push({ name: s.name, error: 'missing ' + scriptPath, pass: 0, fail: 1 });
      OUT.fail += 1;
      continue;
    }
    const run = await runNode(s.cwd, s.script);
    const jpath = path.join(s.cwd, s.json);
    const j = loadJson(jpath);
    const raw = j?.summary || {};
    let passN = Number(raw.pass ?? raw.passed ?? j?.pass ?? 0);
    let failN = Number(raw.fail ?? raw.failed ?? j?.fail ?? (run.code === 0 ? 0 : 1));
    if (Array.isArray(j?.cases)) {
      passN = j.cases.filter((c) => c.pass).length;
      failN = j.cases.filter((c) => !c.pass).length;
    }
    const summary = {
      ...raw,
      pass: passN,
      fail: failN,
      total: Number(raw.total ?? (passN + failN)),
    };
    const entry = {
      name: s.name,
      script: s.script,
      exitCode: run.code,
      ms: run.ms,
      summary,
      hook_hashes: j?.hook_hashes || null,
      blocker: j?.blocker || null,
      error: j?.error || null,
    };
    OUT.suites.push(entry);
    if (j?.hook_hashes) Object.assign(OUT.hook_hashes, j.hook_hashes);
    OUT.pass += passN;
    OUT.fail += failN;
    if (run.code !== 0 && failN === 0) OUT.fail += 1;
    console.log('\n-----', s.name, 'done exit', run.code, 'summary', JSON.stringify(summary), '-----\n');
  }

  OUT.summary = {
    pass: OUT.pass,
    fail: OUT.fail,
    total: OUT.pass + OUT.fail,
    suiteExitFails: OUT.suites.filter((s) => s.exitCode !== 0).map((s) => s.name),
  };

  const outPath = path.join(ROOT, 'IT_ALL.json');
  fs.writeFileSync(outPath, JSON.stringify(OUT, null, 2));
  /* also write a short run log */
  fs.writeFileSync(path.join(ROOT, 'IT_ALL.run.log'), [
    `when ${OUT.when}`,
    ...OUT.suites.map((s) => `${s.name}: exit=${s.exitCode} pass=${s.summary?.pass} fail=${s.summary?.fail} ms=${s.ms}`),
    `TOTAL pass=${OUT.pass} fail=${OUT.fail}`,
    `hashes ${JSON.stringify(OUT.hook_hashes)}`,
  ].join('\n') + '\n');

  console.log('\n========== IT_ALL SUMMARY ==========');
  console.log(JSON.stringify(OUT.summary, null, 2));
  console.log('HookHashes', JSON.stringify(OUT.hook_hashes, null, 2));
  console.log('wrote', outPath);
  process.exit(OUT.fail > 0 || OUT.summary.suiteExitFails.length ? 1 : 0);
}

main().catch((e) => {
  console.error(e);
  fs.writeFileSync(path.join(ROOT, 'IT_ALL.json'), JSON.stringify({ error: String(e?.stack || e) }, null, 2));
  process.exit(2);
});
