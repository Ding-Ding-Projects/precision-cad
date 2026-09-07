import { execFileSync } from 'node:child_process';
import { existsSync } from 'node:fs';
import { homedir, platform } from 'node:os';
import { resolve, extname } from 'node:path';
import { pathToFileURL } from 'node:url';

// The private rules stay outside this public repository. No dictionary or digest is embedded.
const root=resolve(import.meta.dirname,'..');
let privateRoot=process.env.PRIVATE_INSTRUCTIONS_ROOT;
if(!privateRoot && platform()==='win32'){
 try{const documents=execFileSync('powershell.exe',['-NoProfile','-Command',"[Environment]::GetFolderPath('MyDocuments')"],{encoding:'utf8',timeout:5000,stdio:['ignore','pipe','ignore']}).trim();privateRoot=resolve(documents,'GitHub','agent-global-memory');}catch{}
}
privateRoot ||= resolve(homedir(),'Documents','GitHub','agent-global-memory');
const scannerPath=resolve(privateRoot,'scripts/private-vocabulary-markdown.mjs');
if(!existsSync(scannerPath)){
 console.log('Publication terminology check skipped: no private rules are available for this contributor.');
 process.exit(0);
}
const {findPrivateVocabularyLeak}=await import(pathToFileURL(scannerPath).href);
if(typeof findPrivateVocabularyLeak!=='function')throw new Error('Private publication scanner is incompatible.');
const ref=process.argv[2]||'HEAD';
if(ref.startsWith('-'))throw new Error('Invalid source revision.');
const git=args=>execFileSync('git',args,{cwd:root,encoding:'utf8',timeout:20000,maxBuffer:16*1024*1024,stdio:['ignore','pipe','pipe']});
const sha=git(['rev-parse','--verify',`${ref}^{commit}`]).trim();
const extensions=new Set(['.md','.cpp','.h','.hpp','.qml','.mjs','.js','.ts','.tsx','.ps1','.bat','.yml','.yaml','.json','.css','.svg']);
const paths=git(['ls-tree','-r','--name-only','-z',sha]).split('\0').filter(p=>p&&extensions.has(extname(p)));
const rejected=[];
for(const path of paths){
 const body=git(['show',`${sha}:${path}`]);
 const count=findPrivateVocabularyLeak(body).length;
 if(count)rejected.push({path,matches:count});
}
// Report locations only, never repeat the private terms that were found.
console.log(JSON.stringify({sourceCommit:sha,filesChecked:paths.length,rejected},null,2));
process.exitCode=rejected.length?1:0;
