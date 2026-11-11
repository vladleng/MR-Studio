"""Validate this run's dependencies; never infer success from unrelated API runs."""
import os
import subprocess

SCOPES = {'profile', 'studio', 'studio-live', 'live', 'separation', 'separation-ui'}

def verify_candidate(scope, integration, results):
    if scope not in SCOPES:
        raise ValueError(f'Unknown release scope: {scope}')
    required = {'core', 'gui'}
    if scope != 'live' or integration:
        required.update({'contracts', 'runtime'})
    for name in sorted(required):
        if results.get(name) != 'success':
            raise ValueError(f'Required candidate check {name}: {results.get(name, "missing")}')

if __name__ == '__main__':
    sha = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
    if sha != os.environ['GITHUB_SHA']:
        raise SystemExit('Candidate checkout SHA mismatch')
    verify_candidate(os.environ['RELEASE_CHECKS'], os.environ['INTEGRATION'] == 'true',
                     {name: os.environ.get(name.upper()) for name in ('core','gui','contracts','runtime')})
    print(f'Candidate dependencies PASS at {sha}')
