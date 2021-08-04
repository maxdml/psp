from __future__ import print_function
import time

COLORS = dict(
    end='\033[0m',
    warning = '\033[93m',
    error = '\033[31m',
    info = '\033[0;32m'
)

start_time = time.time()

LOGFILE = None
TEST_MODE = False

def set_logfile(filename):
    global LOGFILE
    LOGFILE = open(filename, 'w')

def set_test_mode():
    global TEST_MODE
    TEST_MODE = True

def close_logfile():
    LOGFILE.close()

def log_to_file(s, **print_kwargs):
    if (LOGFILE is not None):
        LOGFILE.write(s + '\n')
    print(s, **print_kwargs)

def log(*args, **kwargs):
    s = "debug {:.1f}: ".format(time.time() - start_time)
    log_to_file(s + " ".join([str(x) for x in args]), **kwargs)

def log_info(*args, **kwargs):
    s = COLORS['info'] + 'info {:.1f}: '.format(time.time() - start_time)
    log_to_file(s + " ".join([str(x) for x in args]) + COLORS['end'], **kwargs)

def log_warn(*args, **kwargs):
    s = COLORS["warning"] + "warning {:.1f}: ".format(time.time() - start_time) + \
            ' '.join([str(x) for x in args]) + COLORS['end']
    log_to_file(s, **kwargs)

def log_error(*args, **kwargs):
    if TEST_MODE:
        return log_info(*args, **kwargs)
    s = COLORS['error'] + "\n________________________________________________\n"
    s += "error: " + " ".join([str(x) for x in args])
    s += "\n________________________________________________" + COLORS['end']
    log_to_file(s, **kwargs)

