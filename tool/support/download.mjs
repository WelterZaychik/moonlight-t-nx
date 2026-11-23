import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {Readable} from 'node:stream';
import {pipeline} from 'node:stream/promises';
const [url, target, expected] = process.argv.slice(2);
if (!url || !target) throw Error('Usage: download.mjs URL target [sha256]');
fs.mkdirSync(path.dirname(target),{recursive:true});
for (let attempt = 0; attempt < 8; attempt++) {
  try {
    const start = fs.existsSync(target) ? fs.statSync(target).size : 0;
    const response = await fetch(url,{headers:start ? {Range:`bytes=${start}-`} : {}});
    if (response.status === 416 && start) break;
    if (!response.ok) throw Error(`HTTP ${response.status}: ${url}`);
    const append = start && response.status === 206;
    console.log(path.basename(target),response.status,response.headers.get('content-range') || response.headers.get('content-length'));
    await pipeline(Readable.fromWeb(response.body),fs.createWriteStream(target,{flags:append ? 'a':'w'}));
    break;
  } catch (error) { if (attempt === 7) throw error; console.log('Retry:',error.message); }
}
const sha256 = crypto.createHash('sha256').update(fs.readFileSync(target)).digest('hex');
if (expected && sha256 !== expected.toLowerCase()) throw Error('SHA-256 mismatch: '+target);
fs.writeFileSync(target + '.source.json',JSON.stringify({url,sha256,bytes:fs.statSync(target).size},null,2));
console.log('Complete:',target,sha256);
