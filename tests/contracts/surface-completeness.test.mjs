import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {inspectInventory,requiredIds} from '../../scripts/check-surface-completeness.mjs';
const source=readFileSync(new URL('../../docs/engineering/surface-completeness.md',import.meta.url),'utf8');
test('the explicit two-surface inventory is structurally valid and honestly incomplete',()=>{
 assert.equal(inspectInventory(source,{requireComplete:false}).ok,true);
 assert.equal(inspectInventory(source).ok,false);
 assert.equal(inspectInventory(source).errors.filter(e=>e.startsWith('Incomplete:')).length,128);
});
test('removing each canonical row independently turns the inventory check red',()=>{
 for(const id of requiredIds){
   const broken=source.split(/\r?\n/u).filter(line=>!line.startsWith(`| ${id} |`)).join('\n');
   const result=inspectInventory(broken,{requireComplete:false});
   assert.equal(result.ok,false,id);
   assert.ok(result.errors.includes(`Missing canonical feature: ${id}`),id);
 }
 assert.equal(inspectInventory(source,{requireComplete:false}).ok,true);
});
test('duplicate, renamed and malformed surface rows are not accepted',()=>{
 const line=source.split(/\r?\n/u).find(l=>l.startsWith('| CORE-001 |'));
 assert.equal(inspectInventory(source+'\n'+line,{requireComplete:false}).ok,false);
 assert.equal(inspectInventory(source.replace('| CORE-001 |','| CORE-001-renamed |'),{requireComplete:false}).ok,false);
 assert.equal(inspectInventory(source.replace(line,'| CORE-001 | Version | Missing |'),{requireComplete:false}).ok,false);
});
test('a completed label without an implemented evidence verifier always fails closed',()=>{
 const broken=source.replace('| Missing | Partial |','| Complete | Partial |');
 assert.equal(inspectInventory(broken,{requireComplete:false}).ok,false);
 assert.ok(inspectInventory(broken,{requireComplete:false}).errors.some(e=>e.includes('No complete evidence verifier')));
});
