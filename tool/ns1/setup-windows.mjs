import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
process.chdir(root);
if(process.platform!=='win32')throw Error('This bootstrap is for Windows x64.');
const run=(command,args,options={})=>{const r=spawnSync(command,args,{stdio:'inherit',...options});if(r.error)throw r.error;if(r.status!==0)throw Error(`${command} failed (${r.status})`);};
const entries=JSON.parse(fs.readFileSync('tool/ns1/windows-tools.lock.json','utf8'));
for(const entry of entries){
 const target='tool/downloads/'+entry.file;
 const valid=fs.existsSync(target)&&crypto.createHash('sha256').update(fs.readFileSync(target)).digest('hex')===entry.sha256;
 if(!valid)run(process.execPath,['tool/support/download.mjs',entry.url,target,entry.sha256]);
 let destination;
 if(entry.kind==='sdk')destination='tool/devkitpro';
 else if(entry.kind!=='rustup')destination='tool/'+entry.kind;
 if(destination){fs.mkdirSync(destination,{recursive:true});run('tar',entry.kind==='sdk'?['-xf',target,'--strip-components','2','-C',destination,'opt/devkitpro/']:['-xf',target,'-C',destination]);}
}
// devkitPro's upstream CMake entry point requires MSYS CMake. The native
// Ninja build supplies absolute paths and needs no MSYS path translation.
const initializer='tool/devkitpro/cmake/dkp-initialize-path.cmake';
let text=fs.readFileSync(initializer,'utf8');
text=text.replace(/if\(CMAKE_HOST_WIN32\)[\s\S]*?endif\(\)/,'# Project-local native Windows CMake / Ninja adaptation.');
fs.writeFileSync(initializer,text);
const bin='tool/devkitpro/portlibs/switch/bin';fs.mkdirSync(bin,{recursive:true});
fs.copyFileSync('tool/pkgconf/ucrt64/bin/pkgconf.exe',bin+'/aarch64-none-elf-pkg-config.exe');
for(const name of fs.readdirSync('tool/pkgconf/ucrt64/bin').filter(f=>f.endsWith('.dll')))fs.copyFileSync('tool/pkgconf/ucrt64/bin/'+name,bin+'/'+name);
if(!process.argv.includes('--sdk-only')){
 const cache=path.join(root,'tool/cache');fs.mkdirSync(path.join(cache,'tmp'),{recursive:true});
 const env={...process.env,CARGO_HOME:path.join(cache,'cargo'),RUSTUP_HOME:path.join(cache,'rustup'),TEMP:path.join(cache,'tmp'),TMP:path.join(cache,'tmp')};
 // Mirrors retain official Rustup checksums; rustup validates components.
 if(process.argv.includes('--rust-mirror')){env.RUSTUP_DIST_SERVER='https://rsproxy.cn';env.RUSTUP_UPDATE_ROOT='https://rsproxy.cn/rustup';}
 run(path.join(root,'tool/downloads/rustup-init-mirror.exe'),['-y','--no-modify-path','--profile','minimal','--default-host','x86_64-pc-windows-gnu','--default-toolchain','nightly-2026-09-25','--component','rust-src'],{env});
}
console.log('Portable tools ready in '+path.join(root,'tool'));
