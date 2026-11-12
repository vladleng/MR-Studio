"""Read-only Release gate. Missing evidence never dispatches a workflow."""
import io
import json
import os
import subprocess
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
                 separation='separation.yml')


def validate_evidence(data, sha, workflow, check):
    if (data.get('schema') != 1 or data.get('sha') != sha
            or data.get('workflow') != workflow
            or data.get('definition') != definition_hash(workflow)
            or check not in data.get('checks', [])):
        raise ValueError('source SHA, workflow definition or check profile does not match')


def verify_existing(sha, scope, integration, runs, artifact_evidence, ref):
    errors = []
    evidence = []
    live = integration or scope in {'live', 'studio-live', 'profile'}
    for name in sorted(required_checks(scope, integration)):
        workflow = WORKFLOWS[name]
        check = 'gui-live' if name == 'gui' and live else name
        command = f'gh workflow run {workflow} --ref {ref}'
        if name == 'gui' and live:
            command += ' -f live=true'
        if name == 'core':
            command += ' -f checks=profile'
        candidates = [r for r in runs if r.get('head_sha') == sha
                      and r.get('path') == f'.github/workflows/{workflow}'
                      and r.get('event') in {'pull_request', 'push', 'workflow_dispatch'}]
        # A newer failure/pending run must not be hidden behind an older success.
        run = max(candidates, key=lambda r: (r['id'], r.get('run_attempt', 1)), default=None)
        try:
            if not run:
                raise ValueError('missing check')
            if run.get('status') != 'completed' or run.get('conclusion') != 'success':
                raise ValueError(f"{run.get('status')}/{run.get('conclusion')}")
            validate_evidence(artifact_evidence(run['id'], name), sha, workflow, check)
            evidence.append(f'{check}: PASS at {sha} (run {run["id"]})')
        except (ValueError, KeyError, OSError, zipfile.BadZipFile) as error:
            errors.append(f'{check}: {error}. Run: {command}')
    if errors:
        raise ValueError('\n'.join(errors))
    return evidence


def main():
    sha = os.environ['GITHUB_SHA']
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
    runs = pages(f'actions/runs?head_sha={quote(sha)}', 'workflow_runs')
    for line in verify_existing(sha, os.environ['RELEASE_CHECKS'], os.environ['INTEGRATION'] == 'true',
                                runs, artifact, os.environ['GITHUB_REF_NAME']):
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
