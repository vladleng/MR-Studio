#!/usr/bin/env python3
"""Fail closed before a Windows Release unless prerequisite CI passed at this SHA."""

import json
import os
import subprocess
import sys
from urllib.error import HTTPError, URLError
from urllib.parse import quote
from urllib.request import Request, urlopen

CORE_WORKFLOW = "core-contracts.yml"
SEPARATION_WORKFLOW = "separation.yml"
RELEASE_WORKFLOW = "audio-engine.yml"
GUI_WORKFLOW = "windows-juce-gui-compile.yml"
CORE_TEST_STEP = "Core contract test suite"
GUI_TEST_STEP = "Compile JUCE GUI sources using actual MSVC /WX flags"


class GateError(Exception):
    pass


def workflow_name(run):
    path = run.get("path", "").split("@", 1)[0]
    return path.rsplit("/", 1)[-1] if path.startswith(".github/workflows/") else ""


def latest_runs_by_workflow(runs, sha):
    latest = {}
    for run in runs:
        if run.get("head_sha") != sha:
            continue
        name = workflow_name(run)
        if not name or name == RELEASE_WORKFLOW:
            continue
        previous = latest.get(name)
        if previous is None or (run.get("run_number", 0), run.get("run_attempt", 1)) > (
            previous.get("run_number", 0), previous.get("run_attempt", 1)
        ):
            latest[name] = run
    return latest


def gui_affecting_path(path):
    """Keep this path contract aligned with windows-juce-gui-compile.yml."""
    return path.startswith(("apps/studio-juce/src/", "apps/studio-desktop/include/", "cmake/")) or path in {
        "CMakeLists.txt", "tools/ci/Prepare-Sdks.ps1", "tools/ci/sdk-lock.json",
        ".github/workflows/windows-juce-gui-compile.yml",
    }


def changed_paths_for_release(ref_name):
    """PR/feature: full divergence from main; main: changed files in last push/merge."""
    def git(*args):
        return subprocess.check_output(["git", *args], text=True, stderr=subprocess.PIPE).strip()

    base = git("rev-parse", "HEAD^") if ref_name == "main" else git("merge-base", "HEAD", "origin/main")
    return git("diff", "--name-only", base, "HEAD").splitlines()


def verify(sha, scope, runs, jobs_for_run, gui_required=False):
    """Return evidence; raise GateError on missing, pending, failed or skipped checks."""
    if not (len(sha) == 40 and all(c in "0123456789abcdefABCDEF" for c in sha)):
        raise GateError("Invalid commit SHA")
    required = {CORE_WORKFLOW}
    if scope in {"separation", "separation-ui"}:
        required.add(SEPARATION_WORKFLOW)
    if gui_required and scope in {"profile", "studio", "studio-live", "separation-ui"}:
        required.add(GUI_WORKFLOW)
    latest = latest_runs_by_workflow(runs, sha)
    evidence = []
    for name in sorted(required | set(latest)):
        run = latest.get(name)
        if run is None:
            raise GateError(f"Missing {name} for {sha}; run the required contract workflow first")
        if run.get("status") != "completed" or run.get("conclusion") != "success":
            raise GateError(
                f"{name} run {run.get('id')} is {run.get('status')}/{run.get('conclusion')}; "
                "complete/fix the contract before Windows Release"
            )
        if name in {CORE_WORKFLOW, GUI_WORKFLOW}:
            expected_step = CORE_TEST_STEP if name == CORE_WORKFLOW else GUI_TEST_STEP
            jobs = jobs_for_run(run["id"])
            passed = any(
                job.get("conclusion") == "success"
                and step.get("name") == expected_step and step.get("conclusion") == "success"
                for job in jobs
                for step in job.get("steps", [])
            )
            if not passed:
                raise GateError(
                    f"{name} run {run['id']} succeeded without executing '{expected_step}' "
                    "(possibly skipped or a different profile)"
                )
        evidence.append(f"{name}: PASS at {sha[:12]} (run {run['id']})")
    return evidence


def main():
    token = os.environ["GITHUB_TOKEN"]
    repo = os.environ["GITHUB_REPOSITORY"]
    sha = os.environ["GITHUB_SHA"]
    scope = os.environ.get("RELEASE_CHECKS", "profile")
    api_root = os.environ.get("GITHUB_API_URL", "https://api.github.com").rstrip("/")

    def get_json(path):
        request = Request(
            f"{api_root}/repos/{repo}/{path}",
            headers={
                "Accept": "application/vnd.github+json",
                "Authorization": f"Bearer {token}",
                "X-GitHub-Api-Version": "2022-11-28",
            },
        )
        with urlopen(request, timeout=25) as response:
            return json.load(response)

    try:
        encoded = quote(sha, safe="")
        payload = get_json(f"actions/runs?head_sha={encoded}&per_page=100")
        if payload.get("total_count", 0) > 100:
            raise GateError("More than 100 CI runs for this SHA: cannot verify all relevant runs")
        runs = payload.get("workflow_runs", [])

        def jobs_for_run(run_id):
            response = get_json(f"actions/runs/{run_id}/jobs?per_page=100")
            if response.get("total_count", 0) > 100:
                raise GateError("Fast Core has over 100 jobs: cannot verify executed test steps")
            return response.get("jobs", [])

        paths = changed_paths_for_release(os.environ.get("GITHUB_REF_NAME", ""))
        gui_required = any(gui_affecting_path(p) for p in paths)
        print(f"Windows GUI compilation required for release diff: {gui_required}")
        for line in verify(sha, scope, runs, jobs_for_run, gui_required=gui_required):
            print(line)
        print("CI preflight PASS: Windows job may begin for this exact SHA")
        return 0
    except (GateError, HTTPError, URLError, OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print(f"CI preflight BLOCKED: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
