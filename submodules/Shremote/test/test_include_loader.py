import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

from shlib.include_loader import IncludeLoader, YmlImportException
import unittest
import yaml


class TestLoader(unittest.TestCase):

    def test_full_import(self):
        cfg = '''
imported: !import sample_cfgs/includable.yml
'''
        raw_cfg = yaml.load(cfg, IncludeLoader)
        with open('sample_cfgs/includable.yml') as f:
            included_cfg = yaml.load(f, IncludeLoader)

        self.assertEqual(raw_cfg['imported'], included_cfg)

    def test_1_partial_import(self):
        cfg = '''
imported: !import sample_cfgs/includable.yml::entry_one
'''
        raw_cfg = yaml.load(cfg, IncludeLoader)
        with open('sample_cfgs/includable.yml') as f:
            included_cfg = yaml.load(f, IncludeLoader)

        self.assertEqual(raw_cfg['imported'], included_cfg['entry_one'])

    def test_2_partial_import(self):
        cfg = '''
imported: !import sample_cfgs/includable.yml::entry_one::sub_key1
'''
        raw_cfg = yaml.load(cfg, IncludeLoader)
        with open('sample_cfgs/includable.yml') as f:
            included_cfg = yaml.load(f, IncludeLoader)

        self.assertEqual(raw_cfg['imported'], included_cfg['entry_one']['sub_key1'])

    def test_dne_import(self):
        cfg = '''
imported: !import sample_cfgs/includable.yml::entry_one::NONEXISTENT_KEY2
'''
        try:
            raw_cfg = yaml.load(cfg, IncludeLoader)
            self.assertTrue(False, "Importing nonexistent key did not fail")
        except YmlImportException:
            pass

    def test_inherit_no_override(self):
        cfg = '''
inherited: !inherit |
    sample_cfgs/includable.yml::entry_one
'''
        raw_cfg = yaml.load(cfg, IncludeLoader)
        with open('sample_cfgs/includable.yml') as f:
            included_cfg = yaml.load(f, IncludeLoader)

        self.assertEqual(raw_cfg['inherited'], included_cfg['entry_one'])

    def test_inherit_override(self):
        cfg = '''
inherited: !inherit |
    sample_cfgs/includable.yml::entry_one
    sub_key2: new_value2
'''
        raw_cfg = yaml.load(cfg, IncludeLoader)
        with open('sample_cfgs/includable.yml') as f:
            included_cfg = yaml.load(f, IncludeLoader)

        self.assertEqual(raw_cfg['inherited']['sub_key1'],
                         included_cfg['entry_one']['sub_key1'])

        self.assertEqual(raw_cfg['inherited']['sub_key2'], 'new_value2')

    def test_inherited_merge(self):
        cfg = '''
inherited: !inherit |
    sample_cfgs/includable.yml
    entry_one:
        sub_key2: new_value2
        sub_key3: new_value3
'''
        raw_cfg = yaml.load(cfg, IncludeLoader)
        with open('sample_cfgs/includable.yml') as f:
            included_cfg = yaml.load(f, IncludeLoader)


        self.assertEqual(raw_cfg['inherited']['entry_one']['sub_key1'],
                         included_cfg['entry_one']['sub_key1'])
        self.assertEqual(raw_cfg['inherited']['entry_one']['sub_key2'],
                         'new_value2')
        self.assertEqual(raw_cfg['inherited']['entry_one']['sub_key3'],
                         'new_value3')


