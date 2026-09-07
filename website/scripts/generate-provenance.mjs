import { execFileSync } from 'node:child_process';
import { readFileSync, writeFileSync } from 'node:fs';
const pkg=JSON.parse(readFileSync(new URL('../package.json',import.meta.url),'utf8'));
let sourceCommit=null;
try{sourceCommit=execFileSync('git',['rev-parse','--verify','HEAD'],{encoding:'utf8',stdio:['ignore','pipe','ignore']}).trim();}catch{}
const data=JSON.stringify({schemaVersion:1,version:pkg.version,sourceCommit,updatedAt:new Date().toISOString(),scope:'website'},null,2)+'\n';
writeFileSync(new URL('../app/generated-build.json',import.meta.url),data);
writeFileSync(new URL('../public/provenance.json',import.meta.url),data);
