import paramiko
from paramiko import SSHClient, AutoAddPolicy
from .checked_process import get_name_from_cmd, monitor_wait, \
                              monitor_process, CheckedProcessException
from .logger import *
from threading import Thread
import shlex
import os
import socket

class RemoteProccessException(CheckedProcessException):
    pass

class RemoteProcess(object):

    STOPPABLE = False

    def __init__(self, client, name, cmd, error_event, log_entry, sudo_passwd = None):
        self.name = name
        self.client = None
        self.cmd = cmd
        self.exited = False
        self.joined = False
        self.error_event = error_event
        self.monitor_thread = None
        self.first_line = None
        self.log_entry = log_entry
        self.sudo_passwd = sudo_passwd

    def connect(self, ssh_cfg, addr):
        self.client = SSHClient()
        self.client.set_missing_host_key_policy(AutoAddPolicy())
        self.client.load_system_host_keys()
        try:
            self.client.connect(addr,
                           username = ssh_cfg['user'].format(),
                           port = ssh_cfg['port'].format(),
                           key_filename = os.path.expanduser(ssh_cfg['key'].format()))
        except (paramiko.ssh_exception.AuthenticationException, socket.gaierror):
            log_error("Could not connect to {user}@{addr}:{port} with key {key}".format(addr=addr, **ssh_cfg))
            raise
        except Exception as e:
            log_error("Unknown error connecting to {user}@{addr}:{port} with key {key}".format(addr=addr, **ssh_cfg))
            raise


    def wait(self, seconds):
        if monitor_wait(self.error_event, seconds) :
            log_warn("Error encountered in other thread while executing %s" % self.name)
            return True
        return False

    def start(self, sudo_passwd=None):
        if self.client is None:
            log_error("Cannot start remote process when client is not connected")
        self.stdin, self.stdout, self.stderr = self.client.exec_command(self.cmd)
        if sudo_passwd is not None:
            self.stdin.write(sudo_passwd+'\n')
            self.stdin.flush()

    def run_process_monitor(self, **kwargs):
        try:
            monitor_process(self, **kwargs)
            while not self.exited and self.poll() is None:
                time.sleep(.1)
        except:
            if self.error_event:
                self.error_event.set()
            while not self.exited and self.poll() is None:
                time.sleep(.1)
            raise

    def monitor(self, daemon = False, **kwargs):
        self.monitor_thread = Thread(target = self.run_process_monitor,
                                     kwargs=kwargs)
        self.monitor_thread.daemon = daemon
        self.monitor_thread.start()
        return self.monitor_thread

    def connect_and_run_process_monitor(self, ssh_cfg, addr, **kwargs):
        try:
            self.connect(ssh_cfg, addr)
            self.start(self.sudo_passwd)
        except:
            if self.error_event:
                self.error_event.set()
            raise
        self.run_process_monitor(**kwargs)


    def connect_and_monitor(self, ssh_cfg, addr, daemon = False, **kwargs):
        self.monitor_thread = Thread(target = self.connect_and_run_process_monitor,
                                     args=(ssh_cfg, addr),
                                     kwargs=kwargs)
        self.monitor_thread.daemon = daemon
        self.monitor_thread.start()
        return self.monitor_thread

    def join(self):
        joined = True
        self.monitor_thread.join()
        self.exited = True
        self.client.close()

    def log_output(self):
        if self.stdout.channel.recv_ready():
            data = self.stdout.channel.recv(1024).decode('utf-8')
            if self.first_line is None:
                self.first_line = data.split('\n')[0]
                # Do not print first line, which should be PID
                data = '\n'.join(data.split('\n')[1:])
            if len(data):
                log("{} - {}:{}".format(self.name, "stdout", data))
        if self.stderr.channel.recv_stderr_ready():
            log("{} - {}:{}".format(self.name, "stderr", self.stderr.channel.recv(1024).decode('utf-8')))

    def poll(self):
        exited = self.stdout.channel.exit_status_ready()
        self.log_output()
        if not exited:
            return None
        self.exited = True
        if self.log_entry is not None:
            self.log_entry['stop_time_'] = float(time.time())
        rtn = self.stdout.channel.recv_exit_status()
        return rtn


def insert_exec(cmd, do_sudo):
    parts = shlex.split(cmd, posix=False)
    split_cmd = []
    inserted = False
    n_sudos = 0
    in_sudo = False
    for i, part in enumerate(parts[::-1], 1):
        if do_sudo:
            if part == 'sudo':
                n_sudos += 1
                part = 'sudo -S'
        if not inserted and part in ('&&', '||', ';'):
            split_cmd.insert(0, 'exec')
            inserted=True
        split_cmd.insert(0, part)

    if not inserted:
        split_cmd.insert(0, 'exec')

    if do_sudo and n_sudos != 1:
        log_warn("There is not exactly one 'sudo' in command %s" % cmd)
    return 'echo $$ && ' + ' '.join(split_cmd)

def start_remote_process(cmd, ssh_cfg, addr, error_event, log_entry, name, do_sudo, sudo_passwd, **kwargs):
    client = SSHClient()
    client.set_missing_host_key_policy(AutoAddPolicy())
    client.load_system_host_keys()

    if do_sudo and sudo_passwd is None:
        log_warn("Sudo option specified on host %s, which has no sudo password. Ignoring" % addr)
        do_sudo = False
    if not do_sudo:
        sudo_passwd = None

    with_exec = insert_exec(cmd, do_sudo)

    log(addr, ":", with_exec)

    if name is None:
        name = get_name_from_cmd(cmd)

    remote_proc = RemoteProcess(client, name, with_exec, error_event, log_entry, sudo_passwd)

    remote_proc.connect_and_monitor(ssh_cfg, addr, **kwargs)

    return remote_proc
