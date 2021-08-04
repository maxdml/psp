import time
from .logger import *


class CheckedProcessException(Exception):
    pass

def get_name_from_cmd(cmd):
    if isinstance(cmd, str):
        return cmd.split()[0]
    return cmd[0]

def monitor_wait(event, seconds):
    if event is not None:
        return event.wait(seconds)
    else:
        time.sleep(seconds)
        return False

CHECK_CALL_INTERVAL= .2

def monitor_process(process,
                    min_duration = None, max_duration = None,
                    checked_rtn = None, log_end=False,
                    check_interval = CHECK_CALL_INTERVAL):
    if checked_rtn is False:
        checked_rtn = None

    start_time = time.time()
    rtn_code = process.poll()
    duration = 0
    while not process.exited:
        duration = time.time() - start_time

        if max_duration is not None and duration > max_duration:
            log_error("Command \"", process.name, "\" executed for longer than ",
                      max_duration, " seconds")
            if process.STOPPABLE:
                process.force_stop()
            raise CheckedProcessException(process.name)

        if max_duration is not None:
            sleep_duration = min(max_duration - duration, check_interval)
            sleep_duration = max(sleep_duration, .05)
        else:
            sleep_duration = check_interval

        if process.wait(sleep_duration):
            log_warn("Stopping process monitor")
            return

        rtn_code = process.poll()

    if checked_rtn is not None and rtn_code != checked_rtn:
        log_error("Command:\n\t%s\nReturned: %d\nExpected: %d\n" %
                      (process.name, rtn_code, checked_rtn),
                      "If this is not an error, add `checked_rtn: null` to command")
        raise CheckedProcessException(process.name)

    if min_duration is not None and duration < min_duration:
        log_error("Command \"", process.name, "\" executed for ", duration,
                  "seconds; expected: ", min_duration)
        raise CheckedProcessException(process.name)

    if log_end:
        log("Command {} executed for {:.2f} seconds".format(process.name, duration))
