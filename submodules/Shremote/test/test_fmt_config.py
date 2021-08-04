from test_files import include_loader, fmt_config
import unittest
import yaml

FmtConfig = fmt_config.FmtConfig

def load_raw_cfg(s):
    return yaml.load(s, include_loader.IncludeLoader)

class TestFmtConfig(unittest.TestCase):

    def test_eval_format(self):
        eval_cfg = '''
x: 5
five_as: $('a' * 5)
ten: $({0.x} * 2)
'''
        raw_cfg = load_raw_cfg(eval_cfg)
        cfg = FmtConfig(raw_cfg, formattable=True)

        x = cfg.formatted("five_as")
        self.assertEqual(x, 'a'*5)

        x = cfg.formatted("ten")
        self.assertEqual(x, 10)

    def test_no_eval_format(self):
        no_eval_cfg = '''
x: $$(foobar)
y: $$({0.x})
'''
        raw_cfg = load_raw_cfg(no_eval_cfg)
        cfg = FmtConfig(raw_cfg, formattable=True)

        x = cfg.formatted('x')
        self.assertEqual(x, '$(foobar)')

        x = cfg.formatted('y')
        self.assertEqual(x, '$($(foobar))')
