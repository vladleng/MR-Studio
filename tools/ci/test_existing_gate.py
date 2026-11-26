"""Gate evidence selection regressions; no network, dispatch or build."""
import unittest
from pathlib import Path
from ci_evidence import definition_hash
from verify_ci_evidence import verify_existing, verify_automatic, plan_missing, WORKFLOWS

SHA = 'a' * 40
class ExistingEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.runs = [dict(id=i, path='.github/workflows/'+WORKFLOWS[n], head_sha=SHA,
                          event='pull_request', status='completed', conclusion='success')
                     for i,n in enumerate(('core','gui','contracts','runtime','mp3'),1)]
        self.data = {n: dict(schema=1, sha=SHA, workflow=WORKFLOWS[n],
                            definition=definition_hash(WORKFLOWS[n]), checks=[n])
                     for n in ('core','gui','contracts','runtime','mp3')}
    def verify(self,scope='studio',integration=False):
        return verify_existing(SHA,scope,integration,self.runs,lambda _,n:self.data[n],'feature')
    def test_reuses_success_without_dispatch(self):
        self.assertEqual(len(self.verify()),5)
    def test_reports_every_missing_check_and_command(self):
        self.runs=[]
        with self.assertRaises(ValueError) as caught:self.verify()
        for name in ('core','gui','contracts','runtime','mp3'):
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
                self.assertEqual(len(self.verify(scope)), 5)

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
    def test_release_uses_only_explicit_missing_dependencies(self):
        release=Path('.github/workflows/audio-engine.yml').read_text()
        self.assertIn('types: [edited]', release)
        self.assertIn("!startsWith(github.event.changes.title.from, '[candidate:')", release)
        self.assertIn('needs: [plan, preflight]', release)
        self.assertIn("needs.plan.result == 'success' && needs.preflight.result == 'success'", release)
        self.assertIn("id: package\n        if: success()", release)
        self.assertIn('python3 tools/ci/verify_ci_evidence.py', release)
        for name in WORKFLOWS:
            self.assertIn("needs.plan.outputs.automatic == 'true' && needs.plan.outputs.need_" + name + " == 'true'", release)

    def test_plan_reuses_and_schedules_missing_only(self):
        self.runs = [r for r in self.runs if r['path'] != '.github/workflows/'+WORKFLOWS['runtime']]
        missing, pending = plan_missing(SHA, 'studio', False, self.runs, lambda _, n: self.data[n])
        self.assertEqual(missing, {'runtime'})
        self.assertFalse(pending)

    def test_plan_waits_for_pending_and_rejects_failure(self):
        self.runs[0]['status'] = 'in_progress'
        self.runs[0]['conclusion'] = None
        missing, pending = plan_missing(SHA, 'studio', False, self.runs, lambda _, n: self.data[n])
        self.assertFalse(missing)
        self.assertEqual(pending, {'core'})
        self.runs[0].update(status='completed', conclusion='failure')
        with self.assertRaisesRegex(ValueError, 'core:'):
            plan_missing(SHA, 'studio', False, self.runs, lambda _, n: self.data[n])

    def test_auto_gate_requires_actual_job_and_evidence(self):
        self.runs = [r for r in self.runs if r['path'] != '.github/workflows/'+WORKFLOWS['runtime']]
        needs = {name: {'result': 'skipped'} for name in WORKFLOWS}
        needs['plan'] = {'result': 'success', 'outputs': {
            'need_'+name: 'true' if name == 'runtime' else 'false' for name in WORKFLOWS}}
        needs['runtime']['result'] = 'success'
        def verify():
            return verify_automatic(SHA, 'studio', False, self.runs,
                                    lambda _, n: self.data[n], 'feature', needs, 200)
        self.assertEqual(len(verify()), 4)
        # A new standalone failure must block even if reused evidence was valid at planning time.
        self.runs[0].update(status='completed', conclusion='failure')
        with self.assertRaises(ValueError):
            verify()
        self.runs[0].update(status='completed', conclusion='success')
        needs['runtime']['result'] = 'skipped'
        with self.assertRaises(ValueError):
            verify()
        needs['runtime']['result'] = 'success'
        self.data['runtime']['sha'] = 'b'*40
        with self.assertRaises(ValueError):
            verify()


    def test_newer_studio_gui_does_not_hide_live_evidence(self):
        self.data['gui']['checks'].append('gui-live')
        self.runs.append(dict(self.runs[1], id=100))
        def artifact(run_id, name):
            if run_id == 100:
                return dict(self.data[name], checks=['gui'])
            return self.data[name]
        result = verify_existing(SHA, 'live', False, self.runs, artifact, 'feature')
        self.assertTrue(any('run 2)' in line for line in result))
