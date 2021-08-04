#!/usr/bin/env python3

from __future__ import print_function

from shlib.local_process import start_local_process, exec_locally
from shlib.remote_process import start_remote_process
from shlib.cfg_format import load_cfg
from shlib.fmt_config import CfgFormatException
from shlib.include_loader import IncludeLoader
from shlib.logger import * # log*(), set_logfile(), close_logfile()

from shlib.cfg_format_v0 import likely_v0_cfg, load_v0_cfg

from collections import namedtuple, defaultdict
import sys
import re
import threading # For threading.Event
import argparse
import pprint
import os
import json
import signal
import itertools
import textwrap
import time
import copy

class ShException(Exception):
    pass

class ShLocalCmd(object):

    def __init__(self, cfg, event=None):
        self.cmd = cfg.cmd.format().replace('\n', ' ')
        self.checked_rtn = cfg.checked_rtn.format()
        self.event = event

    def execute(self):
        log_info("Executing %s" % self.cmd)
        p = start_local_process(self.cmd, self.event, shell=True,
                                checked_rtn = self.checked_rtn)
        p.join()

class ShHost(object):

    RSYNC_FROM_CMD = \
            'rsync -av -e "ssh -p {ssh.port} -i {ssh.key}" "{ssh.user}@{addr}:{src}" "{dst}"'

    RSYNC_TO_CMD = \
            'rsync -av -e "ssh -p {ssh.port} -i {ssh.key}" "{src}" "{ssh.user}@{addr}:{dst}"'

    @classmethod
    def create_host_list(cls, cfg_hosts):
        hosts = []
        for host in cfg_hosts:
            if host.enabled.format():
                hostname = host.hostname.format()
                # TODO: This is a consequence of the fact that if list_ok is true,
                # the formatter will wrap the first element in a list, regardless
                # of whether it will expand to a list when formatted.
                # Thus, list_ok cannot be true for hostname, which may be evaluated
                # dynamically
                if isinstance(hostname, str):
                    hosts.append(cls(host, hostname))
                else:
                    hosts.extend([cls(host, name.format()) for name in hostname])
        return hosts

    def __init__(self, cfg, hostname):
        self.name = cfg.name.format()
        self.addr = hostname
        self.ssh = cfg.ssh
        self.cfg = cfg
        self.sudo_passwd = cfg.sudo_passwd.format()
        label = cfg.get_root().label.format()
        self.log_dir = os.path.join(cfg.log_dir.format(), label)

    def __eq__(self, other):
        return self.addr == other.addr and self.log_dir == other.log_dir

    def __hash__(self):
        return hash(self.addr + self.log_dir)

    def copy_from(self, src, dst, background=False, event=None):
        p = start_local_process(['mkdir', '-p', os.path.expanduser(dst)], event, shell=False, checked_rtn = 0)
        p.join()

        cmd = self.RSYNC_FROM_CMD.format(src = src, dst = dst, addr = self.addr, ssh = self.ssh)
        p = start_local_process(cmd, None, shell=True, checked_rtn = 0)
        if not background:
            p.join()
        return p

    def copy_to(self, src, dst, background=False, event=None):
        cmd = self.RSYNC_TO_CMD.format(src = src, dst = dst, addr = self.addr, ssh = self.ssh)
        p = start_local_process(cmd, event, shell=True, checked_rtn = 0)
        if not background:
            p.join()
        return p

    def exec_cmd(self, cmd, event=None, background=True, log_entry = None, name = None, do_sudo = False, **kwargs):
        proc = start_remote_process(cmd, self.ssh, self.addr, event, log_entry, name,
                                    do_sudo, self.sudo_passwd, **kwargs)
        if not background:
            proc.join()
        return proc

class ShFile(object):

    def __init__(self, cfg, local_out):
        self.name = cfg.name.format()
        self.hosts = ShHost.create_host_list(cfg.hosts)
        if len(self.hosts) == 0:
            log_error("File %s is to be uploaded to only disabled hosts" % self.name)
            raise ShException("No enabled hosts")
        self.local_out = os.path.join(local_out, cfg.get_root().label.format())
        self.cfg_src = cfg.src
        self.cfg_dst = cfg.dst

    def validate(self):
        for host in self.hosts:
            try:
                self.cfg_src.format(host = host.cfg)
            except (KeyError, CfgFormatException) as e:
                log_error("Error formatting source of file {}: {}".format(self.name, e))
                raise

            try:
                self.cfg_dst.format(host = host.cfg)
            except (KeyError, CfgFormatException) as e:
                log_error("Error formatting dest of file {}: {}".format(self.name, e))
                raise

    def copy_to_host(self, event=None, background=True):
        procs = []
        for host in self.hosts:
            src = self.cfg_src.format(host = host.cfg)
            dst = self.cfg_dst.format(host = host.cfg)
            log_info("Copying {} to host {}".format(src, host.name))
            procs.append(host.copy_to(src, dst, background=background, event=event))
        return procs

class ShLog(object):

    DIRS_COPIED = set()

    def __init__(self, cfg):
        self.subdir = cfg.dir
        self.do_append = cfg.append.format()

        if 'out' in cfg:
            self.out = cfg.out
        else:
            self.out = None

        if 'err' in cfg:
            self.err = cfg.err
        else:
            self.err = None

    def assert_no_overlap(self, other, self_host, other_host):
        if self.do_append and other.do_append:
            return

        if not self.subdir.format(host_idx=0, host=self_host) == \
                other.subdir.format(host_idx=0, host=other_host):
            return

        if self.out is not None:
            if self.out.format(host_idx=0, host=self_host) == \
                    other.out.format(host_idx=0, host=other_host):
                raise ShException("Overlapping output log file: {}".format(self.out.format(host_idx=0)))

        if self.err is not None:
            if self.err.format(host_idx=0, host = self_host) == \
                    other.err.format(host_idx=0, host = other_host):
                raise ShException("Overlapping error log file: {}".format(self.err.format(host_idx=0)))

    def log_dir(self, host, host_idx):
        return os.path.join(host.log_dir, self.subdir.format(host_idx=host_idx))

    def suffix(self, host, host_idx):
        if self.do_append:
            redir = '>>'
        else:
            redir = '>'

        suffix = ''
        if self.out is not None:
            suffix += ' {} {}'.format(redir, os.path.join(host.log_dir,
                                                  self.subdir.format(host_idx = host_idx,
                                                                     host = host.cfg),
                                                  self.out.format(host_idx = host_idx,
                                                                  host = host.cfg)))
        if self.err is not None:
            suffix += ' 2{} {}'.format(redir, os.path.join(host.log_dir,
                                                   self.subdir.format(host_idx = host_idx,
                                                                      host = host.cfg),
                                                   self.err.format(host_idx = host_idx,
                                                                   host = host.cfg)))
        return suffix

    def remote_directories(self, hosts):
        dirs = set()
        for i, host in enumerate(hosts):
            dirs.add((host, os.path.join(host.log_dir, self.subdir.format(host_idx = i))))
        return dirs

    def copy_local(self, hosts, local_dir, event = None, background=False):
        threads = []

        for i, host in enumerate(hosts):
            remote_out = os.path.join(host.log_dir, self.subdir.format(host_idx = i))

            if (host.addr, remote_out) in self.DIRS_COPIED:
                continue

            exec_locally(["mkdir", "-p", local_dir])

            threads.append(host.copy_from(remote_out, local_dir, background=True))

            self.DIRS_COPIED.add((host.addr, remote_out))

        if background:
            return threads
        else:
            for thread in threads:
                thread.join()

class ShProgram(object):

    def __init__(self, cfg):
        self.name = cfg.name.format()
        self.log = ShLog(cfg.log)
        self.start = cfg.start
        self.stop = cfg.get('stop', None)
        self.shorter_error = cfg.duration_reduced_error.format()
        self.longer_error = cfg.duration_exceeded_error.format()
        self.checked_rtn = cfg.checked_rtn.format()
        self.background = cfg.bg.format()
        self.sudo = cfg.sudo.format()

    def start_cmd(self, host, host_idx):
        log_dir = self.log.log_dir(host, host_idx)
        return self.start.format(host_idx = host_idx,
                                 log_dir = log_dir,
                                 host = host.cfg) +\
                self.log.suffix(host, host_idx)

    def stop_cmd(self, host, host_idx, start_pid):
        if self.stop is not None:
            return self.stop.format(host_idx = host_idx, host = host.cfg, pid = start_pid)
        else:
            return None

class ShCommand(object):

    def __repr__(self):
        return "<ShCommand object: %s.%s>" % (self.name, self.begin)

    def __init__(self, cfg, event):
        self.cfg = cfg
        self.event = event
        self.name = cfg.name.format()
        self.begin = cfg.get('begin', None).format()
        self.start_trigger_name = cfg.get('start_after', None).format()
        self.stop_trigger_name = cfg.get('stop_after', None).format()
        self.start_trigger = None
        self.stop_trigger = None

        self.program = ShProgram(cfg.program)
        self.hosts = ShHost.create_host_list(cfg.hosts)
        if len(self.hosts) == 0:
            log_error("Hosts for command %s are all disabled" % self.name)
            raise ShException("No enabled hosts")
        self.max_duration = cfg.max_duration.format()
        self.min_duration = cfg.min_duration.format()
        self.log_entries = []
        self.processes = []
        self.dependent_starts = []
        self.dependent_stops = []
        self.started = False
        self.started_time = None
        self.stopped = False

        if self.max_duration is not None and self.begin is not None:
            self.end = self.begin + self.max_duration
        else:
            self.end = None

        if self.min_duration and not self.program.shorter_error:
            log_warn("Min duration specified but shorter_duration_error is false for: {}"
                     .format(self.pformat()))
            self.min_duration = None

        log("Preparing formatted trees for command %s" % self.name)
        self.formatted_trees = []
        for i, (host, start_cmd) in enumerate(self.start_cmds()):
            self.formatted_trees.append(self.formatted(host, i))

    def dependencies_contain(self, cmd):
        if self.start_trigger is not None:
            if self.start_trigger == cmd:
                return True

            if self.start_trigger.dependencies_contain(cmd):
                return True

        if self.stop_trigger is not None:
            if self.stop_trigger == cmd:
                return True

            if self.stop_trigger.dependencies_contain(cmd):
                return True

        return False


    def add_start_dependency(self, cmd):
        self.dependent_starts.append(cmd)
        cmd.start_trigger = self

    def add_stop_dependency(self, cmd):
        self.dependent_stops.append(cmd)
        cmd.stop_trigger = self

    def get_logs(self, local_dir, event=None):
        return self.program.log.copy_local(self.hosts, local_dir, background=True, event=event)

    def remote_log_directories(self):
        return self.program.log.remote_directories(self.hosts)

    def raw(self):
        self_dict = self.cfg.get_raw()
        if 'host' in self_dict['program']:
            del self_dict['program']['host']
        return self_dict

    def formatted(self, host, host_idx):
        host_only_cfg = copy.deepcopy(self.cfg)
        host_only_cfg.hosts = host.cfg
        host_only_cfg.program.hosts = host.cfg
        host_only_cfg.set_formattable()
        return host_only_cfg.format_tree(host=host.cfg, host_idx=host_idx, pid='__pid__')

    def pformat(self):
        return pprint.pformat(self.raw())

    def check_overlapping_logs(self, other):
        try:
            self.program.log.assert_no_overlap(other.program.log, self.hosts[0].cfg, other.hosts[0].cfg)
        except ShException:
            log_error("Instances of two commands log to the same file,"
                      "and will clobber each other:")
            log_error(self.cfg.program.log.pformat())
            raise

    def validate(self):
        if self.begin is None and self.start_trigger_name is None:
            log_error("Must provide either 'begin' or 'start_after' for all commands")
            raise ShException("No start trigger for command %s" % (self.name))

        if self.dependencies_contain(self):
            raise ShException("Dependency loop encountered for command %s" % (self.name))

        for i, host in enumerate(self.hosts):
            try:
                start_cmd = self.program.start_cmd(host, i)
            except KeyError as e:
                log_error("Error validating command %s: %s" % (self.program.start.get_raw(), e))
                raise

        try:
            stop_cmd = self.program.stop_cmd(self.hosts[0], 0, 'pid')
        except KeyError as e:
            log_error("Error validating command %s: %s" % (self.program.stop.get_raw(), e))
            raise

        if self.program.background and self.max_duration is not None and stop_cmd is None:
            log_error("Must specify stop_cmd if program is backgrounded "
                      "and max_duration is specified: {}".format(start_cmd))
            raise Exception("Program would not be stoppable")

        if self.program.background and self.min_duration is not None:
            log_error("Cannot specify min_duration for a backgrounded program: {}"
                      .format(start_cmd))

    def cmd_text_iter(self):
        for i, host in enumerate(self.hosts):
            start_cmd = self.program.start_cmd(host, i)
            stop_cmd = self.program.stop_cmd(host, i, '__pid__')
            yield host, start_cmd, stop_cmd

    def start_cmds(self):
        for i, host in enumerate(self.hosts):
            start_cmd = self.program.start_cmd(host, i)
            yield host, start_cmd

    def stop_cmds(self):
        for i, (proc, host) in enumerate(zip(self.processes, self.hosts)):
            stop_cmd = self.program.stop_cmd(host, i, proc.first_line)
            yield host, proc, stop_cmd

    def join(self):
        for proc in self.processes:
            if not proc.joined:
                proc.join()

    def running(self):
        if not self.started:
            return False
        return not self.exited()

    def exited(self):
        if self.program.background:
            return self.stopped
        for proc in self.processes:
            if not proc.exited:
                return False
        return True

    def started_dependencies(self):
        deps = []
        for cmd in self.dependent_starts:
            if cmd.started:
                deps.append(cmd)
                deps.extend(cmd.started_dependencies())
        return deps

    def ready_start_dependencies(self):
        if (not self.started) or (not self.exited()):
            return []
        ready = []
        for cmd in self.dependent_starts:
            if not cmd.started:
                ready.append(cmd)
        return ready

    def ready_stop_dependencies(self):
        if (not self.started) or (not self.exited()):
            return []
        ready = []
        for cmd in self.dependent_stops:
            if cmd.running():
                ready.append(cmd)
        return ready

    def stop(self, kill_pid = False):
        log("Attempting stop of", self.name)
        for (host, proc, stop_cmd), log_entry in zip(self.stop_cmds(), self.log_entries):
            cmd_name = "STOP %s on %s" % (self.name, host.name)
            if not self.program.background and proc.exited:
                log("Process %s already exited" % cmd_name)
                proc.join()
                continue

            if kill_pid:
                log("LAST ATTEMPT TO KILL %s" % cmd_name)
                if self.program.sudo:
                    cmd = 'sudo kill -9'
                else:
                    cmd = 'kill -9'
                host.exec_cmd("%s %s" % (cmd, proc.first_line), background=False,
                              name = "kill %s " % cmd_name,  log_end = False, do_sudo = self.program.sudo)
            elif stop_cmd is not None:
                host.exec_cmd(stop_cmd, background = False,
                              name = "stop %s " % cmd_name , log_end = False, do_sudo = self.program.sudo)

            if self.program.background:
                log_entry['stop_time_'] = float(time.time())

        self.stopped = True
            # NOTE: Not joining process here - might need more time to shut down
            # and don't want to block execution

    def start(self, log_entry):
        self.started = True
        self.started_time = time.time()
        max_dur = None if self.program.stop is not None else self.max_duration
        for i, (host, start_cmd) in enumerate(self.start_cmds()):
            cmd_name = "%s on %s" % (self.name, host.addr)
            host_log_entry = {}

            host_log_entry['addr_'] = host.addr
            host_log_entry['start_'] = start_cmd
            host_log_entry['time_'] = float(time.time())

            log_info("Executing %s : %s" % (cmd_name, start_cmd))
            if not self.program.background:
                p = host.exec_cmd(start_cmd, self.event,
                                  background=True, log_entry = host_log_entry,
                                  name = cmd_name,
                                  min_duration = self.min_duration,
                                  max_duration = max_dur,
                                  checked_rtn = self.program.checked_rtn,
                                  log_end = True,
                                  do_sudo = self.program.sudo)
            else:
                p = host.exec_cmd(start_cmd, self.event,
                                  background=False,
                                  name = cmd_name,
                                  checked_rtn = self.program.checked_rtn,
                                  max_duration = max_dur,
                                  log_end = True,
                                  do_sudo = self.program.sudo)
            self.processes.append(p)

            host_log_entry.update(self.formatted_trees[i])
            self.log_entries.append(host_log_entry)
            log_entry.append(host_log_entry)

class ShRemote(object):

    CommandInstance = namedtuple("cmd_instance", ["time", "is_stop", "cmd"])

    def sigint_handler(self, signal, frame):
        if self.interrupts_attempted == 0:
            log_error("CTRL+C PRESSED!")
            self.event.set()
        if self.interrupts_attempted == 1:
            log_error("CTRL+C PRESSED AGAIN!")
            log_warn("Interrupt one more time to skip waiting for processes to finish")
        if self.interrupts_attempted ==2 :
            log_error("CTRL+C PRESSED AGAIN! Processes may now be left running!!!")
        if self.interrupts_attempted > 2:
            log_error("CTRL+C PRESSED EVEN MORE! BE PATIENT!")
        self.interrupts_attempted += 1

    def __init__(self, cfg_file, label, out_dir, args_dict, suppress_output):
        self.event = threading.Event()
        self.interrupts_attempted = 0
        signal.signal(signal.SIGINT, self.sigint_handler)

        self.output_dir = os.path.expanduser(os.path.join(out_dir, label, ''))
        log("Making output directory: %s" % self.output_dir)
        if not suppress_output:
            exec_locally(['mkdir', '-p', self.output_dir])
            set_logfile(os.path.join(self.output_dir, 'shremote.log'))
        log("Made output dir")

        self.cfg_file = cfg_file
        log("Loading %s" % cfg_file)

        try:
            self.cfg = load_cfg(cfg_file)
        except Exception:
            if likely_v0_cfg(cfg_file):
                log_warn("Exception encountered loading {}. "
                         "Attempting to fall back to older cfg format".format(cfg_file))
                self.cfg = load_v0_cfg(cfg_file)
            else:
                raise

        self.cfg.args = args_dict
        self.cfg.label = label
        self.cfg.user = os.getenv('USER')
        if os.path.dirname(cfg_file):
            self.cfg.cfg_dir = os.path.dirname(cfg_file)
        else:
            self.cfg.cfg_dir = '.'
        self.cfg.output_dir = self.output_dir
        log("Assuming user is : %s" % self.cfg.user)

        self.label = label


        self.commands = [ShCommand(cmd, self.event) for cmd in self.cfg.commands if cmd.enabled.format()]

        cmd_names = defaultdict(list)
        self.timed_commands = []
        for command in self.commands:
            if command.begin is not None:
                self.timed_commands.append(self.CommandInstance(command.begin, False, command))
                cmd_names[command.name].append(command)
            if command.end is not None:
                self.timed_commands.append(self.CommandInstance(command.end, True, command))

        self.timed_commands = sorted(self.timed_commands, key = lambda cmd: cmd.time)

        for command in self.commands:
            if command.start_trigger_name is not None:
                matches = cmd_names[command.start_trigger_name]
                if len(matches) == 0:
                    raise ShException("Command %s triggered by nonexistent name %s" %
                                        (command.name, command.start_trigger_name))
                if len(matches) > 1:
                    raise ShException("Command %s triggered by ambiguous name %s" %
                                        (command.name, command.start_trigger_name))

                matches[0].add_start_dependency(command)
                cmd_names[command.name].append(command)

            if command.stop_trigger_name is not None:
                matches = cmd_names[command.stop_trigger_name]
                if len(matches) == 0:
                    raise ShException("Command %s triggered stop by nonexistent name %s" %
                                        (command.name, command.stop_trigger_name))
                if len(matches) > 1:
                    raise ShException("Command %s triggered stop by ambiguous name %s" %
                                        (command.name, command.stop_trigger_name))

                matches[0].add_stop_dependency(command)



        self.init_cmds = [ShLocalCmd(cmd, self.event) for cmd in self.cfg.get('init_cmds', [])]
        self.post_cmds = [ShLocalCmd(cmd, self.event) for cmd in self.cfg.get('post_cmds', [])]

        self.event_log = []

        self.files = []
        for cfg in self.cfg.get('files', {}).values():
            if cfg.enabled.format():
                self.files.append(ShFile(cfg, self.output_dir))

    def has_waiting_dependencies(self):
        for cmd in self.commands:
            if len(cmd.ready_start_dependencies()) > 0:
                return True
            if len(cmd.ready_stop_dependencies()) > 0:
                return True

    def waiting_dependencies(self, start=True, stop=True):
        deps = []
        for cmd in self.commands:
            if start:
                for dep in cmd.ready_start_dependencies():
                    deps.append(dep)
            if stop:
                for dep in cmd.ready_stop_dependencies():
                    deps.append(dep)
        return deps

    def handle_waiting_dependencies(self):
        for cmd in self.commands:
            for dep in cmd.ready_start_dependencies():
                dep.start(self.event_log)

            for dep in cmd.ready_stop_dependencies():
                dep.stop()

    def show_args(self):
        required_args = set()
        for entry in self.cfg.children(True):
            if entry.is_leaf():
                raw = entry.get_raw()
                if isinstance(raw, str) and '{0.args.' in raw:
                    for arg in re.findall('(?<={0.args.).+?(?=})', raw):
                        required_args.add(arg)

        log_info("Specified file requires the following command line arguments: {}"
                 .format(', '.join(list(required_args))))

    def validate(self):
        for cmd in self.commands:
            cmd.validate()

        for cmd1, cmd2 in itertools.combinations(self.commands, 2):
            cmd1.check_overlapping_logs(cmd2)

        for file in self.files:
            file.validate()

    def run_init_cmds(self):
        for cmd in self.init_cmds:
            cmd.execute()

    def run_post_cmds(self):
        for cmd in self.post_cmds:
            cmd.execute()

    def copy_files(self):
        procs = []
        for file in self.files:
            procs.extend(file.copy_to_host(self.event))
        for proc in procs:
            proc.join()

    def delete_remote_logs(self):
        remote_dirs = set()
        for cmd in self.commands:
            remote_dirs |= cmd.remote_log_directories()

        log_info("About to delete the following directories:")
        for host, remote_dir in remote_dirs:
            log_info("%s: %s" % (host.addr, remote_dir))

        if self.event.wait(5):
            log_error("Interrupted while pausing before deletion. Cancelled.")
            return

        threads = []
        for host, remote_dir in remote_dirs:
            del_cmd = 'rm -rf %s' % remote_dir
            threads.append(host.exec_cmd(del_cmd, event=self.event, background=True, checked_rtn = 0))

        for thread in threads:
            thread.join()

        if self.event.is_set():
            log_error("Error deleting remote logs!")

    def mk_remote_dirs(self):
        remote_dirs = set()
        for cmd in self.commands:
            remote_dirs |= cmd.remote_log_directories()

        threads = []
        event = threading.Event()
        for host, remote_dir in remote_dirs:
            mkdir_cmd = 'mkdir -p %s' % remote_dir
            threads.append(host.exec_cmd(mkdir_cmd, event = event, background=True, checked_rtn = 0))

        for thread in threads:
            thread.join()

        if event.is_set():
            raise Exception("Error making remote directories")

    def get_logs(self):
        log_info("Copying logs into {}".format(self.output_dir))
        threads = []
        event = threading.Event()
        for cmd in self.commands:
            threads.extend(cmd.get_logs(self.output_dir))

        for thread in threads:
            thread.join()

        if event.is_set():
            log_error("Error encountered getting logs!")

        exec_locally(['cp', self.cfg_file, self.output_dir])
        exec_locally(['cp', self.cfg_file, os.path.join(self.output_dir, 'shremote_cfg.yml')])
        for filename in set(IncludeLoader.included_files):
            exec_locally(['cp', filename, self.output_dir])

        with open(os.path.join(self.output_dir, 'event_log.json'), 'w') as f:
            json.dump(self.event_log, f, indent=2)

    def show_commands(self):
        cmds_summary = []
        for cmd in self.commands:
            cmd_summary = ['Time: {}'.format(cmd.begin)]
            if cmd.max_duration is not None:
                cmd_summary.append('Duration: {}'.format(cmd.max_duration))
            if cmd.min_duration is not None:
                cmd_summary.append('Minimum Duration: {}'.format(cmd.min_duration))
            for host, start, stop in cmd.cmd_text_iter():
                host_summary = ['Host: {}'.format(host.name)]
                wrapped = textwrap.wrap(start, break_on_hyphens=False)
                start = ' \\\n\t\t\t'.join(wrapped)
                host_summary.append('Start: {}'.format(start))
                if stop is not None:
                    host_summary.append('Stop: {}'.format(stop))
                cmd_summary.append('\n\t\t'.join(host_summary))
            cmds_summary.append('\n\t'.join(cmd_summary))
        log_info('\n' + '\n'.join(cmds_summary))

    def any_commands_unfinished(self):
        for cmd in self.commands:
            if (not cmd.started) or (not cmd.exited()):
                return True
        return False

    def any_commands_running(self):
        for cmd in self.commands:
            if cmd.running():
                return True
        return False

    def sleep_for(self, delay):
        if delay < -.1:
            log_warn("Falling behind on execution by %.01f seconds" % -delay)
            return True
        log("sleeping for %d" % delay)
        end_time = time.time() + delay
        while time.time() < end_time:
            if self.event.wait(1):
                return False
            if not self.any_commands_unfinished():
                return False
            if self.has_waiting_dependencies():
                return False
        return True

    def wait_iter_commands(self, start_time):
        while self.any_commands_unfinished():
            while self.has_waiting_dependencies():
                for cmd in self.waiting_dependencies(start=True, stop=False):
                    yield self.CommandInstance(-1, False, cmd)
                for cmd in self.waiting_dependencies(stop=True, start=False):
                    yield self.CommandInstance(-1, True, cmd)

            next_commands = []
            for timed_cmd in self.timed_commands:
                if not timed_cmd.is_stop and not timed_cmd.cmd.started:
                    next_commands.append(timed_cmd)
                if timed_cmd.is_stop and not timed_cmd.cmd.stopped:
                    next_commands.append(timed_cmd)

                for cmd in timed_cmd.cmd.started_dependencies():
                    if cmd.max_duration is not None and not cmd.stopped:
                        end_time = (cmd.started_time - start_time) + cmd.max_duration
                        next_commands.append(self.CommandInstance(end_time, True, cmd))

            next_commands = sorted(next_commands, key=lambda cmd: cmd.time)
            if len(next_commands) == 0:
                continue

            next_command = next_commands[0]

            elapsed = time.time() - start_time
            delay = next_command.time - elapsed
            if self.sleep_for(delay):
                yield next_command
            else:
                if self.has_waiting_dependencies():
                    continue
                return

    def run_commands(self):
        if self.event.is_set():
            log_error("Not running commands! Execution already halted")
            return
        min_begin = self.timed_commands[0].cmd.begin
        start_time = time.time() - min_begin

        elapsed = 0
        last_begin = 0

        for cmd_time, is_stop, cmd in self.wait_iter_commands(start_time):
            if is_stop:
                cmd.stop()
            else:
                if not cmd.started:
                    cmd.start(self.event_log)

        # Wait for commands to be done, or error event to be set
        while self.any_commands_running():
            time.sleep(1)
            if self.event.is_set():
                log_warn("Error event detected from main thread")
                break

            for cmd in self.commands:
                if cmd.running():
                    log("Waiting on command %s to finish" % cmd.name)

        # Iterate once to stop all commands
        for cmd in self.commands:
            if cmd.running():
                cmd.stop()

        # Try multiple times to stop any commands that are still running
        stops_attempted = 0
        while self.any_commands_running():
            time.sleep(.5)
            for cmd in self.commands:
                if cmd.running():
                    cmd.stop()
            stops_attempted += 1
            if stops_attempted > 20:
                break

        # Kill any programs that couldn't be stopped
        for cmd in self.commands:
            if cmd.running():
                cmd.stop(True)

        if self.any_commands_running():
            time.sleep(1)
            for cmd in self.commands:
                if cmd.running():
                    log_warn("Command %s appears to still be running" % cmd.name)

    def stop(self):
        self.event.set()

    def run(self):
        self.mk_remote_dirs()
        self.run_init_cmds()
        self.copy_files()
        self.run_commands()
        self.get_logs()
        self.run_post_cmds()

        log_info("Test complete! Output to {}".format(self.output_dir))
        log_info("Done with test!")
        close_logfile()
        return self.event.is_set()

def parse_unknown_args(args):
    new_args = {}
    key = None
    value = None
    for arg in args:
        if arg.startswith('--'):
            key = arg.strip('--')
            new_args[key] = None
        else:
            new_args[key] = arg

    return new_args

def main():
    parser = argparse.ArgumentParser(description="Schedule remote commands over SSH")
    parser.add_argument('cfg_file', type=str, help='.yml cfg file')
    parser.add_argument('label', type=str, help='Label for resulting logs')
    parser.add_argument('--parse-test', action='store_true', help='Only test parsing of cfg')
    parser.add_argument('--get-only', action='store_true', help='Only get log files, do not run')
    parser.add_argument('--out', type=str, default='.', help="Directory to output files into")
    parser.add_argument('--delete-remote', action='store_true', help='Deletes remote log directories')
    parser.add_argument('--args', type=str, required=False,
                        help="Additional arguments which are passed to the config file (format 'k1:v1;k2:v2')")

    if '--' in sys.argv:
        argv = sys.argv[1:sys.argv.index('--')]
        other_args = parse_unknown_args(sys.argv[sys.argv.index('--')+1:])
    else:
        argv = sys.argv[1:]
        other_args = {}

    args = parser.parse_args(argv)

    if args.args is not None:
        for entry in args.args.split(';'):
            k, v = entry.split(":")
            other_args[k] = v

    shremote = ShRemote(args.cfg_file, args.label, args.out, other_args, args.parse_test)

    if args.parse_test:
        shremote.show_args()
        shremote.validate()
        shremote.show_commands()
        exit(0)

    shremote.validate()
    if args.get_only:
        shremote.get_logs()
    else:
        if args.delete_remote:
            shremote.delete_remote_logs()
            if shremote.event.is_set():
                exit(1)
        return shremote.run()


if __name__ == '__main__':
    exit(main())
