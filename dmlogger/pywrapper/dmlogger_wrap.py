from ctypes import *
from enum import IntEnum

# Load libdmlogger shared object:
dmlogger = CDLL("./libdmlogger.so")

# Void pointer referenc to dmlogger into python:
dmlogger_pt = c_void_p

# ==== C functions prototypes into C ==== #
# Init / Run / Deinit:
dmlogger.dmlogger_init.argtypes = [POINTER(dmlogger_pt)]

dmlogger.dmlogger_run.argtypes = [dmlogger_pt]
dmlogger.dmlogger_run.restype = c_bool

dmlogger.dmlogger_deinit.argtypes = [POINTER(dmlogger_pt)]

# Output configurations: STDOUT / STDERR / FILE / CUSTOM:
dmlogger.dmlogger_conf_output_stdout.argtypes = [dmlogger_pt]
dmlogger.dmlogger_conf_output_stdout.restype = c_bool

dmlogger.dmlogger_conf_output_stderr.argtypes = [dmlogger_pt]
dmlogger.dmlogger_conf_output_stderr.restype = c_bool

dmlogger.dmlogger_conf_output_file.argtypes = [
    dmlogger_pt, c_char_p, c_char_p, c_bool, c_bool, c_size_t
    ]
dmlogger.dmlogger_conf_output_file.restype = c_bool

CUSTOM_CALLBACK = CFUNCTYPE(None, c_char_p, c_void_p)
dmlogger.dmlogger_conf_output_custom.argtypes = [
    dmlogger_pt, CUSTOM_CALLBACK, c_void_p
    ]
dmlogger.dmlogger_conf_output_custom.restype = c_bool

# Queue configurations: Of policy / capacity:
dmlogger.dmlogger_conf_queue_ofpolicy.argtypes = [
    dmlogger_pt, c_int, c_uint
]
dmlogger.dmlogger_conf_queue_ofpolicy.restype = c_bool

dmlogger.dmlogger_conf_queue_capacity.argtypes = [
    dmlogger_pt, c_size_t
]
dmlogger.dmlogger_conf_queue_capacity.restype = c_bool

# Minimum level configuration:
dmlogger.dmlogger_conf_logger_minlvl.argtypes = [dmlogger_pt, c_int]
dmlogger.dmlogger_conf_logger_minlvl.restype = c_bool

# Log / Flush:
dmlogger.dmlogger_log.argtypes = [dmlogger_pt, c_int, c_char_p]

dmlogger.dmlogger_flush.argtypes = [dmlogger_pt]
dmlogger.dmlogger_flush.restype = c_bool



# ==== DMLogger class in Python ==== #
class Dmlogger:
    # Logger level constants class:
    class Level(IntEnum):
        DEBUG = 1
        INFO = 2
        NOTIFY = 3
        WARNING = 4
        ERROR = 5
        FATAL = 6

    # Logger queue overflow policy constants class:
    class OverflowPolicy(IntEnum):
        DROP = 0
        OVERWRITE = 1
        WAIT = 2
        WAIT_TIMEOUT = 3

    # Logger output select constants class:
    class OutputSelect(IntEnum):
        FILE = 0
        STDOUT = 1
        STDERR = 2
        CUSTOM = 3

    def __init__(self) -> None:
        self._logger = dmlogger_pt()
        dmlogger.dmlogger_init(byref(self._logger))
        self._isRunning = dmlogger.dmlogger_run(self._logger)

    def __del__(self) -> None:
        dmlogger.dmlogger_deinit(byref(self._logger))

    def configureOutput(self, outputSel: OutputSelect, **kwargs) -> bool:
        if outputSel == self.OutputSelect.STDOUT:
            return dmlogger.dmlogger_conf_output_stdout(self._logger)

        elif outputSel == self.OutputSelect.STDERR:
            return dmlogger.dmlogger_conf_output_stderr(self._logger)

        elif outputSel == self.OutputSelect.FILE:
            # Keywords arguments:
            try:
                path = kwargs['path'].encode()
                name = kwargs['name'].encode()
                rotf_bydate = kwargs.get('rotf_bydate', False)
                rotf_bysize = kwargs.get('rotf_bysize', False)
                maxf_size = kwargs.get('maxf_size', 10 * 1024 * 1024)
            except KeyError as e:
                raise ValueError(f"Missing file output parameter: {e}")

            return dmlogger.dmlogger_conf_output_file(self._logger, path, name, rotf_bydate, rotf_bysize, maxf_size)

        elif outputSel == self.OutputSelect.CUSTOM:
            # Keywords arguments:
            try:
                callback = kwargs['callback']
                udata = kwargs.get('udata', None)
            except KeyError as e:
                raise ValueError(f"Missing custom output parameter: {e}")

            # Callback check:
            if not isinstance(callback, CUSTOM_CALLBACK):
                raise TypeError("callback must be of type CUSTOM_CALLBACK")

            return dmlogger.dmlogger_conf_output_custom(self._logger, callback, udata)

        else:
            return False

    def configureQueue(self, capacity: int, overflowPolicy: OverflowPolicy, timeout: int) -> bool:
        ret_cap = dmlogger.dmlogger_conf_queue_capacity(self._logger, capacity)
        ret_ofp = dmlogger.dmlogger_conf_queue_ofpolicy(self._logger, overflowPolicy, timeout)
        return ret_cap and ret_ofp

    def configureMinlvl(self, lvl: Level):
        return dmlogger.dmlogger_conf_logger_minlvl(self._logger, lvl)

    def log(self, lvl: int, msg: str) -> None:
        dmlogger.dmlogger_log(self._logger, lvl, msg.encode())

    def flush(self) -> bool:
        return dmlogger.dmlogger_flush(self._logger)


if __name__ == "__main__":
    logger = Dmlogger()
    logger.configureOutput(logger.OutputSelect.STDOUT)
    logger.configureQueue(200, logger.OverflowPolicy.DROP, 0)
    logger.configureMinlvl(logger.Level.DEBUG)
    usuario = input("Nombre de usuario: ")
    logger.log(logger.Level.INFO, f"({usuario})Todo correcto!")