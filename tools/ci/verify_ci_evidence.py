"""Read-only Release gate. Missing evidence never dispatches a workflow."""
import io
import json
import os
import subprocess
import time
import zipfile
from urllib.request import Request, HTTPRedirectHandler, build_opener
from urllib.parse import urlsplit
from urllib.parse import quote
from ci_evidence import definition_hash
from verify_candidate_gate import required_checks

class SafeRedirect(HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, msg, headers, newurl):
        redirected = super().redirect_request(request, fp, code, msg, headers, newurl)
        if redirected is not None and urlsplit(request.full_url).netloc != urlsplit(newurl).netloc:
            redirected.remove_header('Authorization')
        return redirected


WORKFLOWS = dict(core='core-contracts.yml', gui='windows-juce-gui-compile.yml',
                 contracts='transcription-contracts.yml', runtime='transcription-runtime.yml',
                 separation='separation.yml', mp3='windows-mp3-export.yml')


def validate_evidence(data, sha, workflow, check):
    if (data.get('schema') != 1 or data.get('sha') != sha
            or data.get('workflow') != workflow
            or data.get('definition') != definition_hash(workflow)
            or check not in data.get('checks', [])):
        raise ValueError('source SHA, workflow definition or check profile does not match')


class PendingCheck(ValueError):
    pass


class FailedCheck(ValueError):
    pass


def expected_check(name, scope, integration):
    live = integration or scope in {'live', 'studio-live', 'profile'}
    return 'gui-live' if name == 'gui' and live else name


def existing_check(sha, name, scope, integration, runs, artifact_evidence):
    workflow = WORKFLOWS[name]
    check = expected_check(name, scope, integration)
    candidates = [r for r in runs if r.get('head_sha') == sha
                  and r.get('path') == f'.github/workflows/{workflow}'
                  and r.get('event') in {'pull_request', 'push', 'workflow_dispatch'}]
    for run in sorted(candidates, key=lambda r: (r['id'], r.get('run_attempt', 1)), reverse=True):
        if run.get('status') != 'completed':
            raise PendingCheck(f"{run.get('status')}/{run.get('conclusion')}")
        if run.get('conclusion') != 'success':
            raise FailedCheck(f"{run.get('status')}/{run.get('conclusion')}")
        data = artifact_evidence(run['id'], name)
        if check not in data.get('checks', []):
            continue
        validate_evidence(data, sha, workflow, check)
        return f'{check}: PASS at {sha} (run {run["id"]})'
    raise ValueError('missing check')


def verify_existing(sha, scope, integration, runs, artifact_evidence, ref, checks=None):
    errors = []
    evidence = []
    for name in sorted(required_checks(scope, integration) if checks is None else checks):
        workflow = WORKFLOWS[name]
        check = expected_check(name, scope, integration)
        command = f'gh workflow run {workflow} --ref {ref}'
        if check == 'gui-live':
            command += ' -f live=true'
        if name == 'core':
            command += ' -f checks=profile'
        try:
            evidence.append(existing_check(sha, name, scope, integration, runs, artifact_evidence))
        except (ValueError, KeyError, OSError, zipfile.BadZipFile) as error:
            errors.append(f'{check}: {error}. Run: {command}')
    if errors:
        raise ValueError('\n'.join(errors))
    return evidence


def plan_missing(sha, scope, integration, runs, artifact_evidence):
    missing, pending = set(), set()
    for name in sorted(required_checks(scope, integration)):
        try:
            print(existing_check(sha, name, scope, integration, runs, artifact_evidence))
        except PendingCheck:
            pending.add(name)
        except FailedCheck as error:
            raise ValueError(f'{name}: {error}; fix or explicitly rerun the failed check') from error
        except (ValueError, KeyError, OSError, zipfile.BadZipFile):
            missing.add(name)
    return missing, pending


def verify_automatic(sha, scope, integration, runs, artifact_evidence, ref, needs, run_id):
    plan = needs['plan']
    if plan['result'] != 'success':
        raise ValueError('Candidate plan did not pass')
    reused = set()
    for name in sorted(required_checks(scope, integration)):
        decision = plan['outputs'].get(f'need_{name}')
        if decision == 'false':
            if needs[name]['result'] != 'skipped':
                raise ValueError(f'{name}: unexpected reused-check result')
            reused.add(name)
        elif decision == 'true':
            if needs[name]['result'] != 'success':
                raise ValueError(f'{name}: required candidate job {needs[name]["result"]}')
            # The enclosing run is still running, so validate the successful job's
            # own evidence artifact, not the enclosing run's conclusion.
            validate_evidence(artifact_evidence(run_id, name), sha, WORKFLOWS[name],
                              expected_check(name, scope, integration))
            print(f'{name}: PASS at {sha} (candidate run {run_id})')
            # A concurrently started standalone failure/pending run must not
            # be hidden by a successful candidate job.
            try:
                existing_check(sha, name, scope, integration, runs, artifact_evidence)
            except (PendingCheck, FailedCheck):
                raise
            except (ValueError, KeyError, OSError, zipfile.BadZipFile):
                pass  # New job has valid evidence for this SHA/profile.
        else:
            raise ValueError(f'{name}: missing or invalid plan decision')
    return verify_existing(sha, scope, integration, runs, artifact_evidence, ref, checks=reused)


def main():
    sha = os.environ.get('CANDIDATE_SHA', os.environ['GITHUB_SHA'])
    if subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip() != sha:
        raise ValueError('Candidate checkout SHA mismatch')
    root = os.environ['GITHUB_API_URL'] + '/repos/' + os.environ['GITHUB_REPOSITORY'] + '/'
    def fetch(path, binary=False):
        request = Request(root + path, headers={'Authorization': 'Bearer ' + os.environ['GITHUB_TOKEN'],
                          'Accept': 'application/vnd.github+json', 'X-GitHub-Api-Version': '2022-11-28'})
        with build_opener(SafeRedirect()).open(request, timeout=25) as response:
            data = response.read(1024 * 1024)
        return data if binary else json.loads(data)
    def pages(path, field):
        result = []
        for page in range(1, 101):
            payload = fetch(f'{path}{"&" if "?" in path else "?"}per_page=100&page={page}')
            result.extend(payload[field])
            if len(result) >= payload['total_count']:
                return result
        raise ValueError('API pagination limit exceeded; evidence cannot be verified')
    def artifact(run_id, name):
        items = pages(f'actions/runs/{run_id}/artifacts', 'artifacts')
        matching = [a for a in items if a['name'] == f'ci-evidence-{name}-{sha}' and not a['expired']]
        if len(matching) != 1:
            raise ValueError('successful execution evidence missing or expired')
        data = fetch(f'actions/artifacts/{matching[0]["id"]}/zip', binary=True)
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            info = archive.getinfo('ci-evidence.json')
            if info.file_size > 65536:
                raise ValueError('invalid evidence size')
            return json.loads(archive.read(info))
    mode = os.environ.get('CI_GATE_MODE', 'verify')
    scope = os.environ['RELEASE_CHECKS']
    integration = os.environ['INTEGRATION'] == 'true'
    ref = os.environ.get('CANDIDATE_REF', os.environ['GITHUB_REF_NAME'])

    def list_runs():
        return pages(f'actions/runs?head_sha={quote(sha)}', 'workflow_runs')

    if mode in ('plan', 'manual-plan'):
        missing = set()
        if mode == 'plan':
            # Allow synchronize-triggered PR checks to register before planning.
            time.sleep(20)
            deadline = time.monotonic() + 30 * 60
            while True:
                missing, pending = plan_missing(sha, scope, integration, list_runs(), artifact)
                if not pending:
                    break
                if time.monotonic() >= deadline:
                    raise ValueError(f'Pending candidate checks: {sorted(pending)}')
                print(f'Awaiting existing checks: {sorted(pending)}', flush=True)
                time.sleep(60)
        with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as output:
            for name in WORKFLOWS:
                output.write(f'need_{name}={"true" if name in missing else "false"}\n')
        print(f'Checks to run: {sorted(missing)}')
        return
    runs = list_runs()
    if mode == 'automatic':
        lines = verify_automatic(sha, scope, integration, runs, artifact, ref,
                                 json.loads(os.environ['CI_NEEDS']), int(os.environ['GITHUB_RUN_ID']))
    elif mode == 'verify':
        lines = verify_existing(sha, scope, integration, runs, artifact, ref)
    else:
        raise ValueError(f'Unknown CI gate mode: {mode}')
    for line in lines:
        print(line)


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        message = f'CI preflight BLOCKED: {error}'
        if os.environ.get('GITHUB_STEP_SUMMARY'):
            with open(os.environ['GITHUB_STEP_SUMMARY'], 'a') as summary:
                summary.write('```text\n' + message + '\n```\n')
        raise SystemExit(message)
