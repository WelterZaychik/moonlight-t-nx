import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {Readable} from 'node:stream';
import {pipeline} from 'node:stream/promises';
import {spawnSync} from 'node:child_process';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const dependencies = JSON.parse(fs.readFileSync(path.join(root,'tool/ns1/dependencies.json')));
const downloads = path.join(root,'downloads');
fs.mkdirSync(downloads,{recursive:true});
const digest = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
for (const dependency of dependencies) {
  const archive = path.join(downloads,dependency.archive);
  if (!fs.existsSync(archive) || digest(archive) !== dependency.sha256) {
    const temporary = archive + '.part';
    const response = await fetch(`https://codeload.github.com/${dependency.repository}/tar.gz/${dependency.revision}`);
    if (!response.ok) throw Error(`${dependency.repository}: HTTP ${response.status}`);
    await pipeline(Readable.fromWeb(response.body),fs.createWriteStream(temporary));
    if (digest(temporary) !== dependency.sha256) throw Error(`${dependency.repository}: archive hash mismatch`);
    fs.renameSync(temporary,archive);
  }
  const destination = path.resolve(root,dependency.directory);
  if (!destination.startsWith(root + path.sep)) throw Error('Dependency destination escapes project');
  const marker = path.join(destination,'.moonlight-source-revision');
  if (!fs.existsSync(marker) || fs.readFileSync(marker,'utf8').trim() !== dependency.revision) {
    fs.mkdirSync(destination,{recursive:true});
    // Android-only symlinks cannot be created by ordinary Windows tar. They
    // are irrelevant to this NS1 dependency bootstrap.
    const result = spawnSync('tar',['-xzf',archive,'--strip-components','1','--exclude=*/android-project/*','-C',destination],{stdio:'inherit'});
    if (result.status !== 0) throw Error(`${dependency.repository}: extraction failed`);
    fs.writeFileSync(marker,dependency.revision + '\n');
  }
  console.log(`${dependency.repository}@${dependency.revision}: SHA-256 verified`);
}
