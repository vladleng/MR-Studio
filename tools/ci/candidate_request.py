"""Resolve a manual release or an explicit same-repository PR title request."""
import json
import os
import re
import subprocess

from verify_candidate_gate import SCOPES

PREFIX = re.compile(r'^\[candidate:(studio-ui|studio|live|studio-live)\](?:\s|$)')


def resolve(event_name, event, environ):
    if event_name == 'workflow_dispatch':
        scope = environ['REQUEST_CHECKS']
        if scope not in SCOPES:
            raise ValueError('Invalid manual release scope')
        result = dict(scope=scope, integration=environ['REQUEST_INTEGRATION'],
                      ara=environ['REQUEST_ARA'], sha=environ['GITHUB_SHA'],
                      ref=environ['GITHUB_REF_NAME'], automatic='false')
    elif event_name == 'pull_request':
        pr = event['pull_request']
        old = event.get('changes', {}).get('title', {}).get('from')
        match = PREFIX.match(pr['title'])
        if (event.get('action') != 'edited' or not isinstance(old, str)
                or old.lower().startswith('[candidate:') or not match
                or pr['head']['repo']['full_name'] != environ['GITHUB_REPOSITORY']
                or pr['base']['ref'] != 'main'):
            raise ValueError('No explicit same-repository candidate request')
        scope = match.group(1)
        result = dict(scope=scope, integration='false',
                      ara='false' if scope == 'live' else 'true',
                      sha=pr['head']['sha'], ref=pr['head']['ref'], automatic='true')
    else:
        raise ValueError('Unsupported candidate event')
    if (not re.fullmatch(r'[a-f0-9]{40}', result['sha'])
            or not result['ref'] or any(c in result['ref'] for c in '\r\n')
            or result['integration'] not in ('true', 'false')
            or result['ara'] not in ('true', 'false')):
        raise ValueError('Invalid candidate identity or options')
    return result


def main():
    with open(os.environ['GITHUB_EVENT_PATH'], encoding='utf-8') as file:
        event = json.load(file)
    result = resolve(os.environ['GITHUB_EVENT_NAME'], event, os.environ)
    if subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip() != result['sha']:
        raise ValueError('Candidate checkout SHA mismatch')
    with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as output:
        for name, value in result.items():
            output.write(f'{name}={value}\n')
    print(f"Candidate {result['scope']} at {result['sha']} ({result['ref']})")


if __name__ == '__main__':
    main()
