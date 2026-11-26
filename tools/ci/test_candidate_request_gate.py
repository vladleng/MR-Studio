import unittest
from candidate_request import resolve

SHA = 'a'*40


class CandidateRequestTests(unittest.TestCase):
    def setUp(self):
        self.env = dict(GITHUB_REPOSITORY='owner/repo', GITHUB_SHA='b'*40,
                        GITHUB_REF_NAME='feature', REQUEST_CHECKS='studio',
                        REQUEST_INTEGRATION='false', REQUEST_ARA='true')
        self.event = dict(action='edited', changes={'title': {'from': 'Title'}},
                          pull_request=dict(title='[candidate:studio] Title',
                              head=dict(sha=SHA, ref='feature', repo=dict(full_name='owner/repo')),
                              base=dict(ref='main')))

    def test_explicit_request_uses_head_not_merge(self):
        result = resolve('pull_request', self.event, self.env)
        self.assertEqual(result['sha'], SHA)
        self.assertEqual(result['scope'], 'studio')
        self.assertEqual(result['ara'], 'true')
        self.assertEqual(result['automatic'], 'true')

    def test_studio_ui_request(self):
        self.event['pull_request']['title'] = '[candidate:studio-ui] Title'
        result = resolve('pull_request', self.event, self.env)
        self.assertEqual(result['scope'], 'studio-ui')
        self.assertEqual(result['sha'], SHA)
        self.assertEqual(result['ara'], 'true')
        self.env['REQUEST_CHECKS'] = 'studio-ui'
        self.assertEqual(resolve('workflow_dispatch', {}, self.env)['scope'], 'studio-ui')

    def test_live_ara_off(self):
        self.event['pull_request']['title'] = '[candidate:live] Title'
        self.assertEqual(resolve('pull_request', self.event, self.env)['ara'], 'false')

    def test_body_edit_and_persistent_marker_do_not_request(self):
        self.event['changes'] = {'body': {'from': 'old'}}
        with self.assertRaises(ValueError):
            resolve('pull_request', self.event, self.env)
        self.event['changes'] = {'title': {'from': '[candidate:studio] Old'}}
        with self.assertRaises(ValueError):
            resolve('pull_request', self.event, self.env)

    def test_normal_title_and_wrong_base_rejected(self):
        self.event['pull_request']['title'] = 'Normal title'
        with self.assertRaises(ValueError):
            resolve('pull_request', self.event, self.env)
        self.event['pull_request']['title'] = '[candidate:studio] Title'
        self.event['pull_request']['base']['ref'] = 'other'
        with self.assertRaises(ValueError):
            resolve('pull_request', self.event, self.env)

    def test_fork_rejected(self):
        self.event['pull_request']['head']['repo']['full_name'] = 'other/repo'
        with self.assertRaises(ValueError):
            resolve('pull_request', self.event, self.env)

    def test_manual_dispatch_unchanged(self):
        result = resolve('workflow_dispatch', {}, self.env)
        self.assertEqual(result['sha'], 'b'*40)
        self.assertEqual(result['automatic'], 'false')
