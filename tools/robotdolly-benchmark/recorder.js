// ROBOTDOLLY Benchmark local text recorder. Node.js 18+; no dependencies.
// Runs only on loopback. Writes reports in this folder's reports directory.
"use strict";
const http = require("node:http");
const os = require("node:os");
const fs = require("node:fs");
const path = require("node:path");
const { execFileSync } = require("node:child_process");
const PORT = 18473, directory = path.join(__dirname, "reports");
fs.mkdirSync(directory, { recursive: true });
function windowsSpec() {
  if (process.platform !== "win32") return { os: os.type()+" "+os.release(), cpu: os.cpus()?.[0]?.model, ram_gb: +(os.totalmem()/1073741824).toFixed(1), gpu: "unknown" };
  const ps = "$c=Get-CimInstance Win32_Processor|Select-Object -First 1 Name,NumberOfCores,NumberOfLogicalProcessors; $g=Get-CimInstance Win32_VideoController|Select-Object Name,AdapterRAM,DriverVersion; $o=Get-CimInstance Win32_OperatingSystem|Select-Object Caption,Version,BuildNumber; @{cpu=$c;gpu=@($g);os=$o} | ConvertTo-Json -Depth 6 -Compress";
  try { return JSON.parse(execFileSync("powershell.exe",["-NoProfile","-NonInteractive","-Command",ps],{encoding:"utf8",timeout:12000})); }
  catch (e) { return { note: "Windows hardware query failed", cpu: os.cpus()?.[0]?.model, ram_gb: +(os.totalmem()/1073741824).toFixed(1) }; }
}
function mean(values) { return values.length ? values.reduce((a,b)=>a+b,0)/values.length : null; }
function writeReport(data) {
  const samples = Array.isArray(data.samples) ? data.samples.slice(0,7200) : [];
  const fps = samples.map(s=>+s.fps).filter(v=>Number.isFinite(v)&&v>0);
  const sorted = fps.slice().sort((a,b)=>a-b);
  const stats = { samples:samples.length, average_fps:mean(fps), min_sampled_fps:sorted[0]??null, max_sampled_fps:sorted[sorted.length-1]??null };
  const hardware = windowsSpec();
  const lines = [
    "ROBOTDOLLY DARKRIDE BENCHMARK — v0.1", "Date (UTC): "+String(data.timestamp||new Date().toISOString()),
    "Build ID: "+String(data.build_id||"UNKNOWN"), "Test: "+String(data.test_label||"Unlabelled"),
    "Landmark / location: "+String(data.location||"Unknown"),
    "Target resolution: 1920 x 1080 (NOT enforced or verified by script)",
    "Hardware: "+JSON.stringify(hardware), "System RAM (GB): "+(os.totalmem()/1073741824).toFixed(1),
    "Summary: "+JSON.stringify(stats),
    "CAVEAT: Once-per-second FPS samples cannot accurately measure 1% lows or frame pacing.",
    "CAVEAT: Display resolution, render scale, scene consistency and viewer settings must be matched manually.",
    "", "SAMPLES (JSON lines)", ...samples.map(s=>JSON.stringify(s)), ""
  ];
  const stamp=new Date().toISOString().replace(/[:.]/g,"-");
  const id=String(data.build_id||"unknown").replace(/[^\w.-]/g,"_").slice(0,48);
  const file=path.join(directory, "darkride_"+id+"_"+stamp+".txt");
  fs.writeFileSync(file,lines.join("\r\n"),"utf8");
  console.log("Saved:",file);
  return file;
}
const server=http.createServer((req,res)=>{
  if(req.method!=="POST" || req.url!=="/report"){res.writeHead(404);res.end();return;}
  let body="";
  req.on("data",chunk=>{body+=chunk;if(body.length>2_000_000)req.destroy();});
  req.on("end",()=>{
    try{ const d=JSON.parse(body); if(!Array.isArray(d.samples))throw Error("No samples");
      const f=writeReport(d);res.writeHead(200,{"Content-Type":"text/plain","Access-Control-Allow-Origin":"*"});res.end(f);
    }catch(e){res.writeHead(400,{"Access-Control-Allow-Origin":"*"});res.end(String(e));}
  });
});
server.listen(PORT,"127.0.0.1",()=>console.log("ROBOTDOLLY recorder listening on http://127.0.0.1:"+PORT+"; reports -> "+directory));
