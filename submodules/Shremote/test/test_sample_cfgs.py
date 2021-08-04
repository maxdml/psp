import unittest
import test_files
import os
import shutil
from shlib.cfg_format import load_cfg as load_cfg_file
from shlib.fmt_config import CfgFormatException, CfgKeyError
from shremote import ShRemote

class TestSampleCfgs(unittest.TestCase):

    TEST_OUTPUT_DIR='test_output'

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.TEST_OUTPUT_DIR, ignore_errors=True)

    def test_all_sample_cfgs(self):
        cfgs = [
            'simple_cfg.yml',
            'default_args_test.yml',
            'test_computed_fields.yml',
            'test_escaped_computation.yml',
            'test_escaped_reference.yml',
            'multi_host_cfg.yml',
            'uses_computed_fields.yml'
        ]

        for cfg in cfgs:
            print("Testing cfg %s\n" % cfg)
            ShRemote(os.path.join('sample_cfgs', cfg), 'cfg_test', self.TEST_OUTPUT_DIR, {}, True)

    def test_succeeds_on_provided_computed_fields(self):
        cfg = load_cfg_file('sample_cfgs/uses_computed_fields.yml')
        cfg.user = 'username'
        cfg.label = 'label'
        cfg.cfg_dir = '.'
        cfg.output_dir = './output'
        for cmd in cfg['commands']:
            cmd.program.start.format(host_idx = 0, log_dir='./logs', host = cmd.hosts[0])

        for file in cfg['files'].values():
            file.src.format(host=file.hosts[0])
            file.dst.format(host=file.hosts[0])

    def test_fails_on_unprovided_computed_field(self):
        cfg = load_cfg_file("sample_cfgs/uses_computed_fields.yml")
        for command in cfg['commands']:
            threw_exception = False
            try:
                command.program.start.format()
            except (CfgFormatException, CfgKeyError) as e:
                threw_exception = True
            self.assertTrue(threw_exception, "Command {}: {} did not throw an exception when formatted".format(command.program.name, command.program.start.get_raw()))

        for file in cfg['files'].values():
            threw_exception = False
            try:
                file.src.format()
            except (CfgFormatException, CfgKeyError) as e:
                threw_exception = True
            self.assertTrue(threw_exception, "File {} : {} did not throw an exception".format(file.name, file.src.get_raw()))
            try:
                file.dst.format()
            except (CfgFormatException, CfgKeyError) as e:
                threw_exception = True
            self.assertTrue(threw_exception, "File {} : {} did not throw an exception".format(file.name, file.dst.get_raw()))

    def test_default_args(self):
        cfg = load_cfg_file("sample_cfgs/default_args_test.yml")
        start1 = cfg.commands[0].to_echo.format(host=cfg.commands[0].hosts[0])
        self.assertEqual(start1, "Hello world!", "Default was not applied")
        start2 = cfg.commands[1].to_echo.format(host=cfg.commands[1].hosts[0])
        self.assertEqual(start2, "Goodbye local", "Default was not overridden,  instead: %s" % start2)

    def test_evaled_fields(self):
        cfg = load_cfg_file("sample_cfgs/test_computed_fields.yml")
        self.assertEqual(cfg.commands[0].begin.format(), 42,
                         "Begin (numerical) computation not applied")
        self.assertEqual(cfg.commands[0].to_echo.format(), 'x' * 42,
                         "to_echo (str) computation not applied")
        self.assertEqual(cfg.commands[0].program.start.format(), 'echo "%s"' % ('x' * 42),
                         "program.start (str reference) computation not applied")


    def test_escaped_evaled_fields(self):
        cfg = load_cfg_file("sample_cfgs/test_escaped_computation.yml")
        self.assertEqual(cfg.commands[0].computed.format(), 'x' * 42,
                         "Computed field not set properly")
        escaped = cfg.commands[0].escaped.format()
        self.assertEqual(escaped, "$( 'x' * 10 )",
                         "Escaped computation not properly evaluated: {}".format(escaped))
        referenced = cfg.commands[0].referenced.format()
        self.assertEqual(referenced, "$( 'x' * 10 )",
                         "referenced computation not properly evaluated: {}".format(referenced))

    def test_escaped_reference(self):
        cfg = load_cfg_file("sample_cfgs/test_escaped_reference.yml")
        self.assertEqual(cfg.commands[0].reference.format(), 'ref',
                         "Referenced field not substituted")
        escaped = cfg.commands[0].escaped.format()
        self.assertEqual(escaped, "{0.referenceable}",
                         "Escaped reference not properly formatted: {}".format(escaped))
        self.assertEqual(cfg.commands[0].dne.format(), "{0.DNE}",
                         "Escaped DNE reference not properly formatted")
        escaped = cfg.commands[0].escaped_reference.format()

        self.assertEqual(escaped, "{0.referenceable}",
                         "Reference to escaped not properly formatted: {}".format(escaped))

if __name__ == '__main__':
    unittest.main()
