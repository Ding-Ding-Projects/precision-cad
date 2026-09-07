import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtempSync, mkdirSync, rmSync, writeFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {bracketWorkflowIds,requiredCapabilityIds,validateCandidate} from '../../scripts/check-product-acceptance.mjs';

const hash=value=>createHash('sha256').update(value).digest('hex');
const png=Buffer.from('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M/wHwAF/gL+dnRU9wAAAABJRU5ErkJggg==','base64');
const fixture=()=>{
 const root=mkdtempSync(join(tmpdir(),'precision-cad-acceptance-'));
 mkdirSync(join(root,'evidence'),{recursive:true});
 writeFileSync(join(root,'README.md'),'acceptance fixture');
 execFileSync('git',['init','--quiet'],{cwd:root});execFileSync('git',['config','user.name','Acceptance Fixture'],{cwd:root});execFileSync('git',['config','user.email','fixture@example.invalid'],{cwd:root});execFileSync('git',['add','README.md'],{cwd:root});execFileSync('git',['commit','--quiet','-m','fixture'],{cwd:root});
 const sourceCommit=execFileSync('git',['rev-parse','HEAD'],{cwd:root,encoding:'utf8'}).trim();
 const sourceTreeSha256=hash(execFileSync('git',['ls-tree','-r','--full-tree','HEAD'],{cwd:root,encoding:'utf8'}).trimEnd());
 const write=(path,contents)=>{writeFileSync(join(root,path),contents);return hash(contents);};
 const executable=Buffer.alloc(128);executable.write('MZ');executable.writeUInt32LE(64,0x3c);executable.write('PE\0\0',64);const artifactSha256=write('evidence/PrecisionCAD.exe',executable);
 const provenancePath='evidence/provenance.json'; const provenanceSha256=write(provenancePath,JSON.stringify({sourceCommit}));
 const interactions=requiredCapabilityIds.map((capabilityId,index)=>{
   const interactionLogPath=`evidence/${capabilityId}.json`;
   const capturePath=`evidence/${capabilityId}.png`;
   const log={capabilityId,fixtureId:'mechanical-bracket-v1',sequence:index,assertions:[{expected:'documented acceptance result',actual:'observed acceptance result',pass:true}],privacyVerdict:'pass',accessibilityVerdict:'pass'};
   writeFileSync(join(root,capturePath),png);
   return {capabilityId,fixtureId:'mechanical-bracket-v1',action:`Exercise ${capabilityId}`,expectedOutcome:'The built artifact completes the declared operation.',language:'bilingual',theme:'dark',viewport:'1280x820',scale:'150%',screen:'main',state:'completed',interactionLogPath,interactionLogSha256:write(interactionLogPath,JSON.stringify(log)),capturePath,captureSha256:hash(png)};
 });
 return {root,candidate:{schemaVersion:1,candidateId:'precision-cad-acceptance-001',sourceCommit,sourceTreeSha256,releasePhase:'final-single-full-release',evaluatedAt:'2026-09-07T20:00:00.000Z',components:{qt:'6.8.3',opencascade:'7.8.1',calculix:'2.22',gmsh:'4.13.1'},actualProof:{kind:'built-artifact',artifactType:'installed-runtime',artifactPath:'evidence/PrecisionCAD.exe',artifactSha256,provenancePath,provenanceSha256},interactions}};
};
test('finite acceptance candidate requires every hand-written capability and a mechanical bracket workflow',()=>{
 const {root,candidate}=fixture();
 try {const result=validateCandidate(candidate,{root});assert.equal(result.ok,true,result.errors.join('\n'));assert.equal(result.requiredCapabilities,20);assert.deepEqual(bracketWorkflowIds,['CAD-SKETCH-001','CAD-SOLID-001','CAD-REF-001','CAD-DRAWING-001','CAD-EXCHANGE-001']);}
 finally {rmSync(root,{recursive:true,force:true});}
});
test('removing each required CAD capability turns completion red',()=>{
 for(const id of requiredCapabilityIds){const {root,candidate}=fixture();try{candidate.interactions=candidate.interactions.filter(entry=>entry.capabilityId!==id);const result=validateCandidate(candidate,{root});assert.equal(result.ok,false,id);assert.ok(result.errors.includes(`Missing required CAD capability: ${id}`),id);}finally{rmSync(root,{recursive:true,force:true});}}
});
test('forged evidence, an incomplete phase, unsafe paths, invented fields, and fake artifacts cannot claim completion',()=>{
 const {root,candidate}=fixture();
 try {candidate.interactions[0].captureSha256='0'.repeat(64);candidate.interactions[1].capturePath='../forged.png';candidate.releasePhase='intermediate-installer';writeFileSync(join(root,'evidence/PrecisionCAD.exe'),'fake');candidate.actualProof.artifactSha256=hash('fake');candidate.claimedWithoutProof=true;const result=validateCandidate(candidate,{root});assert.equal(result.ok,false);assert.ok(result.errors.some(error=>error.startsWith('Hash mismatch for interaction 0 capture:')));assert.ok(result.errors.includes('Completion is reserved for final-single-full-release'));assert.ok(result.errors.some(error=>error.startsWith('Unsafe interaction 1 capture path:')));assert.ok(result.errors.some(error=>error.startsWith('Artifact is not a Windows executable:')));assert.ok(result.errors.includes('Unexpected candidate field: claimedWithoutProof'));}
 finally {rmSync(root,{recursive:true,force:true});}
});
