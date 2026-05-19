import {
  Activity,
  AlertTriangle,
  CheckCircle2,
  Clock,
  Code2,
  Copy,
  Cpu,
  FileCode2,
  FileJson,
  Gauge,
  Hammer,
  ListChecks,
  Play,
  RefreshCw,
  ShieldCheck,
  Terminal,
  Timer,
  Workflow,
  XCircle
} from "lucide-react";
import { useCallback, useEffect, useMemo, useState } from "react";
import type {
  AllocationReport,
  BenchmarkRow,
  DashboardReports,
  DashboardStatus,
  EvaluationRow,
  Job,
  TaskDefinition,
  TaskId
} from "./types";

const groupLabels = {
  core: "Build + Correctness",
  reports: "Reports + Transform",
  runtime: "Runtime Evidence"
};

const taskIcons: Record<TaskId, typeof Activity> = {
  demo: Workflow,
  build: Hammer,
  ctest: ListChecks,
  evaluate: Gauge,
  arrayReport: FileJson,
  failureReport: AlertTriangle,
  profile: Activity,
  transform: Code2,
  benchmark: Timer,
  hlfir: Cpu
};

const jobStatusIcon = {
  queued: Clock,
  running: RefreshCw,
  completed: CheckCircle2,
  failed: XCircle
};

async function api<T>(path: string, init?: RequestInit): Promise<T> {
  const response = await fetch(path, {
    ...init,
    headers: {
      "content-type": "application/json",
      ...(init?.headers || {})
    }
  });
  if (!response.ok) {
    throw new Error(await response.text());
  }
  return response.json() as Promise<T>;
}

function number(value: string | number | undefined): number {
  const parsed = Number(value ?? 0);
  return Number.isFinite(parsed) ? parsed : 0;
}

function formatBytes(value: number): string {
  if (value < 1024) {
    return `${value} B`;
  }
  if (value < 1024 * 1024) {
    return `${(value / 1024).toFixed(1)} KB`;
  }
  return `${(value / (1024 * 1024)).toFixed(2)} MB`;
}

function summarizeEvaluation(rows: EvaluationRow[]) {
  const baseline = rows.reduce((sum, row) => sum + number(row.baseline_estimated_bytes), 0);
  const saved = rows.reduce((sum, row) => sum + number(row.estimated_byte_reduction), 0);
  const sites = rows.reduce((sum, row) => sum + number(row.sites), 0);
  const eliminable = rows.reduce((sum, row) => sum + number(row.provably_eliminable), 0);
  return {
    baseline,
    saved,
    sites,
    eliminable,
    percent: baseline === 0 ? 0 : (saved / baseline) * 100
  };
}

function classifyTone(value: string | undefined): string {
  if (value === "provably-eliminable") {
    return "good";
  }
  if (value === "possibly-unnecessary") {
    return "warn";
  }
  if (value === "necessary") {
    return "bad";
  }
  return "neutral";
}

async function copyToClipboard(text: string): Promise<void> {
  if (!text.trim()) {
    throw new Error("Nothing to copy yet");
  }

  if (navigator.clipboard && window.isSecureContext) {
    try {
      await navigator.clipboard.writeText(text);
      return;
    } catch {
      // Fall through to the textarea copy path for browsers that block the API.
    }
  }

  const textarea = document.createElement("textarea");
  textarea.value = text;
  textarea.setAttribute("readonly", "true");
  textarea.style.position = "fixed";
  textarea.style.left = "-9999px";
  textarea.style.top = "0";
  document.body.appendChild(textarea);
  textarea.focus();
  textarea.select();

  const copied = document.execCommand("copy");
  document.body.removeChild(textarea);
  if (!copied) {
    throw new Error("Browser blocked clipboard access");
  }
}

function StatusPill({ ok, label }: { ok: boolean; label: string }) {
  const Icon = ok ? CheckCircle2 : XCircle;
  return (
    <span className={`pill ${ok ? "good" : "bad"}`}>
      <Icon size={14} />
      {label}
    </span>
  );
}

function MetricCard({
  icon: Icon,
  label,
  value,
  detail
}: {
  icon: typeof Activity;
  label: string;
  value: string;
  detail: string;
}) {
  return (
    <section className="metric-card">
      <div className="metric-icon">
        <Icon size={18} />
      </div>
      <div>
        <p>{label}</p>
        <strong>{value}</strong>
        <span>{detail}</span>
      </div>
    </section>
  );
}

function TaskCard({
  task,
  running,
  onRun,
  onCopy
}: {
  task: TaskDefinition;
  running: boolean;
  onRun: (taskId: TaskId) => void;
  onCopy: (text: string, label: string) => void;
}) {
  const Icon = taskIcons[task.id];
  return (
    <article className={`task-card ${task.id === "demo" ? "primary-task" : ""}`}>
      <div className="task-heading">
        <span className="task-icon">
          <Icon size={18} />
        </span>
        <div>
          <h3>{task.title}</h3>
          <p>{task.deliverable}</p>
        </div>
      </div>
      <p className="task-detail">{task.detail}</p>
      <div className="task-actions">
        <button
          className="icon-button"
          type="button"
          title="Copy command"
          aria-label={`Copy command for ${task.title}`}
          onClick={() => onCopy(task.command, `${task.title} command`)}
        >
          <Copy size={16} />
        </button>
        <button className="run-button" type="button" onClick={() => onRun(task.id)} disabled={running}>
          <Play size={16} />
          Run
        </button>
      </div>
    </article>
  );
}

function ReportSummary({ title, report }: { title: string; report?: AllocationReport }) {
  const entries = report?.entries ?? [];
  const summary = report?.summary;
  return (
    <section className="panel">
      <div className="panel-title">
        <FileJson size={17} />
        <h2>{title}</h2>
      </div>
      {report ? (
        <>
          <div className="mini-metrics">
            <span>{summary?.totalSites ?? entries.length} sites</span>
            <span>{summary?.provablyEliminable ?? 0} eliminable</span>
            <span>{formatBytes(summary?.totalEstimatedBytes ?? 0)}</span>
          </div>
          <div className="entry-list">
            {entries.map((entry, index) => (
              <div className="entry-row" key={`${entry.opName}-${index}`}>
                <span className={`classification ${classifyTone(entry.classification)}`}>{entry.classification}</span>
                <strong>{entry.construct || entry.opName}</strong>
                <small>
                  line {entry.line ?? "?"}, {formatBytes(entry.estimatedBytes ?? 0)}
                </small>
                <p>{entry.reason}</p>
              </div>
            ))}
          </div>
        </>
      ) : (
        <div className="empty-state">Run the matching task to generate this report.</div>
      )}
    </section>
  );
}

function EvaluationTable({ rows }: { rows: EvaluationRow[] }) {
  return (
    <section className="panel wide-panel">
      <div className="panel-title">
        <Gauge size={17} />
        <h2>Evaluation</h2>
      </div>
      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Testcase</th>
              <th>Sites</th>
              <th>Eliminable</th>
              <th>Maybe</th>
              <th>Necessary</th>
              <th>Baseline</th>
              <th>Saved</th>
              <th>Reduction</th>
            </tr>
          </thead>
          <tbody>
            {rows.map((row) => (
              <tr key={row.testcase}>
                <td>{row.testcase}</td>
                <td>{row.sites}</td>
                <td>{row.provably_eliminable}</td>
                <td>{row.possibly_unnecessary}</td>
                <td>{row.necessary}</td>
                <td>{formatBytes(number(row.baseline_estimated_bytes))}</td>
                <td>{formatBytes(number(row.estimated_byte_reduction))}</td>
                <td>{row.estimated_reduction_percent}%</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </section>
  );
}

function BenchmarkTable({ rows }: { rows: BenchmarkRow[] }) {
  return (
    <section className="panel wide-panel">
      <div className="panel-title">
        <Timer size={17} />
        <h2>Runtime Benchmark</h2>
      </div>
      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Program</th>
              <th>Baseline</th>
              <th>Optimized</th>
              <th>Speedup</th>
              <th>Status</th>
            </tr>
          </thead>
          <tbody>
            {rows.map((row) => (
              <tr key={row.program}>
                <td>{row.program}</td>
                <td>{row.baseline_seconds || "-"}</td>
                <td>{row.optimized_seconds || "-"}</td>
                <td className={number(row.speedup_percent) >= 0 ? "positive" : "negative"}>{row.speedup_percent || "-"}%</td>
                <td>{row.status}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
      <p className="note">Tiny kernels can be noisy on Windows; the allocation estimates are the stable evidence.</p>
    </section>
  );
}

function CodePreview({
  title,
  icon: Icon,
  text,
  onCopy
}: {
  title: string;
  icon: typeof Activity;
  text?: string;
  onCopy: (text: string, label: string) => void;
}) {
  return (
    <section className="panel code-panel">
      <div className="panel-title split-title">
        <div>
          <Icon size={17} />
          <h2>{title}</h2>
        </div>
        <button className="icon-button small-icon-button" type="button" title={`Copy ${title}`} onClick={() => onCopy(text || "", title)}>
          <Copy size={15} />
        </button>
      </div>
      {text ? <pre>{text}</pre> : <div className="empty-state">No generated file yet.</div>}
    </section>
  );
}

function TerminalPanel({ job, onCopy }: { job?: Job; onCopy: (text: string, label: string) => void }) {
  const Icon = job ? jobStatusIcon[job.status] : Terminal;
  const logText = job?.logs.join("\n") || "";
  return (
    <section className="terminal-panel">
      <div className="terminal-heading">
        <div>
          <Icon className={job?.status === "running" ? "spin" : ""} size={18} />
          <h2>{job ? `${job.title} · ${job.status}` : "Terminal Output"}</h2>
        </div>
        <div className="terminal-tools">
          {job?.finishedAt && <span>{new Date(job.finishedAt).toLocaleTimeString()}</span>}
          <button className="terminal-copy" type="button" onClick={() => onCopy(logText, "terminal logs")}>
            <Copy size={14} />
            Copy Logs
          </button>
        </div>
      </div>
      <pre>{job ? logText : "Run a task to see live output here."}</pre>
    </section>
  );
}

export default function App() {
  const [tasks, setTasks] = useState<TaskDefinition[]>([]);
  const [status, setStatus] = useState<DashboardStatus | null>(null);
  const [reports, setReports] = useState<DashboardReports | null>(null);
  const [activeJob, setActiveJob] = useState<Job | undefined>();
  const [error, setError] = useState<string>("");
  const [copyNotice, setCopyNotice] = useState<string>("");

  const refresh = useCallback(async () => {
    const [nextTasks, nextStatus, nextReports] = await Promise.all([
      api<TaskDefinition[]>("/api/tasks"),
      api<DashboardStatus>("/api/status"),
      api<DashboardReports>("/api/reports")
    ]);
    setTasks(nextTasks);
    setStatus(nextStatus);
    setReports(nextReports);
  }, []);

  useEffect(() => {
    refresh().catch((err) => setError(err.message));
    const id = window.setInterval(() => refresh().catch(() => undefined), 6000);
    return () => window.clearInterval(id);
  }, [refresh]);

  useEffect(() => {
    if (!activeJob || !["queued", "running"].includes(activeJob.status)) {
      return;
    }
    const id = window.setInterval(async () => {
      const next = await api<Job>(`/api/jobs/${activeJob.id}`);
      setActiveJob(next);
      if (next.status === "completed" || next.status === "failed") {
        await refresh();
      }
    }, 900);
    return () => window.clearInterval(id);
  }, [activeJob, refresh]);

  const runTask = async (taskId: TaskId) => {
    setError("");
    try {
      const job = await api<Job>("/api/run", {
        method: "POST",
        body: JSON.stringify({ taskId })
      });
      setActiveJob(job);
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err));
    }
  };

  const handleCopy = async (text: string, label: string) => {
    try {
      await copyToClipboard(text);
      setCopyNotice(`Copied ${label}`);
    } catch (err) {
      setCopyNotice(err instanceof Error ? err.message : "Copy failed");
    }
    window.setTimeout(() => setCopyNotice(""), 1800);
  };

  const running = activeJob?.status === "queued" || activeJob?.status === "running";
  const evaluation = reports?.evaluation ?? [];
  const benchmark = reports?.benchmark ?? [];
  const evalSummary = useMemo(() => summarizeEvaluation(evaluation), [evaluation]);
  const checksOk = status?.checks.filter((check) => check.ok).length ?? 0;
  const groupedTasks = useMemo(
    () =>
      tasks.reduce<Record<string, TaskDefinition[]>>((groups, task) => {
        groups[task.group] = [...(groups[task.group] || []), task];
        return groups;
      }, {}),
    [tasks]
  );

  return (
    <main>
      <header className="app-header">
        <div>
          <p className="eyebrow">Flang Implicit Allocation Profiler</p>
          <h1>Demo Console</h1>
          <p className="subhead">{status?.root || "D:\\Flang-Implicit-Allocation-Profiler-and-Optimizer"}</p>
        </div>
        <div className="header-actions">
          <button className="secondary-button" type="button" onClick={() => refresh()}>
            <RefreshCw size={16} />
            Refresh
          </button>
          <button className="primary-button" type="button" onClick={() => runTask("demo")} disabled={running}>
            <Play size={16} />
            Full Demo
          </button>
        </div>
      </header>

      {error && <div className="error-banner">{error}</div>}
      {copyNotice && <div className="copy-toast">{copyNotice}</div>}

      <section className="status-strip">
        {status?.checks.map((check) => (
          <div className="status-card" key={check.label}>
            <StatusPill ok={check.ok} label={check.label} />
            <span>{check.detail || check.path}</span>
          </div>
        ))}
      </section>

      <section className="metrics-grid">
        <MetricCard icon={ShieldCheck} label="Environment" value={`${checksOk}/${status?.checks.length ?? 0}`} detail="required artifacts ready" />
        <MetricCard icon={Activity} label="Allocation Sites" value={`${evalSummary.sites}`} detail={`${evalSummary.eliminable} provably eliminable`} />
        <MetricCard icon={Gauge} label="Estimated Saving" value={formatBytes(evalSummary.saved)} detail={`${evalSummary.percent.toFixed(1)}% static reduction`} />
        <MetricCard icon={Timer} label="Runtime Cases" value={`${benchmark.filter((row) => row.status === "ok").length}/${benchmark.length || 3}`} detail="baseline vs optimized kernels" />
      </section>

      <section className="workspace-grid">
        <div className="left-stack">
          {Object.entries(groupedTasks).map(([group, items]) => (
            <section className="task-section" key={group}>
              <h2>{groupLabels[group as keyof typeof groupLabels]}</h2>
              <div className="task-grid">
                {items.map((task) => (
                  <TaskCard task={task} running={Boolean(running)} onRun={runTask} onCopy={handleCopy} key={task.id} />
                ))}
              </div>
            </section>
          ))}
        </div>
        <TerminalPanel job={activeJob} onCopy={handleCopy} />
      </section>

      <section className="report-grid">
        <EvaluationTable rows={evaluation} />
        <BenchmarkTable rows={benchmark} />
        <ReportSummary title="Array Temporary" report={reports?.reports.array} />
        <ReportSummary title="Failure Case" report={reports?.reports.failure} />
        <ReportSummary title="Profile Refinement" report={reports?.reports.profile} />
        <ReportSummary title="Real HLFIR" report={reports?.reports.realHlfir} />
        <CodePreview title="Original Fortran" icon={FileCode2} text={reports?.files.baselineVector} onCopy={handleCopy} />
        <CodePreview title="Transformed Fortran" icon={Code2} text={reports?.files.transformedVector} onCopy={handleCopy} />
        <CodePreview title="Real HLFIR Snippet" icon={FileCode2} text={reports?.files.realHlfir?.slice(0, 5000)} onCopy={handleCopy} />
      </section>
    </main>
  );
}
