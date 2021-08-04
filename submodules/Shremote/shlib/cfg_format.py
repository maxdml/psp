import yaml
from collections import defaultdict
from .cfg_formatter import CfgField, CfgMap, CfgMapList, CfgMapMap, CfgReference, NullType, TopLvlCfg
from .fmt_config import FmtConfig
from .include_loader import IncludeLoader

class SshCfg(CfgMap):
    _fields = [
            CfgField('user', str, '{0.user}'),
            CfgField('key', str, '~/.ssh/id_rsa'),
            CfgField('port', int, 22),
    ]

class CmdsCfg(CfgMapList):
    _fields = [
            CfgField('cmd', str, required=True),
            CfgField('checked_rtn', [int, bool, NullType], default=0, aliases=('check_rtn')),
    ]

class HostsCfg(CfgMapMap):
    _fields = [
            CfgField('hostname', None, required=True, aliases=('addr')),
            #FIXME: to enable $(...) expanding to a list, hostname cannot set list_ok
            CfgField('name', str, aliases='_name'),
            CfgMap('ssh', inherit=SshCfg),
            CfgField('log_dir', str, default="{0.log_dir}"),
            CfgField('sudo_passwd', str, default=None),
            CfgField('enabled', bool, default=True)
    ]
    _reserved_fields = [
            CfgField('output_dir', str, default="$(os.path.join('{host.log_dir}', '{0.label}'))")
    ]

class FilesCfg(CfgMapMap):
    _fields = [
            CfgField('src', str, required=True),
            CfgField('dst', str, required=True),
            CfgField('name', str, aliases='_name'),
            CfgField('enabled', bool, default=True),
            CfgReference('hosts', HostsCfg, list_ok = True, required=True, aliases=('host'))
    ]
    _computed_fields = [
            CfgField('host', lambda : defaultdict(str))
    ]

class ProgramLogCfg(CfgMap):
    _fields = [
            CfgField('dir', str, default=''),
            CfgField('out', str),
            CfgField('err', str),
            CfgField('append', bool, default=False)
    ]


class ProgramsCfg(CfgMapMap):
    _fields = [
            CfgReference('hosts', HostsCfg, list_ok = True, aliases=('host')),
            CfgField('name', str, aliases=('_name')),
            CfgField('start', str, required=True),
            CfgField('stop', str, default=None),
            # TODO: Add 'kill' field
            ProgramLogCfg('log'),
            CfgField('duration_reduced_error', bool, default=True),
            CfgField('duration_exceeded_error', bool, default=False),
            CfgField('bg', bool, default=False),
            CfgField('checked_rtn', [int, bool, NullType], default=None, aliases=('check_rtn')),
            CfgField('sudo', bool, default=False),
            CfgField('defaults', dict, default=dict())
    ]

class CommandsCfg(CfgMapList):

    _fields = [
            CfgReference('program', ProgramsCfg, required=True),
            CfgReference('hosts', HostsCfg, sibling_inherit=['program', 'hosts'], aliases=('host'), list_ok = True),
            CfgField('begin', float, required=False),
            CfgField('name', str, default='{program.name}'),
            CfgField('min_duration', [float, NullType], default=None),
            CfgField('max_duration', [float, NullType], default=None),
            CfgField('start_after', str, required=False),
            CfgField('stop_after', str, required=False),
            CfgField('enabled', bool, default=True)
    ]

    _reserved_fields = [
            CfgField('log_dir', str, default="$(os.path.join('{host.output_dir}', '{program.log.dir}'))")
    ]


    _computed_fields = [
            CfgField('host_idx', int),
            CfgField('host', lambda : defaultdict(str)),
            CfgField('pid', int)
    ]

    _child_inherit = ['program', 'defaults']

class CfgFmt(TopLvlCfg):
    _fields = [
            CfgField('log_dir', str, default="~/shremote_logs"),
            SshCfg('ssh'),
            CmdsCfg('init_cmds', format_root=True),
            CmdsCfg('post_cmds', format_root=True),
            HostsCfg('hosts'),
            FilesCfg('files', format_root=True),
            ProgramsCfg('programs'),
            CommandsCfg('commands', format_root=True)
    ]

    _computed_fields = [
            CfgField('user', str),
            CfgField('label', str),
            CfgField('args', defaultdict),
            CfgField('output_dir', str),
            CfgField('cfg_dir', str),
    ]

def load_cfg(cfg, fmt = CfgFmt()):
    if isinstance(cfg, str):
        with open(cfg, 'r') as f:
            cfg = yaml.load(f, Loader=IncludeLoader)
    cfg = FmtConfig(cfg)
    fmt.format(cfg)
    return cfg

if __name__ == '__main__':
    import sys
    cfg = load_cfg(sys.argv[1])

    cfg.user = 'YourUsername'
    cfg.label = 'ShremoteLabel'
    cfg.cfg_dir = sys.argv[1]

    default_kwargs = dict(
            host_idx = -1,
            log_dir = 'prog_log_dir',
    )

    for cmd in cfg.commands:
        start = cmd.program.start.format(host=cmd.hosts[0], **default_kwargs)
        begin = cmd.begin.format()
        print("Command '{}'\n\tstarts at time {}".format(start, begin))
