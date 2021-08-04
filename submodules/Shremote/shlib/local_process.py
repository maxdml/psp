import time
import subprocess
from .logger import *
from .checked_process import get_name_from_cmd, monitor_wait, \
                             monitor_process, CheckedProcessException
try:
    from queue import Queue, Empty
except:
    from Queue import Queue, Empty
from threading import Thread

class LocalProcessException(CheckedProcessException):
    pass

class LocalProcess(object):

    STOPPABLE = True

    #https://stackoverflow.com/questions/375427/non-blocking-read-on-a-subprocess-pipe-in-python
    @staticmethod
    def monitor_stream(stream, queue):
        for line in iter(stream.readline, b''):
            queue.put(line.decode('utf-8'))

    def log_from_queue(self, queue, label):
        while True:
            try:
                line = queue.get_nowait()
                log("{} - {}:{}".format(self.name, label, line), end='')
            except Empty:
                break

    def __init__(self, args, name='', shell=False, error_event=False):
        self.args = args
        self.name = name
        self.shell = shell
        self.exited = False
        self.rtn_code = None
        self.error_event = error_event

    def start(self):
        self.proc = subprocess.Popen(self.args, stdout = subprocess.PIPE, stderr=subprocess.PIPE, 
                                     shell=self.shell, close_fds=True)

    def wait(self, seconds):
        if monitor_wait(self.error_event, seconds) :
            log_warn("Error encountered in other thread while executing %s" % self.name)
            return True
        return False

    def log_output(self):
        self.log_from_queue(self.stderr_q, "stderr")
        self.log_from_queue(self.stdout_q, "stdout")

    def run_process_monitor(self, **kwargs):
        try:
            monitor_process(self, **kwargs)
        except:
            if self.error_event:
                self.error_event.set()
            raise

    def monitor(self, **kwargs):
        self.stderr_q = Queue()
        self.stdout_q = Queue()
        self.stderr_thread = Thread(target = self.monitor_stream,
                                    args = (self.proc.stdout, self.stdout_q))
        self.stderr_thread.start()
        self.stdout_thread = Thread(target = self.monitor_stream,
                                    args = (self.proc.stderr, self.stderr_q))
        self.stdout_thread.start()

        self.monitor_thread = Thread(target = self.run_process_monitor,
                                     kwargs=kwargs)
        self.monitor_thread.start()

    def join(self):
        self.monitor_thread.join()

    def poll(self, valid_rtn = None):
        rtn_code = self.proc.poll()
        self.log_output()
        if rtn_code is not None:
            self.rtn_code = rtn_code
            self.exited = True
            self.stderr_thread.join()
            self.stdout_thread.join()

            if valid_rtn is not None:
                if rtn_code != valid_rtn:
                    log_error("Command:\n\t%s\nReturned: %d\nExpected: %d\n" %
                              (self.args, rtn_code, valid_rtn),
                              "If this is not an error, add `checked_rtn: None` to command")
                    raise CheckedProcessException(self.args)

        return rtn_code

    def force_stop(self):
        try:
            self.proc.terminate()
        except OSError:
            pass
        time.sleep(.05)
        if self.poll() is None:
            log_warn("Forced to kill prcess")
            self.proc.kill()
            time.sleep(.25)
            if self.poll() is None:
                log_error("Could not terminate process!")
                return

def start_local_process(cmd, error_event, shell = False, **kwargs):
    if not shell and isinstance(cmd, str):
        cmd = shlex.shlex(cmd)

    name = get_name_from_cmd(cmd)

    proc = LocalProcess(cmd, name, shell, error_event)
    proc.start()
    proc.monitor(**kwargs)

    return proc
    
def exec_locally(cmd, checked_rtn = 0, **kwargs):
    p = start_local_process(cmd, error_event = None, checked_rtn = checked_rtn, **kwargs)
    p.join()

    if checked_rtn is not None and p.rtn_code != checked_rtn:
        raise ShException("Cmd {} returned non-0 code {}".format(cmd, p.rtn_code))


