import {existsSync, readFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {resolve, relative, isAbsolute} from 'node:path';
import {fileURLToPath} from 'node:url';

// This list is deliberately hand-written. Adding a capability requires changing this contract and its tests.
export const requiredCapabilityIds=Object.freeze([
 'CAD-DOC-001','CAD-SAVE-001','CAD-GEOMETRY-001','CAD-VIEWPORT-001','CAD-HISTORY-001',
 'CAD-SKETCH-001','CAD-SOLID-001','CAD-REF-001','CAD-ASSEMBLY-001','CAD-DRAWING-001','CAD-SHEET-001','CAD-EXCHANGE-001',
 'CAD-CAM-001','CAD-CAM-002','CAD-MESH-001','CAD-ANALYSIS-001','CAD-ANALYSIS-002','CAD-SCRIPT-001','CAD-SURFACE-001','CAD-RELEASE-001'
]);
export const bracketWorkflowIds=Object.freeze(['CAD-SKETCH-001','CAD-SOLID-001','CAD-REF-001','CAD-DRAWING-001','CAD-EXCHANGE-001']);
const sha256=value=>createHash('sha256').update(value).digest('hex');
const sha256File=path=>sha256(readFileSync(path));
const hex40=/^[0-9a-f]{40}$/u;
const hex64=/^[0-9a-f]{64}$/u;
const onlyKeys=(value,keys,label,errors)=>{
 for(const key of Object.keys(value||{}))if(!keys.includes(key))errors.push(`Unexpected ${label} field: ${key}`);
};
const safeLocalPath=(root,path)=>{
 if(typeof path!=='string'||!path)return null;
 const absolute=resolve(root,path);
 return isAbsolute(path)||relative(root,absolute).startsWith('..')||relative(root,absolute)===''?null:absolute;
};
const requiredString=(value,name,errors)=>{if(typeof value!=='string'||!value.trim())errors.push(`Missing ${name}`);};
const evidenceFile=(root,path,expected,name,errors)=>{
 const absolute=safeLocalPath(root,path);
 if(!absolute){errors.push(`Unsafe ${name} path: ${path}`);return;}
 if(!existsSync(absolute)){errors.push(`Missing ${name}: ${path}`);return;}
 if(!hex64.test(expected||'')){errors.push(`Invalid ${name} hash: ${path}`);return;}
 if(sha256File(absolute)!==expected)errors.push(`Hash mismatch for ${name}: ${path}`);
};
const git=(root,args)=>{try{return execFileSync('git',['-C',root,...args],{encoding:'utf8',stdio:['ignore','pipe','ignore']}).trim();}catch{return null;}};
const parseEvidenceLog=(root,entry,index,errors)=>{
 const path=safeLocalPath(root,entry.interactionLogPath); if(!path||!existsSync(path))return;
 let log;try{log=JSON.parse(readFileSync(path,'utf8'));}catch{errors.push(`Invalid JSON interaction log: ${entry.interactionLogPath}`);return;}
 for(const key of ['capabilityId','fixtureId','sequence','assertions','privacyVerdict','accessibilityVerdict'])if(!(key in log))errors.push(`Missing interaction log ${key}: ${entry.interactionLogPath}`);
 if(log.capabilityId!==entry.capabilityId)errors.push(`Interaction log capability mismatch: ${entry.capabilityId}`);
 if(log.sequence!==index)errors.push(`Interaction sequence mismatch: ${entry.capabilityId}`);
 if(!Array.isArray(log.assertions)||log.assertions.length===0||log.assertions.some(assertion=>!assertion||assertion.pass!==true||typeof assertion.expected==='undefined'||typeof assertion.actual==='undefined'))errors.push(`Interaction assertions lack actual passing proof: ${entry.capabilityId}`);
 if(log.privacyVerdict!=='pass')errors.push(`Interaction privacy verdict is not pass: ${entry.capabilityId}`);
 if(log.accessibilityVerdict!=='pass')errors.push(`Interaction accessibility verdict is not pass: ${entry.capabilityId}`);
};
const parsePng=(root,entry,errors)=>{
 const path=safeLocalPath(root,entry.capturePath);if(!path||!existsSync(path))return;
 const bytes=readFileSync(path); const signature='89504e470d0a1a0a';
 if(bytes.subarray(0,8).toString('hex')!==signature||bytes.length<24){errors.push(`Capture is not a PNG: ${entry.capturePath}`);return;}
 const width=bytes.readUInt32BE(16),height=bytes.readUInt32BE(20);
 if(width<1||height<1)errors.push(`Capture dimensions are invalid: ${entry.capturePath}`);
};
const parseWindowsArtifact=(root,proof,errors)=>{
 const path=safeLocalPath(root,proof.artifactPath);if(!path||!existsSync(path))return;
 const bytes=readFileSync(path);
 if(bytes.length<64||bytes.subarray(0,2).toString('ascii')!=='MZ'){errors.push(`Artifact is not a Windows executable: ${proof.artifactPath}`);return;}
 const peOffset=bytes.readUInt32LE(0x3c);
 if(peOffset+4>bytes.length||bytes.subarray(peOffset,peOffset+4).toString('ascii')!=='PE\0\0')errors.push(`Artifact lacks a PE signature: ${proof.artifactPath}`);
};

export function validateCandidate(candidate,{root=process.cwd()}={}){
 const errors=[];
 if(!candidate||typeof candidate!=='object'||Array.isArray(candidate))return {ok:false,errors:['Candidate must be an object']};
 onlyKeys(candidate,['schemaVersion','candidateId','sourceCommit','sourceTreeSha256','releasePhase','evaluatedAt','components','actualProof','interactions'],'candidate',errors);
 for(const key of ['schemaVersion','candidateId','sourceCommit','sourceTreeSha256','releasePhase','evaluatedAt','components','actualProof','interactions'])if(!(key in candidate))errors.push(`Missing ${key}`);
 if(candidate.schemaVersion!==1)errors.push('Unsupported schemaVersion');
 requiredString(candidate.candidateId,'candidateId',errors);
 if(!/^[A-Za-z0-9][A-Za-z0-9._-]{2,127}$/u.test(candidate.candidateId||''))errors.push('Invalid candidateId');
 if(!hex40.test(candidate.sourceCommit||''))errors.push('Invalid sourceCommit');
 if(!hex64.test(candidate.sourceTreeSha256||''))errors.push('Invalid sourceTreeSha256');
 if(candidate.releasePhase!=='final-single-full-release')errors.push('Completion is reserved for final-single-full-release');
 if(!/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d+)?Z$/u.test(candidate.evaluatedAt||''))errors.push('Invalid evaluatedAt');
 if(!candidate.components||typeof candidate.components!=='object'||Array.isArray(candidate.components)||Object.keys(candidate.components).length===0||Object.values(candidate.components).some(value=>typeof value!=='string'||!value))errors.push('Missing component versions');
 const gitCommit=git(root,['rev-parse','--verify',`${candidate.sourceCommit}^{commit}`]);
 if(!gitCommit)errors.push('sourceCommit is not a commit in the candidate repository');
 else if(sha256((git(root,['ls-tree','-r','--full-tree',candidate.sourceCommit])||'').trimEnd())!==candidate.sourceTreeSha256)errors.push('sourceTreeSha256 does not match sourceCommit');
 const proof=candidate.actualProof;
 if(!proof||typeof proof!=='object'||Array.isArray(proof))errors.push('Missing actualProof');
 else {onlyKeys(proof,['kind','artifactType','artifactPath','artifactSha256','provenancePath','provenanceSha256'],'actualProof',errors);if(proof.kind!=='built-artifact')errors.push('actualProof must identify a built-artifact');if(!['installed-runtime','squirrel-installer'].includes(proof.artifactType))errors.push('actualProof must identify an installed runtime or Squirrel installer');evidenceFile(root,proof.artifactPath,proof.artifactSha256,'artifact proof',errors);parseWindowsArtifact(root,proof,errors);evidenceFile(root,proof.provenancePath,proof.provenanceSha256,'artifact provenance',errors);const path=safeLocalPath(root,proof.provenancePath);try{const provenance=JSON.parse(readFileSync(path,'utf8'));if(provenance.sourceCommit!==candidate.sourceCommit)errors.push('Artifact provenance sourceCommit mismatch');}catch{errors.push('Artifact provenance must be JSON');}}
 if(!Array.isArray(candidate.interactions))errors.push('interactions must be an array');
 else {
   const seen=new Set();
   for(const [index,entry] of candidate.interactions.entries()){
     const prefix=`interaction ${index}`;
     if(!entry||typeof entry!=='object'||Array.isArray(entry)){errors.push(`${prefix} must be an object`);continue;}
     onlyKeys(entry,['capabilityId','fixtureId','action','expectedOutcome','language','theme','viewport','scale','screen','state','interactionLogPath','interactionLogSha256','capturePath','captureSha256'],prefix,errors);
     for(const field of ['capabilityId','fixtureId','action','expectedOutcome','language','theme','viewport','scale','screen','state','interactionLogPath','interactionLogSha256','capturePath','captureSha256'])requiredString(entry[field],`${prefix}.${field}`,errors);
     if(!['en','yue','bilingual'].includes(entry.language))errors.push(`Invalid interaction language: ${entry.capabilityId}`);
     if(!['light','dark'].includes(entry.theme))errors.push(`Invalid interaction theme: ${entry.capabilityId}`);
     if(!requiredCapabilityIds.includes(entry.capabilityId))errors.push(`Unregistered capability evidence: ${entry.capabilityId}`);
     if(seen.has(entry.capabilityId))errors.push(`Duplicate capability evidence: ${entry.capabilityId}`);
     seen.add(entry.capabilityId);
     evidenceFile(root,entry.interactionLogPath,entry.interactionLogSha256,`${prefix} interaction log`,errors);
     evidenceFile(root,entry.capturePath,entry.captureSha256,`${prefix} capture`,errors);
     parseEvidenceLog(root,entry,index,errors);parsePng(root,entry,errors);
   }
   for(const id of requiredCapabilityIds)if(!seen.has(id))errors.push(`Missing required CAD capability: ${id}`);
   for(const id of bracketWorkflowIds)if(!seen.has(id))errors.push(`Mechanical bracket workflow lacks: ${id}`);
   const positions=bracketWorkflowIds.map(id=>candidate.interactions.findIndex(entry=>entry.capabilityId===id));
   if(positions.some((position,index)=>index>0&&position<=positions[index-1]))errors.push('Mechanical bracket workflow is not in required order');
 }
 return {ok:errors.length===0,requiredCapabilities:requiredCapabilityIds.length,errors};
}

if(process.argv[1]&&resolve(process.argv[1])===fileURLToPath(import.meta.url)){
 const input=process.argv[2];
 if(!input){console.error('Usage: node scripts/check-product-acceptance.mjs <candidate.json>');process.exitCode=2;}
 else {let candidate;try{candidate=JSON.parse(readFileSync(resolve(process.cwd(),input),'utf8'));}catch(error){console.error(JSON.stringify({ok:false,errors:[`Invalid JSON: ${error.message}`]},null,2));process.exitCode=1;candidate=null;}if(candidate){const result=validateCandidate(candidate,{root:process.cwd()});console.log(JSON.stringify(result,null,2));process.exitCode=result.ok?0:1;}}
}
