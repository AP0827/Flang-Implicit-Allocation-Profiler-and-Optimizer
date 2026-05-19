import crypto from "node:crypto";
import fs from "node:fs";
import fsp from "node:fs/promises";
import http from "node:http";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { spawn } from "node:child_process";
import { createServer as createViteServer } from "vite";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(__dirname, "..");
const production = process.argv.includes("--production");
const port = Number(process.env.PORT || 5173);

const vsDevCmd =
  "C:\\Program Files\\Microsoft Visual Studio\\18\\Community\\Common7\\Tools\\VsDevCmd.bat";
const ctestExe =
  "C:\\Program Files\\Microsoft Visual Studio\\18\\Community\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\ctest.exe";
const flangExe = "D:\\llvm-project\\build\\bin\\flang.exe";

const tasks = [
  {
    id: "demo",
    title: "Run Full Demo",
    detail: "Runs tests, reports, profile refinement, source transform, benchmark, and real HLFIR smoke test.",
    command: "UI sequence: ctest -> evaluate -> reports -> transform -> benchmark -> HLFIR",
    deliverable: "Complete demo evidence refresh",
    group: "core"
  },
  {
    id: "build",
    title: "Build FIAP Tool",
    detail: "Configures and builds fiap-opt with LLVM, MLIR, and Flang CMake packages.",
    command: "powershell -ExecutionPolicy Bypass -File scripts\\build.ps1",
    deliverable: "build\\fiap-opt.exe",
    group: "core"
  },
  {
    id: "ctest",
    title: "Run MLIR Tests",
    detail: "Runs the five required allocation-analysis testcases through fiap-opt.",
    command: "build\\fiap-opt.exe testcases\\*.mlir --format=json",
    deliverable: "5/5 tests passing",
    group: "core"
  },
  {
    id: "evaluate",
    title: "Generate Evaluation CSV",
    detail: "Runs fiap-opt over all MLIR testcases and summarizes allocation reductions.",
    command: "python scripts\\evaluate.py --tool build\\fiap-opt.exe --testcases testcases --out reports\\evaluation-summary.csv",
    deliverable: "reports\\evaluation-summary.csv",
    group: "reports"
  },
  {
    id: "arrayReport",
    title: "Array Temporary Report",
    detail: "Creates the JSON report used by the source transformer demo.",
    command: "build\\fiap-opt.exe testcases\\01_array_temp.mlir --format=json",
    deliverable: "reports\\01_array_temp.json",
    group: "reports"
  },
  {
    id: "failureReport",
    title: "Failure Case Report",
    detail: "Shows a necessary temporary that escapes into a call and must not be rewritten.",
    command: "build\\fiap-opt.exe testcases\\04_escaping_temp.mlir --format=json",
    deliverable: "reports\\04_escaping_temp.json",
    group: "reports"
  },
  {
    id: "profile",
    title: "Profile-Guided Refinement",
    detail: "Applies sample runtime profile data to upgrade a possibly unnecessary site.",
    command: "fiap-opt function_result -> python scripts\\refine_profile.py",
    deliverable: "reports\\function_result.refined.json",
    group: "reports"
  },
  {
    id: "transform",
    title: "Source Rewrite Demo",
    detail: "Rewrites the simple rank-1 array expression into an explicit do concurrent loop.",
    command: "python src\\fiap_source_transformer.py --report reports\\01_array_temp.json --source testcases\\fortran\\vector_add.f90 --output reports\\vector_add.transformed.f90",
    deliverable: "reports\\vector_add.transformed.f90",
    group: "reports"
  },
  {
    id: "benchmark",
    title: "Runtime Benchmark",
    detail: "Compiles and times baseline versus optimized Fortran kernels with Flang when available.",
    command: "python scripts\\benchmark_fortran.py --compiler D:\\llvm-project\\build\\bin\\flang.exe --out reports\\runtime-benchmark.csv --runs 20",
    deliverable: "reports\\runtime-benchmark.csv",
    group: "runtime"
  },
  {
    id: "hlfir",
    title: "Real HLFIR Smoke Test",
    detail: "Uses your built Flang to emit HLFIR, then analyzes that real HLFIR with FIAP.",
    command: "flang -fc1 -emit-hlfir vector_add.f90 -> fiap-opt",
    deliverable: "reports\\vector_add_real_hlfir.json",
    group: "runtime"
  }
];

const taskById = new Map(tasks.map((task) => [task.id, task]));
const jobs = new Map();
const memoryFiles = new Map();

function nowIso() {
  return new Date().toISOString();
}

function exists(relativeOrAbsolute) {
  return fs.existsSync(path.isAbsolute(relativeOrAbsolute) ? relativeOrAbsolute : path.join(repoRoot, relativeOrAbsolute));
}

function rel(relativePath) {
  return path.join(repoRoot, relativePath);
}

function keyPath(relativePath) {
  return relativePath.replaceAll("/", "\\").toLowerCase();
}

function makeJob(taskId) {
  const task = taskById.get(taskId);
  if (!task) {
    throw new Error(`Unknown task: ${taskId}`);
  }
  const job = {
    id: crypto.randomUUID(),
    taskId,
    title: task.title,
    status: "queued",
    startedAt: nowIso(),
    logs: []
  };
  jobs.set(job.id, job);
  return job;
}

function log(job, message) {
  const stamped = `[${new Date().toLocaleTimeString()}] ${message}`;
  job.logs.push(stamped);
  if (job.logs.length > 2000) {
    job.logs.splice(0, job.logs.length - 2000);
  }
}

function runProcess(job, command, args, options = {}) {
  const cwd = options.cwd || repoRoot;
  const label = options.label || [command, ...args].join(" ");
  log(job, `$ ${label}`);

  return new Promise((resolve, reject) => {
    const child = spawn(command, args, {
      cwd,
      windowsHide: true,
      env: { ...process.env, ...options.env }
    });

    let stdout = "";
    let stderr = "";

    child.stdout.on("data", (chunk) => {
      const text = chunk.toString();
      stdout += text;
      text.split(/\r?\n/).filter(Boolean).forEach((line) => log(job, line));
    });

    child.stderr.on("data", (chunk) => {
      const text = chunk.toString();
      stderr += text;
      text.split(/\r?\n/).filter(Boolean).forEach((line) => log(job, line));
    });

    child.on("error", (error) => {
      log(job, `failed to start: ${error.message}`);
      reject(error);
    });

    child.on("close", (code) => {
      log(job, `exit code ${code}`);
      if (code === 0) {
        resolve({ stdout, stderr, code });
      } else {
        const error = new Error(`${label} failed with exit code ${code}`);
        error.stdout = stdout;
        error.stderr = stderr;
        error.code = code;
        reject(error);
      }
    });
  });
}

function isPermissionProblem(error) {
  const text = [error?.message, error?.stdout, error?.stderr].filter(Boolean).join("\n");
  return /EPERM|PermissionError|Permission denied|Access is denied|Access to the path|UnauthorizedAccessException/i.test(text);
}

async function runProcessKeepingExistingOutputs(job, command, args, outputPaths, options = {}) {
  try {
    return await runProcess(job, command, args, options);
  } catch (error) {
    const allOutputsExist = outputPaths.every((outputPath) => exists(outputPath));
    if (allOutputsExist && isPermissionProblem(error)) {
      log(job, `write permission was denied; keeping existing generated artifact(s): ${outputPaths.join(", ")}`);
      return { stdout: error.stdout || "", stderr: error.stderr || "", code: 0 };
    }
    throw error;
  }
}

async function captureToFile(job, command, args, outRelative, label) {
  const result = await runProcess(job, command, args, { label });
  const outPath = rel(outRelative);
  memoryFiles.set(keyPath(outRelative), result.stdout);
  await fsp.mkdir(path.dirname(outPath), { recursive: true });
  try {
    await writeTextOutput(job, outPath, result.stdout);
    log(job, `wrote ${outRelative}`);
  } catch (error) {
    log(job, `filesystem write failed; ${outRelative} is available in this dashboard session`);
  }
}

async function writeTextOutput(job, outPath, text) {
  try {
    await fsp.writeFile(outPath, text, "utf8");
    return;
  } catch (error) {
    log(job, `direct write failed (${error.code || error.message}); retrying through PowerShell`);
  }

  try {
    await new Promise((resolve, reject) => {
      const child = spawn(
        "powershell.exe",
        [
          "-NoProfile",
          "-Command",
          "$input | Set-Content -LiteralPath $env:FIAP_OUTPUT_PATH -Encoding UTF8"
        ],
        {
          cwd: repoRoot,
          windowsHide: true,
          env: { ...process.env, FIAP_OUTPUT_PATH: outPath }
        }
      );

      let stderr = "";
      child.stderr.on("data", (chunk) => {
        stderr += chunk.toString();
      });
      child.on("error", reject);
      child.on("close", (code) => {
        if (code === 0) {
          resolve();
        } else {
          reject(new Error(stderr.trim() || `PowerShell write failed with exit code ${code}`));
        }
      });
      child.stdin.end(text);
    });
  } catch (error) {
    if (fs.existsSync(outPath)) {
      log(job, `write retry failed; keeping existing ${path.relative(repoRoot, outPath)}`);
      return;
    }
    throw error;
  }
}

async function ensureArrayReport(job) {
  if (!exists("reports\\01_array_temp.json")) {
    await runTaskSteps(job, "arrayReport");
  }
}

async function runCtest(job) {
  const cases = [
    "01_array_temp.mlir",
    "02_function_result.mlir",
    "03_realloc_assignment.mlir",
    "04_escaping_temp.mlir",
    "05_elemental_temp.mlir"
  ];
  let passed = 0;
  for (const name of cases) {
    await runProcess(job, rel("build\\fiap-opt.exe"), [rel(`testcases\\${name}`), "--format=json"], {
      label: `build\\fiap-opt.exe testcases\\${name} --format=json`
    });
    passed += 1;
    log(job, `PASS ${name}`);
  }
  log(job, `${passed}/${cases.length} MLIR tests passed`);
}

async function runTaskSteps(job, taskId) {
  switch (taskId) {
    case "build":
      await runProcessKeepingExistingOutputs(
        job,
        "powershell.exe",
        ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", rel("scripts\\build.ps1")],
        ["build\\fiap-opt.exe"],
        { label: "powershell -ExecutionPolicy Bypass -File scripts\\build.ps1" }
      );
      break;
    case "ctest":
      await runCtest(job);
      break;
    case "evaluate":
      await runProcessKeepingExistingOutputs(
        job,
        "python",
        [
          rel("scripts\\evaluate.py"),
          "--tool",
          rel("build\\fiap-opt.exe"),
          "--testcases",
          rel("testcases"),
          "--out",
          rel("reports\\evaluation-summary.csv")
        ],
        ["reports\\evaluation-summary.csv"],
        { label: "python scripts\\evaluate.py --tool build\\fiap-opt.exe --testcases testcases --out reports\\evaluation-summary.csv" }
      );
      break;
    case "arrayReport":
      await captureToFile(
        job,
        rel("build\\fiap-opt.exe"),
        [rel("testcases\\01_array_temp.mlir"), "--format=json"],
        "reports\\01_array_temp.json",
        "build\\fiap-opt.exe testcases\\01_array_temp.mlir --format=json"
      );
      break;
    case "failureReport":
      await captureToFile(
        job,
        rel("build\\fiap-opt.exe"),
        [rel("testcases\\04_escaping_temp.mlir"), "--format=json"],
        "reports\\04_escaping_temp.json",
        "build\\fiap-opt.exe testcases\\04_escaping_temp.mlir --format=json"
      );
      break;
    case "profile":
      await captureToFile(
        job,
        rel("build\\fiap-opt.exe"),
        [rel("testcases\\02_function_result.mlir"), "--format=json"],
        "reports\\function_result.json",
        "build\\fiap-opt.exe testcases\\02_function_result.mlir --format=json"
      );
      await runProcessKeepingExistingOutputs(
        job,
        "python",
        [
          rel("scripts\\refine_profile.py"),
          "--report",
          rel("reports\\function_result.json"),
          "--profile",
          rel("profiles\\sample_profile.csv"),
          "--out",
          rel("reports\\function_result.refined.json")
        ],
        ["reports\\function_result.refined.json"],
        { label: "python scripts\\refine_profile.py --report reports\\function_result.json --profile profiles\\sample_profile.csv --out reports\\function_result.refined.json" }
      );
      break;
    case "transform":
      await ensureArrayReport(job);
      await runProcessKeepingExistingOutputs(
        job,
        "python",
        [
          rel("src\\fiap_source_transformer.py"),
          "--report",
          rel("reports\\01_array_temp.json"),
          "--source",
          rel("testcases\\fortran\\vector_add.f90"),
          "--output",
          rel("reports\\vector_add.transformed.f90")
        ],
        ["reports\\vector_add.transformed.f90"],
        { label: "python src\\fiap_source_transformer.py --report reports\\01_array_temp.json --source testcases\\fortran\\vector_add.f90 --output reports\\vector_add.transformed.f90" }
      );
      break;
    case "benchmark": {
      const args = [
        rel("scripts\\benchmark_fortran.py"),
        "--out",
        rel("reports\\runtime-benchmark.csv"),
        "--runs",
        "20"
      ];
      if (exists(flangExe)) {
        args.splice(1, 0, "--compiler", flangExe);
      }
      await runProcessKeepingExistingOutputs(
        job,
        "python",
        args,
        ["reports\\runtime-benchmark.csv"],
        {
          label: exists(flangExe)
            ? "python scripts\\benchmark_fortran.py --compiler D:\\llvm-project\\build\\bin\\flang.exe --out reports\\runtime-benchmark.csv --runs 20"
            : "python scripts\\benchmark_fortran.py --out reports\\runtime-benchmark.csv --runs 20"
        }
      );
      break;
    }
    case "hlfir":
      if (!exists(flangExe)) {
        throw new Error(`Flang not found: ${flangExe}`);
      }
      await runProcessKeepingExistingOutputs(
        job,
        flangExe,
        [
          "-fc1",
          "-emit-hlfir",
          "-o",
          rel("testcases\\fortran\\vector_add.mlir"),
          rel("testcases\\fortran\\vector_add.f90")
        ],
        ["testcases\\fortran\\vector_add.mlir"],
        { label: "flang -fc1 -emit-hlfir -o testcases\\fortran\\vector_add.mlir testcases\\fortran\\vector_add.f90" }
      );
      await captureToFile(
        job,
        rel("build\\fiap-opt.exe"),
        [rel("testcases\\fortran\\vector_add.mlir"), "--format=json"],
        "reports\\vector_add_real_hlfir.json",
        "build\\fiap-opt.exe testcases\\fortran\\vector_add.mlir --format=json"
      );
      break;
    case "demo":
      for (const step of ["ctest", "evaluate", "arrayReport", "failureReport", "profile", "transform", "benchmark", "hlfir"]) {
        log(job, `--- ${taskById.get(step).title} ---`);
        await runTaskSteps(job, step);
      }
      break;
    default:
      throw new Error(`Task has no runner: ${taskId}`);
  }
}

async function runJob(job) {
  job.status = "running";
  log(job, `started ${job.title}`);
  try {
    await runTaskSteps(job, job.taskId);
    job.status = "completed";
    job.exitCode = 0;
    log(job, `completed ${job.title}`);
  } catch (error) {
    job.status = "failed";
    job.exitCode = Number.isInteger(error.code) ? error.code : 1;
    log(job, `ERROR: ${error.message}`);
  } finally {
    job.finishedAt = nowIso();
  }
}

function parseCsv(text) {
  const lines = text.trim().split(/\r?\n/).filter(Boolean);
  if (lines.length < 2) {
    return [];
  }
  const headers = lines[0].split(",");
  return lines.slice(1).map((line) => {
    const values = line.split(",");
    return Object.fromEntries(headers.map((header, index) => [header, values[index] ?? ""]));
  });
}

async function readText(relativePath) {
  const memoryValue = memoryFiles.get(keyPath(relativePath));
  if (memoryValue !== undefined) {
    return memoryValue;
  }
  try {
    return await fsp.readFile(rel(relativePath), "utf8");
  } catch {
    return "";
  }
}

async function readJson(relativePath) {
  const text = await readText(relativePath);
  if (!text) {
    return undefined;
  }
  try {
    return JSON.parse(text);
  } catch {
    return undefined;
  }
}

async function getReports() {
  const evaluation = parseCsv(await readText("reports\\evaluation-summary.csv"));
  const benchmark = parseCsv(await readText("reports\\runtime-benchmark.csv"));

  return {
    evaluation,
    benchmark,
    reports: {
      array: await readJson("reports\\01_array_temp.json"),
      failure: await readJson("reports\\04_escaping_temp.json"),
      profile: await readJson("reports\\function_result.refined.json"),
      realHlfir: await readJson("reports\\vector_add_real_hlfir.json")
    },
    files: {
      baselineVector: await readText("testcases\\fortran\\vector_add.f90"),
      transformedVector: await readText("reports\\vector_add.transformed.f90"),
      realHlfir: await readText("testcases\\fortran\\vector_add.mlir")
    }
  };
}

function statDetail(relativeOrAbsolute) {
  const full = path.isAbsolute(relativeOrAbsolute) ? relativeOrAbsolute : rel(relativeOrAbsolute);
  if (!fs.existsSync(full)) {
    return undefined;
  }
  const stat = fs.statSync(full);
  return `${Math.max(1, Math.round(stat.size / 1024))} KB, ${stat.mtime.toLocaleString()}`;
}

function getStatus() {
  const checks = [
    { label: "FIAP executable", path: "build\\fiap-opt.exe", ok: exists("build\\fiap-opt.exe"), detail: statDetail("build\\fiap-opt.exe") },
    { label: "Flang compiler", path: flangExe, ok: exists(flangExe), detail: statDetail(flangExe) },
    { label: "Evaluation CSV", path: "reports\\evaluation-summary.csv", ok: exists("reports\\evaluation-summary.csv"), detail: statDetail("reports\\evaluation-summary.csv") },
    { label: "Runtime benchmark", path: "reports\\runtime-benchmark.csv", ok: exists("reports\\runtime-benchmark.csv"), detail: statDetail("reports\\runtime-benchmark.csv") },
    { label: "Source transform", path: "reports\\vector_add.transformed.f90", ok: exists("reports\\vector_add.transformed.f90"), detail: statDetail("reports\\vector_add.transformed.f90") },
    { label: "Real HLFIR report", path: "reports\\vector_add_real_hlfir.json", ok: exists("reports\\vector_add_real_hlfir.json"), detail: statDetail("reports\\vector_add_real_hlfir.json") }
  ];
  return { root: repoRoot, generatedAt: nowIso(), checks };
}

async function readBody(req) {
  const chunks = [];
  for await (const chunk of req) {
    chunks.push(chunk);
  }
  const text = Buffer.concat(chunks).toString("utf8");
  return text ? JSON.parse(text) : {};
}

function sendJson(res, data, status = 200) {
  res.writeHead(status, {
    "content-type": "application/json; charset=utf-8",
    "cache-control": "no-store"
  });
  res.end(JSON.stringify(data));
}

async function handleApi(req, res) {
  const url = new URL(req.url, `http://127.0.0.1:${port}`);

  if (req.method === "GET" && url.pathname === "/api/tasks") {
    sendJson(res, tasks);
    return true;
  }

  if (req.method === "GET" && url.pathname === "/api/status") {
    sendJson(res, getStatus());
    return true;
  }

  if (req.method === "GET" && url.pathname === "/api/reports") {
    sendJson(res, await getReports());
    return true;
  }

  if (req.method === "GET" && url.pathname.startsWith("/api/jobs/")) {
    const id = decodeURIComponent(url.pathname.slice("/api/jobs/".length));
    const job = jobs.get(id);
    sendJson(res, job || { error: "job not found" }, job ? 200 : 404);
    return true;
  }

  if (req.method === "POST" && url.pathname === "/api/run") {
    const body = await readBody(req);
    const taskId = body.taskId;
    if (!taskById.has(taskId)) {
      sendJson(res, { error: `Unknown task: ${taskId}` }, 400);
      return true;
    }
    const job = makeJob(taskId);
    queueMicrotask(() => runJob(job));
    sendJson(res, job, 202);
    return true;
  }

  return false;
}

async function serveStatic(req, res) {
  const url = new URL(req.url, `http://127.0.0.1:${port}`);
  const distRoot = path.join(__dirname, "dist");
  const requested = url.pathname === "/" ? "index.html" : decodeURIComponent(url.pathname.slice(1));
  const full = path.resolve(distRoot, requested);
  const safe = full.startsWith(distRoot);
  const file = safe && fs.existsSync(full) && fs.statSync(full).isFile() ? full : path.join(distRoot, "index.html");
  const ext = path.extname(file);
  const type = {
    ".html": "text/html; charset=utf-8",
    ".js": "text/javascript; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".svg": "image/svg+xml"
  }[ext] || "application/octet-stream";
  res.writeHead(200, { "content-type": type });
  fs.createReadStream(file).pipe(res);
}

const vite = production
  ? null
  : await createViteServer({
      root: __dirname,
      server: { middlewareMode: true },
      appType: "spa"
    });

const server = http.createServer(async (req, res) => {
  try {
    if (req.url?.startsWith("/api/") && (await handleApi(req, res))) {
      return;
    }
    if (production) {
      await serveStatic(req, res);
    } else {
      vite.middlewares(req, res);
    }
  } catch (error) {
    sendJson(res, { error: error.message || String(error) }, 500);
  }
});

server.listen(port, "127.0.0.1", () => {
  console.log(`FIAP dashboard running at http://127.0.0.1:${port}`);
});
