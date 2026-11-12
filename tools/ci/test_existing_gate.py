"""Gate evidence selection regressions; no network, dispatch or build."""
import unittest
from unittest.mock import patch
from pathlib import Path
from ci_evidence import definition_hash
from verify_ci_evidence import verify_existing, WORKFLOWS

SHA = 'a' * 40
class ExistingEvidenceTests(unittest.TestCase):
    def setUp(self):
        # Generic profile-selection tests use synthetic definitions. This legacy
        # branch intentionally has no Studio Python runtime workflows.
        definition = patch('verify_ci_evidence.definition_hash', return_value='fixture-definition')
        definition.start()
        self.addCleanup(definition.stop)
        self.runs = [dict(id=i, path='.github/workflows/'+WORKFLOWS[n], head_sha=SHA,
                          event='pull_request', status='completed', conclusion='success')
                     for i,n in enumerate(('core','gui','contracts','runtime'),1)]
        self.data = {n: dict(schema=1, sha=SHA, workflow=WORKFLOWS[n],
                            definition='fixture-definition', checks=[n])
                     for n in ('core','gui','contracts','runtime')}
    def verify(self,scope='studio',integration=False):
        return verify_existing(SHA,scope,integration,self.runs,lambda _,n:self.data[n],'feature')
    def test_reuses_success_without_dispatch(self):
        self.assertEqual(len(self.verify()),4)
    def test_reports_every_missing_check_and_command(self):
        self.runs=[]
        with self.assertRaises(ValueError) as caught:self.verify()
        for name in ('core','gui','contracts','runtime'):
            self.assertIn('gh workflow run '+WORKFLOWS[name],str(caught.exception))
    def test_wrong_source_sha(self):
        self.data['core']['sha']='b'*40
        with self.assertRaises(ValueError):self.verify()
    def test_wrong_definition(self):
        self.data['core']['definition']='old'
        with self.assertRaises(ValueError):self.verify()
    def test_hollow_success(self):
        self.data['core']['checks']=[]
        with self.assertRaises(ValueError):self.verify()
    def test_live_requires_live_gui_evidence(self):
        with self.assertRaisesRegex(ValueError,'live=true'):self.verify('live')
        self.data['gui']['checks'].append('gui-live')
        self.assertEqual(len(self.verify('live')),2)
    def test_combined_profiles_require_extended_live_gui(self):
        for scope in ('studio-live', 'profile'):
            with self.subTest(scope=scope):
                with self.assertRaisesRegex(ValueError, 'gui-live'):
                    self.verify(scope)
        self.data['gui']['checks'].append('gui-live')
        for scope in ('studio-live', 'profile'):
            with self.subTest(scope=scope):
                self.assertEqual(len(self.verify(scope)), 4)

    def test_automatic_gui_workflow_compiles_live_before_evidence(self):
        workflow = Path('.github/workflows/windows-juce-gui-compile.yml').read_text()
        self.assertIn("GITHUB_EVENT_NAME -ne 'workflow_dispatch'", workflow)
        self.assertIn("REQUESTED_LIVE -eq 'true'", workflow)
        self.assertIn("steps.coverage.outputs.live == 'true' && 'ON' || 'OFF'", workflow)
        self.assertIn('MRLiveJuce.vcxproj', workflow)
        self.assertIn("steps.coverage.outputs.live == 'true' && 'gui-live' || ''", workflow)

    def test_integration_overrides_live(self):
        self.data['gui']['checks'].append('gui-live')
        self.runs=[r for r in self.runs if 'runtime' not in r['path']]
        with self.assertRaisesRegex(ValueError,'runtime'):self.verify('live',True)
    def test_latest_failure_or_pending_blocks_older_pass(self):
        for status,conclusion in [('completed','failure'),('in_progress',None),('completed','cancelled')]:
            self.runs.append(dict(self.runs[0],id=99,status=status,conclusion=conclusion))
            with self.subTest(status=status,conclusion=conclusion):
                with self.assertRaises(ValueError):self.verify()
            self.runs.pop()
    def test_other_sha_does_not_qualify(self):
        self.runs[0]['head_sha']='b'*40
        with self.assertRaises(ValueError):self.verify()
    def test_release_never_calls_prerequisite_workflows(self):
        release=Path('.github/workflows/audio-engine.yml').read_text()
        self.assertNotIn('uses: ./.github/workflows/',release)
        self.assertIn('needs: preflight',release)
        self.assertIn('python3 tools/ci/verify_ci_evidence.py',release)

    def test_newer_studio_gui_does_not_hide_live_evidence(self):
        self.data['gui']['checks'].append('gui-live')
        self.runs.append(dict(self.runs[1], id=100))
        def artifact(run_id, name):
            if run_id == 100:
                return dict(self.data[name], checks=['gui'])
            return self.data[name]
        result = verify_existing(SHA, 'live', False, self.runs, artifact, 'feature')
        self.assertTrue(any('run 2)' in line for line in result))
