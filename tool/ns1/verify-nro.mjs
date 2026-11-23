import fs from 'node:fs';import crypto from 'node:crypto';import path from 'node:path';import {spawnSync} from 'node:child_process';
const root=process.cwd(), nro=fs.readFileSync('dist/Moonlight-T-NX.nro'),elf=fs.readFileSync('build/ns1/Moonlight.elf');
function check(v,label){if(!v)throw Error(label);}
check(nro.toString('ascii',0x10,0x14)==='NRO0','NRO0 header');check(elf.readUInt16LE(0x12)===183,'AArch64 ELF');
const imageSize=nro.readUInt32LE(0x18);check(nro.toString('ascii',imageSize,imageSize+4)==='ASET','NRO assets');
const u64=(buffer,offset)=>Number(buffer.readBigUInt64LE(offset));
const nacpOffset=imageSize+u64(nro,imageSize+0x18),nacpSize=u64(nro,imageSize+0x20);
check(nacpSize===0x4000,'NACP size');const title=nro.toString('utf8',nacpOffset,nacpOffset+0x200).split('\0')[0];check(title==='Moonlight-T-NX','NACP title');
const romfs=nro.subarray(imageSize+u64(nro,imageSize+0x28),imageSize+u64(nro,imageSize+0x28)+u64(nro,imageSize+0x30));
check(u64(romfs,0)===0x50,'RomFS header');const dirBase=u64(romfs,0x18),fileBase=u64(romfs,0x38),dataBase=u64(romfs,0x48);const files=new Map();
function walk(offset,parent=''){const base=dirBase+offset;const name=romfs.toString('utf8',base+0x18,base+0x18+romfs.readUInt32LE(base+0x14));const prefix=parent+(name?name+'/':'');let f=romfs.readUInt32LE(base+12);while(f!==0xffffffff){const b=fileBase+f;const n=romfs.toString('utf8',b+0x20,b+0x20+romfs.readUInt32LE(b+0x1c));const start=dataBase+u64(romfs,b+8);files.set(prefix+n,romfs.subarray(start,start+u64(romfs,b+16)));f=romfs.readUInt32LE(b+4);}let d=romfs.readUInt32LE(base+8);while(d!==0xffffffff){walk(d,prefix);d=romfs.readUInt32LE(dirBase+d+4);}}
walk(0);
for(const f of ['i18n/zh-Hans/main.json','xml/tabs/tailscale.xml'])check(files.has(f)&&files.get(f).equals(fs.readFileSync('resources/'+f)),'Embedded resource '+f);
const language=JSON.parse(files.get('i18n/zh-Hans/main.json'));check(language.settings.deadzone.title==='摇杆死区','Chinese settings');check(files.get('xml/tabs/tailscale.xml').toString().includes('扫描二维码登录'),'Chinese Tailscale panel');
const nmTool=path.join(process.env.DEVKITPRO||path.join(root,'tool/devkitpro'),'devkitA64/bin/aarch64-none-elf-nm'+(process.platform==='win32'?'.exe':''));
const nm=spawnSync(nmTool,['-u','build/ns1/Moonlight.elf'],{encoding:'utf8'});check(nm.status===0&&!nm.stdout.trim(),'No unresolved ELF symbols');
const symbols=spawnSync(nmTool,['--defined-only','build/ns1/Moonlight.elf'],{encoding:'utf8',maxBuffer:16000000});for(const symbol of ['tsnx_engine_new','__wrap_connect','__wrap_sendmsg'])check(symbols.stdout.includes(symbol),'Embedded symbol '+symbol);
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
const report={project:'Moonlight-T-NX NS1',builtAt:new Date().toISOString(),language:'zh-Hans',nroBytes:nro.length,nroSha256:hash(nro),elfMachine:'AArch64',elfUnresolvedSymbols:0,nroHeader:'NRO0',nacpTitle:title,romfsFiles:files.size,verifiedEmbeddedChineseResources:true,embeddedTailscale:true,hardwareTested:false};
fs.writeFileSync('build/validation/nro-result.json',JSON.stringify(report,null,2)+'\n');fs.writeFileSync('dist/build-info.json',JSON.stringify(report,null,2)+'\n');
fs.writeFileSync('dist/SHA256SUMS',['Moonlight-T-NX.nro','Moonlight-T-NX-NS1.zip'].map(f=>hash(fs.readFileSync('dist/'+f))+'  '+f).join('\n')+'\n');
console.log(JSON.stringify(report,null,2));
