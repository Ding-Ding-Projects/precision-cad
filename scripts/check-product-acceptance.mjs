import {existsSync,readFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {resolve,relative,isAbsolute} from 'node:path';
import {fileURLToPath} from 'node:url';
import {inspectInventory} from './check-surface-completeness.mjs';

// Hand-written approved initial-suite capability inventory. It is never inferred from evidence.
export let requiredCapabilityIds=Object.freeze(['CAD-DOC-001','CAD-SAVE-001','CAD-GEOMETRY-001','CAD-VIEWPORT-001','CAD-HISTORY-001','CAD-SKETCH-ENTITIES-001','CAD-SKETCH-CONSTRAINTS-001','CAD-SKETCH-EXPRESSIONS-001','CAD-SKETCH-PROFILE-001','CAD-SOLID-EXTRUDE-001','CAD-SOLID-REVOLVE-001','CAD-SOLID-LOFT-001','CAD-SOLID-SWEEP-001','CAD-MODIFY-FILLET-001','CAD-MODIFY-CHAMFER-001','CAD-MODIFY-SHELL-001','CAD-MODIFY-DRAFT-001','CAD-MODIFY-PATTERN-001','CAD-REF-001','CAD-ASSEMBLY-MATES-001','CAD-ASSEMBLY-INTERFERENCE-001','CAD-ASSEMBLY-BOM-001','CAD-DRAWING-PDF-001','CAD-DRAWING-DXF-001','CAD-SHEET-UNFOLD-001','CAD-SHEET-REFOLD-001','CAD-EXCHANGE-STEP-001','CAD-EXCHANGE-IGES-001','CAD-EXCHANGE-STL-001','CAD-EXCHANGE-3MF-001','CAD-CAM-25D-001','CAD-CAM-3AXIS-001','CAD-CAM-SIMULATION-001','CAD-CAM-GCODE-001','CAD-MESH-001','CAD-ANALYSIS-STATIC-001','CAD-ANALYSIS-MODAL-001','CAD-SCRIPT-001','CAD-SURFACE-001','CAD-INSTALL-001','CAD-UPDATE-001','CAD-RELEASE-001']);
export const bracketWorkflowIds=Object.freeze(['CAD-SKETCH-ENTITIES-001','CAD-SKETCH-CONSTRAINTS-001','CAD-SKETCH-EXPRESSIONS-001','CAD-SKETCH-PROFILE-001','CAD-SOLID-EXTRUDE-001','CAD-MODIFY-FILLET-001','CAD-DRAWING-PDF-001','CAD-EXCHANGE-STEP-001']);
export const approvedCapabilityMembers=Object.freeze({
 foundation:['TRANSACTIONS','UNDO','REDO','ATOMIC_SAVE','EXCLUSIVE_WRITER','RECOVERY','CANCEL','STALE_RESULT','BOUNDS','MIGRATION'],
 sketch:['DATUM','POINT','LINE','POLYLINE','RECTANGLE','CIRCLE','ARC','SPLINE','CONSTRUCTION','SNAP','TRIM','OFFSET','CLOSED_PROFILE','COINCIDENT','HORIZONTAL','VERTICAL','PARALLEL','PERPENDICULAR','TANGENT','EQUAL','DISTANCE','RADIUS','ANGLE','DOF','CONFLICT','UNIT_EXPRESSIONS'],
 viewport:['DEPTH','NORMALS','MULTIBODY','ORTHOGRAPHIC','PERSPECTIVE','STANDARD_VIEWS','SECTION','FACE_PICK','EDGE_PICK','VERTEX_PICK','ORBIT','PAN','ZOOM','FIT'],
 solid:['PAD','POCKET','REVOLVE','GROOVE','HOLE','UNION','CUT','INTERSECTION','TRANSLATE','ROTATE','FILLET','CHAMFER','SHELL','DRAFT','MIRROR','LINEAR_PATTERN','CIRCULAR_PATTERN','LOFT','SWEEP','SURFACE_TRIM','SEW','OFFSET','THICKEN'],
 exchange:['IMPORT_STEP','EXPORT_STEP','IMPORT_IGES','EXPORT_IGES','IMPORT_STL','EXPORT_STL','IMPORT_3MF','EXPORT_3MF','UNITS','SOURCE_IDENTITY','HEALING','LOSS_DISCLOSURE'],
 daily:['DOCUMENT_TABS','TREE','PROPERTIES','MEASURE','RECENT','RECOVERY','HISTORY_COMPARE','RESTORE','KEYBOARD','EXAMPLES'],
 assembly:['COMPONENTS','NESTED_INSTANCES','FIXED','COINCIDENT','CONCENTRIC','PARALLEL','PERPENDICULAR','DISTANCE','ANGLE','DOF','CONFLICTS','INTERFERENCE','EXPLODED','BOM'],
 drawing:['BASE','PROJECTED','SECTION','DETAIL','DIMENSIONS','TOLERANCES','ANNOTATIONS','TITLEBLOCKS','BOM','REVISION','PDF','DXF'],
 sheet:['THICKNESS','BEND_PARAMS','BASE_WALL','FLANGE','BEND','RELIEF','CUT','UNFOLD','REFOLD','BEND_TABLE','FLAT_DXF'],
 cam:['STOCK','TOOLS','FIXTURES','WCS','FACING','PROFILE','POCKET','DRILL','ROUGH_3AXIS','FINISH_3AXIS','SIMULATION','COLLISION','GENERIC_GCODE'],
 analysis:['MATERIAL','LOAD','RESTRAINT','MESH','STATIC','MODAL','RESULTS','REVISION','CONVERGENCE'],
 command:['API','DISCOVERY','DIAGNOSTICS','BATCH','SCRIPT_CLIENT']
});
requiredCapabilityIds=Object.freeze([...requiredCapabilityIds,...Object.entries(approvedCapabilityMembers).flatMap(([group,members])=>members.map(member=>`CAD-${group.toUpperCase()}-${member}-001`))]);
const hash=value=>createHash('sha256').update(value).digest('hex'); const h64=/^[0-9a-f]{64}$/u;
const file=(root,path,expected,label,errors)=>{if(typeof path!=='string'||isAbsolute(path)){errors.push(`Unsafe ${label} path`);return null;}const full=resolve(root,path);if(relative(root,full).startsWith('..')||!existsSync(full)){errors.push(`Missing ${label}`);return null;}if(!h64.test(expected||'')||hash(readFileSync(full))!==expected)errors.push(`Hash mismatch for ${label}`);return full;};
const git=(root,args)=>{try{return execFileSync('git',['-C',root,...args],{encoding:'utf8'}).trim();}catch{return null;}};
export function validateFixture(candidate,{root=process.cwd()}={}){
 const errors=[];if(!candidate||typeof candidate!=='object')return {ok:false,errors:['Candidate must be an object']};
 if(!/^[0-9a-f]{40}$/u.test(candidate.sourceCommit||'')||!git(root,['rev-parse','--verify',`${candidate.sourceCommit}^{commit}`]))errors.push('Candidate source commit is unavailable');
 if(!h64.test(candidate.sourceTreeSha256||'')||hash((git(root,['ls-tree','-r','--full-tree',candidate.sourceCommit])||'').trimEnd())!==candidate.sourceTreeSha256)errors.push('Candidate source tree hash mismatch');
 if(!Array.isArray(candidate.interactions)){errors.push('Interactions are required');return {ok:false,errors};} const seen=new Set();let prior=-1;
 for(const entry of candidate.interactions){if(!entry||!requiredCapabilityIds.includes(entry.capabilityId)||seen.has(entry.capabilityId)){errors.push(`Invalid or duplicate capability: ${entry?.capabilityId}`);continue;}seen.add(entry.capabilityId);for(const key of ['fixtureId','action','expectedOutcome','language','theme','viewport','scale','screen','state'])if(typeof entry[key]!=='string'||!entry[key])errors.push(`Missing ${entry.capabilityId}.${key}`);if(!Number.isInteger(entry.previousRevision)||!Number.isInteger(entry.resultRevision)||entry.resultRevision<=entry.previousRevision||entry.previousRevision<prior)errors.push(`Invalid revision lineage: ${entry.capabilityId}`);prior=entry.resultRevision;const log=file(root,entry.interactionLogPath,entry.interactionLogSha256,`${entry.capabilityId} log`,errors);file(root,entry.capturePath,entry.captureSha256,`${entry.capabilityId} capture`,errors);try{const record=JSON.parse(readFileSync(log,'utf8'));if(record.capabilityId!==entry.capabilityId||record.fixtureId!==entry.fixtureId||!Array.isArray(record.assertions)||record.assertions.some(a=>a?.pass!==true||!('expected'in a)||!('actual'in a)||!('comparison'in a)))errors.push(`Unproven structured assertions: ${entry.capabilityId}`);}catch{errors.push(`Invalid interaction log: ${entry.capabilityId}`);}}
 for(const id of requiredCapabilityIds)if(!seen.has(id))errors.push(`Missing required CAD capability: ${id}`);const bracket=bracketWorkflowIds.map(id=>candidate.interactions.find(entry=>entry.capabilityId===id));if(bracket.some(entry=>!entry||entry.fixtureId!=='mechanical-bracket-v1'))errors.push('Mechanical bracket fixture lineage is incomplete');else if(bracket.some((entry,index)=>index&&entry.previousRevision<bracket[index-1].resultRevision))errors.push('Mechanical bracket workflow is unordered');
 return {ok:errors.length===0,errors,requiredCapabilities:requiredCapabilityIds.length};
}
export function validateRelease(candidate,{root=process.cwd()}={}){const result=validateFixture(candidate,{root});const inventory=inspectInventory(readFileSync(resolve(root,'docs/engineering/surface-completeness.md'),'utf8'));result.errors.push(...inventory.errors.map(error=>`Surface completion: ${error}`));result.errors.push('No registered real runtime acceptance collector exists');result.ok=false;return result;}
export const validateCandidate=validateRelease;
if(process.argv[1]&&resolve(process.argv[1])===fileURLToPath(import.meta.url)){const input=process.argv[2];const fixture=process.argv.includes('--fixture-consistency');const result=input?(fixture?validateFixture:validateRelease)(JSON.parse(readFileSync(resolve(process.cwd(),input),'utf8'))):{ok:false,errors:['Candidate JSON path required']};console.log(JSON.stringify(result,null,2));process.exitCode=result.ok?0:1;}
