#!/usr/bin/env python3
"""Fail closed before a Windows Release unless prerequisite CI passed at this SHA."""

import json
import os
import sys
from urllib.error import HTTPError, URLError
from urllib.parse import quote
from urllib.request import Request, urlopen

CORE_WORKFLOW = "core-contracts.yml"
SEPARATION_WORKFLOW = "separation.yml"
OPTIONAL_WORKFLOWS = {"transcription-runtime.yml", SEPARATION_WORKFLOW}
CORE_TEST_STEP = "Core contract test suite"


class GateError(Exception):
    pass


def workflow_name(run):
    return run.get("path", "").split("@", 1)[0].rsplit("/", 1)[-1]


def latest_runs_by_workflow(runs, sha):
    latest = {}
    for run in runs:
        if run.get("head_sha") != sha:
            continue
        name = workflow_name(run)
        if name not in {CORE_WORKFLOW, *OPTIONAL_WORKFLOWS}:
            continue
        previous = latest.get(name)
        if previous is None or (run.get("run_number", 0), run.get("run_attempt", 1)) > (
            previous.get("run_number", 0), previous.get("run_attempt", 1)
        ):
            latest[name] = run
    return latest


def verify(sha, scope, runs, jobs_for_run):
    """Return evidence; raise GateError on missing, pending, failed or skipped checks."""
    if not (len(sha) == 40 and all(c in "0123456789abcdefABCDEF" for c in sha)):
        raise GateError("Invalid commit SHA")
    required = {CORE_WORKFLOW}
    if scope in {"separation", "separation-ui"}:
        required.add(SEPARATION_WORKFLOW)
    latest = latest_runs_by_workflow(runs, sha)
    evidence = []
    for name in sorted(required | (set(latest) & OPTIONAL_WORKFLOWS)):
        run = latest.get(name)
        if run is None:
            raise GateError(f"Missing {name} for {sha}; run the required contract workflow first")
        if run.get("status") != "completed" or run.get("conclusion") != "success":
            raise GateError(
                f"{name} run {run.get('id')} is {run.get('status')}/{run.get('conclusion')}; "
                "complete/fix the contract before Windows Release"
            )
        if name == CORE_WORKFLOW:
            jobs = jobs_for_run(run["id"])
            passed = any(
                step.get("name") == CORE_TEST_STEP and step.get("conclusion") == "success"
                for job in jobs
                for step in job.get("steps", [])
            )
            if not passed:
                raise GateError(
                    f"{name} run {run['id']} succeeded without executing '{CORE_TEST_STEP}' "
                    "(possibly skipped or a different Fast Core profile)"
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

        for line in verify(sha, scope, runs, jobs_for_run):
            print(line)
        print("CI preflight PASS: Windows job may begin for this exact SHA")
        return 0
    except (GateError, HTTPError, URLError, OSError, ValueError, KeyError) as error:
        print(f"CI preflight BLOCKED: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
