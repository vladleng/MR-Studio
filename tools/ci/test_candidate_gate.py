"""Regression matrix for the release dependency gate, without network/builds."""
import unittest
from verify_candidate_gate import verify_candidate

class CandidateGateTests(unittest.TestCase):
    def test_product_scopes_require_every_contract(self):
        for scope in ('profile','studio','studio-live','separation','separation-ui'):
            for check in (('core','contracts','runtime','separation') if scope == 'separation' else ('core','gui','contracts','runtime','separation') if scope == 'separation-ui' else ('core','gui','contracts','runtime')):
                for result in ('failure','cancelled','skipped','',None):
                    with self.subTest(scope=scope,check=check,result=result):
                        results=dict.fromkeys(('core','gui','contracts','runtime','separation'),'success')
                        results[check]=result
                        with self.assertRaises(ValueError):verify_candidate(scope,False,results)
    def test_live_still_requires_core_and_gui(self):
        verify_candidate('live',False,dict(core='success',gui='success'))
        for check in ('core','gui'):
            with self.subTest(check=check):
                results=dict(core='success',gui='success');results[check]='skipped'
                with self.assertRaises(ValueError):verify_candidate('live',False,results)
    def test_integration_overrides_live(self):
        with self.assertRaises(ValueError):
            verify_candidate('live',True,dict(core='success',gui='success'))
    def test_success(self):
        verify_candidate('studio',False,dict.fromkeys(('core','gui','contracts','runtime','separation'),'success'))
    def test_unknown_scope_is_rejected(self):
        with self.assertRaises(ValueError):verify_candidate('typo',False,{})
