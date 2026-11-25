"""Gate evidence selection regressions; no network, dispatch or build."""
import unittest
from pathlib import Path
from ci_evidence import definition_hash
from verify_ci_evidence import verify_existing, WORKFLOWS

SHA = 'a' * 40
class ExistingEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.runs = [dict(id=i, path='.github/workflows/'+WORKFLOWS[n], head_sha=SHA,
                          event='pull_request', status='completed', conclusion='success')
                     for i,n in enumerate(('core','gui','contracts','runtime'),1)]
        self.data = {n: dict(schema=1, sha=SHA, workflow=WORKFLOWS[n],
                            definition=definition_hash(WORKFLOWS[n]), checks=[n])
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

    def test_cancelled_parallel_pr_does_not_hide_manual_gui_pass(self):
        self.data['gui']['checks'].append('gui-live')
        self.runs[1]['event'] = 'workflow_dispatch'
        self.runs.append(dict(self.runs[1], id=100, event='pull_request',
                              conclusion='cancelled'))
        self.assertTrue(any('run 2)' in line for line in self.verify('live')))

    def test_cancelled_gui_without_valid_pass_still_blocks(self):
        self.runs[1]['conclusion'] = 'cancelled'
        with self.assertRaises(ValueError):
            self.verify()

    def test_cancelled_gui_rerun_still_blocks(self):
        self.runs.append(dict(self.runs[1], id=100, run_attempt=2,
                              conclusion='cancelled'))
        with self.assertRaises(ValueError):
            self.verify()

    def test_cancellation_does_not_hide_a_real_gui_failure(self):
        self.runs.append(dict(self.runs[1], id=99, conclusion='failure'))
        self.runs.append(dict(self.runs[1], id=100, conclusion='cancelled'))
        with self.assertRaisesRegex(ValueError, 'completed/failure'):
            self.verify()

    def test_cancellation_still_requires_valid_evidence(self):
        self.runs.append(dict(self.runs[1], id=100, conclusion='cancelled'))
        self.data['gui']['definition'] = 'old'
        with self.assertRaisesRegex(ValueError, 'does not match'):
            self.verify()

    def test_failed_or_pending_live_gui_still_blocks(self):
        self.data['gui']['checks'].append('gui-live')
        for status, conclusion in [('in_progress', None), ('completed', 'failure')]:
            with self.subTest(status=status):
                self.runs.append(dict(self.runs[1], id=100, status=status,
                                      conclusion=conclusion, event='workflow_dispatch',
                                      display_title='GUI compile (gui-live)'))
                with self.assertRaises(ValueError):
                    self.verify('live')
                self.runs.pop()

    def test_known_studio_only_dispatch_does_not_block_live(self):
        self.data['gui']['checks'].append('gui-live')
        for status, conclusion in [('in_progress', None), ('completed', 'failure')]:
            with self.subTest(status=status):
                self.runs.append(dict(self.runs[1], id=100, status=status,
                                      conclusion=conclusion, event='workflow_dispatch',
                                      display_title='GUI compile (gui)'))
                self.assertTrue(any('run 2)' in line for line in self.verify('live')))
                # Studio itself must still observe the failure/pending run.
                with self.assertRaises(ValueError):
                    self.verify('studio')
                self.runs.pop()

    def test_unknown_dispatch_or_pr_title_cannot_bypass_live_blocker(self):
        self.data['gui']['checks'].append('gui-live')
        for event, title in [('workflow_dispatch', 'legacy title'),
                             ('pull_request', 'GUI compile (gui)')]:
            with self.subTest(event=event):
                self.runs.append(dict(self.runs[1], id=100, status='in_progress',
                                      conclusion=None, event=event, display_title=title))
                with self.assertRaises(ValueError):
                    self.verify('live')
                self.runs.pop()

    def test_wrong_profile_evidence_is_validated_before_skipping(self):
        self.data['gui']['checks'].append('gui-live')
        self.runs.append(dict(self.runs[1], id=100))
        for invalid in ({'sha': 'b' * 40}, {'definition': 'wrong'},
                        {'schema': 0}, {'workflow': 'other.yml'}):
            with self.subTest(invalid=invalid):
                def artifact(run_id, name):
                    return (dict(self.data[name], checks=['gui'], **invalid)
                            if run_id == 100 else self.data[name])
                with self.assertRaises(ValueError):
                    verify_existing(SHA, 'live', False, self.runs, artifact, 'feature')
