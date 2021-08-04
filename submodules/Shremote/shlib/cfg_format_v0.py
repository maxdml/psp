import yaml
from collections import defaultdict
from .cfg_formatter import CfgField, CfgMap, CfgMapList, CfgMapMap, CfgReference, NullType, TopLvlCfg
from .logger import log_warn
from .fmt_config import FmtConfig
from .include_loader import IncludeLoader
from .cfg_format import CfgFmt

def fix_programs_cfg(cfg):
    for prog_name, program in cfg['programs'].items():
        if 'enforce_duration' in program and program.enforce_duration:
            log_warn("program.enforce_duration has been renamed to program.duration_reduced_error")
            program.duration_reduced_error = True

def fix_commands_cfg(cfg):
    for command in cfg['commands']:
        if command.min_duration.format() is None:
            if 'enforce_duration' in command and command.enforce_duration is not True:
                log_warn("command.enforce_duration has been renamed to command.min_duration")
                command.min_duration = command.enforce_duration
        if command.max_duration.format() is None:
            if 'duration' in command:
                log_warn("command.duration has been renamed to command.max_duration")
                command.max_duration = command.duration

def mapify_cmd_list(cmds):
    new_cmds = []

    if isinstance(cmds, dict):
        log_warn("Changing cmd mapping into cmd list")
        for name, cmd in cmds.items():
            if isinstance(cmd, str):
                cmd = dict(name=name, cmd=cmd)
            else:
                cmd['name'] = name
            new_cmds.append(cmd)
    else:
        new_cmds = cmds

    return new_cmds

def mapify_cmds(cfg):
    if 'init_cmds' in cfg:
        cfg['init_cmds'] = mapify_cmd_list(cfg['init_cmds'])

    if 'post_cmds' in cfg:
        cfg['post_cmds'] = mapift_cmd_list(cfg['post_cmds'])

def unlistify_hosts(cfg):
    newhosts = {}
    for hostname, host in cfg['hosts'].items():
        if isinstance(host, list):
            log_warn("Changing list-host %s into host with list addr" % hostname)
            newhosts[hostname] = {'addr': []}
            for subhost in host:
                newhosts[hostname]['addr'].append(subhost['addr'])

    for hostname, host in newhosts.items():
        cfg['hosts'][hostname] = host

def listify_commands(cfg):
    if isinstance(cfg['commands'], dict):
        log_warn("Changing map of commands into list of commands")
        new_commands = []
        for name, cmd in cfg['commands'].items():
            if isinstance(cmd, list):
                for i, subcmd in enumerate(cmd):
                    subcmd['program'] = name
                    subcmd['i'] = i
                    new_commands.append(subcmd)
            else:
                cmd['program'] = name
                cmd['i'] = 0
                new_commands.append(cmd)
        cfg['commands'] = new_commands

def walk_dict_values(d):
    for k in d.keys():
        if isinstance(d[k], dict):
            for x in walk_dict_values(d[k]):
                yield x
        elif isinstance(d[k], str):
            yield d, k


def fix_format_substitutions(cfg):
    replacements = [
            ( '{0.log_dir}', '{0.output_dir}' ),
            ( '{local_out}', '{0.output_dir}' ),
            ( '{0.local_out}', '{0.output_dir}' ),
            ( '{0.remote_out}', '{host.output_dir}' ),
            ( '{remote_out}', '{host.output_dir}'),
            ( '{i}', '$({host_idx} + {i})'),
            ( '{log}', '$(os.path.join("{log_dir}","{program.log.log}"))')
    ]

    for parent, key in walk_dict_values(cfg):
        for before, after in replacements:
            orig = parent[key]
            new = orig.replace(before, after)

            if orig != new:
                log_warn("Modifying deprecated substitution in {} : {} -> {}".format(key, before, after))

            parent[key] = new

def likely_v0_cfg(cfg):
    if isinstance(cfg, str):
        with open(cfg, 'r') as f:
            cfg = yaml.load(f, Loader=IncludeLoader)

    if isinstance(cfg['commands'], dict):
        return True
    return False

def load_v0_cfg(cfg, fmt = CfgFmt()):
    if isinstance(cfg, str):
        with open(cfg, 'r') as f:
            cfg = yaml.load(f, Loader=IncludeLoader)

    unlistify_hosts(cfg)
    listify_commands(cfg)
    fix_format_substitutions(cfg)
    mapify_cmds(cfg)

    if 'log_dir' in cfg['programs']:
        log_dir = cfg['programs']['log_dir']
        if '{0.label}' in log_dir:
            log_dir = log_dir.replace('{0.label}','')
            log_warn("specifying {0.label} in log_dir no longer necessary")
        cfg['log_dir'] = log_dir
        del cfg['programs']['log_dir']

    cfg = FmtConfig(cfg)

    fmt.format(cfg)

    fix_programs_cfg(cfg)
    fix_commands_cfg(cfg)

    return cfg

if __name__ == '__main__':
    import sys
    if not likely_v0_cfg(sys.argv[1]):
        print("Config file {} likely not a v0 cfg".format(sys.argv[1]))
    else:
        print("Config file {} likely a v0 cfg".format(sys.argv[1]))

    cfg = load_v0_cfg(sys.argv[1])

    cfg.user = '__username__'
    cfg.label = '__shlabel__'
    cfg.cfg_dir = sys.argv[1]

    default_kwargs = dict(
            host_idx = -1,
            log_dir = 'prog_log_dir',
    )


    for cmd in cfg.commands:
        start = cmd.program.start.format(host_idx=0, host = cmd.hosts[0])
        begin = cmd.begin.format()
        print("Command '{}'\n\tstarts at time {}".format(start, begin))
