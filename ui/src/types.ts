export type JobStatus = "queued" | "running" | "completed" | "failed";

export type TaskId =
  | "demo"
  | "build"
  | "ctest"
  | "evaluate"
  | "arrayReport"
  | "failureReport"
  | "profile"
  | "transform"
  | "benchmark"
  | "hlfir";

export interface TaskDefinition {
  id: TaskId;
  title: string;
  detail: string;
  command: string;
  deliverable: string;
  group: "core" | "reports" | "runtime";
}

export interface Job {
  id: string;
  taskId: TaskId;
  title: string;
  status: JobStatus;
  startedAt: string;
  finishedAt?: string;
  exitCode?: number;
  logs: string[];
}

export interface StatusItem {
  label: string;
  ok: boolean;
  path?: string;
  detail?: string;
}

export interface DashboardStatus {
  root: string;
  generatedAt: string;
  checks: StatusItem[];
}

export interface EvaluationRow {
  testcase: string;
  sites: string;
  provably_eliminable: string;
  possibly_unnecessary: string;
  necessary: string;
  baseline_estimated_bytes: string;
  optimized_estimated_bytes: string;
  estimated_byte_reduction: string;
  estimated_reduction_percent: string;
}

export interface BenchmarkRow {
  program: string;
  compiler: string;
  baseline_seconds: string;
  optimized_seconds: string;
  speedup_percent: string;
  status: string;
}

export interface AllocationEntry {
  file?: string;
  line?: number;
  column?: number;
  classification?: string;
  construct?: string;
  estimatedBytes?: number;
  reason?: string;
  advice?: string;
  transform?: string;
  opName?: string;
}

export interface AllocationReport {
  summary?: {
    totalSites?: number;
    provablyEliminable?: number;
    possiblyUnnecessary?: number;
    necessary?: number;
    totalEstimatedBytes?: number;
  };
  entries?: AllocationEntry[];
}

export interface DashboardReports {
  evaluation: EvaluationRow[];
  benchmark: BenchmarkRow[];
  reports: {
    array?: AllocationReport;
    failure?: AllocationReport;
    profile?: AllocationReport;
    realHlfir?: AllocationReport;
  };
  files: {
    baselineVector?: string;
    transformedVector?: string;
    realHlfir?: string;
  };
}
