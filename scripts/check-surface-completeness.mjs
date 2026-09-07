import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

// This explicit list is independent of the Markdown row discovery.
export const requiredIds = Object.freeze([
 'CORE-001','CORE-002','CORE-003','CORE-004',
 'I18N-001','I18N-002','I18N-003','I18N-004','I18N-005',
 'NARR-001','NARR-002','SCHED-001','SCHED-002','DELIGHT-001','DELIGHT-002',
 'UI-001','UI-002','UI-003','UI-004','UI-005','UI-006',
 'SEARCH-001','SEARCH-002','FOCUS-001','NOTICE-001','SAFETY-001',
 'APPEAR-001','APPEAR-002','BRAND-001','CONVERT-001','CONVERT-002',
 'OLLAMA-001','OLLAMA-002','TABS-001','TABS-002','LOCK-001','LOCK-002',
 'AUTH-001','DOCS-001','DOCS-002','DOCS-003','HANDOFF-001','EXPORT-001',
 'BULK-001','HISTORY-001','PRESET-001','CHANGELOG-001','PALETTE-001',
 'OVERLAY-001','MENU-001','PROGRESS-001','EXTDL-001','EXTDL-002','EXTDL-003',
 'RECOVERY-001','CONTENT-001','PUBLISH-001','FILTER-001','RELEASE-001',
 'RELEASE-002','RELEASE-003','CAPTURE-001','CAPTURE-002','CAPTURE-003',
]);
export function inspectInventory(markdown, { requireComplete = true } = {}) {
 const errors=[];
 const rows=new Map();
 for(const line of markdown.split(/\r?\n/u)) {
   if(!/^\| [A-Z][A-Z0-9]*-[0-9]{3} \|/u.test(line))continue;
   const cells=line.split('|').slice(1,-1).map(value=>value.trim());
   if(cells.length!==4){errors.push(`Malformed surface row: ${cells[0]}`);continue;}
   if(rows.has(cells[0]))errors.push(`Duplicate feature: ${cells[0]}`);
   rows.set(cells[0],cells);
 }
 for(const id of requiredIds){
   const row=rows.get(id);
   if(!row){errors.push(`Missing canonical feature: ${id}`);continue;}
   if(!row[1])errors.push(`Missing feature description: ${id}`);
   for(const [index,surface] of [[2,'desktop'],[3,'website']]){
     if(!['Missing','Partial','Complete'].includes(row[index]))errors.push(`Invalid state: ${surface}/${id}`);
     if(row[index]==='Complete')errors.push(`No complete evidence verifier exists yet: ${surface}/${id}`);
     else if(requireComplete)errors.push(`Incomplete: ${surface}/${id} (${row[index]})`);
   }
 }
 for(const id of rows.keys())if(!requiredIds.includes(id))errors.push(`Unregistered feature: ${id}`);
 return {ok:errors.length===0,canonicalFeatures:requiredIds.length,surfaces:2,errors};
}
if(process.argv[1] && resolve(process.argv[1])===fileURLToPath(import.meta.url)){
 const root=resolve(import.meta.dirname,'..');
 const result=inspectInventory(readFileSync(resolve(root,'docs/engineering/surface-completeness.md'),'utf8'),{requireComplete:!process.argv.includes('--inventory-only')});
 console.log(JSON.stringify(result,null,2));
 process.exitCode=result.ok?0:1;
}
