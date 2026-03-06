from ctypes import *
from enum import IntEnum
import os

# Load libdmlogger shared object:
so_path = os.path.realpath("./libdmlogger.so.link")
dmlogger = CDLL(so_path)

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
class DMLogger:
    """
    DMLogger
    --------
    This is a simple wrapper for my implementation of dmlogger. A logger that operates in a non-blocking way,
    and as efficient as I could make.

    All the implementation is made in plain C, completly open-source.
    """
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
        """ Initialization of the class. """
        # Create the reference and initialize instance:
        self._logger = dmlogger_pt()
        dmlogger.dmlogger_init(byref(self._logger))

        # Execute the consumer thread and set the running state of DMLogger:
        self._isRunning = dmlogger.dmlogger_run(self._logger)

    def __del__(self) -> None:
        """ Deletion of the class. """
        dmlogger.dmlogger_deinit(byref(self._logger))

    def configureOutput(self, outputSel: OutputSelect, **kwargs) -> bool:
        """ Function to select and configure an output for the logger (stdout by default). 
        
        Args:
            -outputSel(OutputSelect): An item of the internal class OutputSelect used to select the output.
            -**kwargs: Keyword arguments specific to the selected output, which are:
                For self.OutputSelect.FILE -> path: str (Path to the directory of the file)
                                           -> name: str (Base name of the log file to use, created if non-existing)
                                           -> rotf_bydate: bool (Rotate the file by change of the date)
                                           -> rotf_bysize: bool (Rotate the file by the size of the log)
                For self.OutputSelect.CUSTOM -> callback: callable (Function reference to a custom output function)
                                             -> udata: any (Any user data to be used internally)                         
        
        Return: 
            - bool: True if configuration succeeded or false otherwise. """
        # In case outputSel is not valid, ignores the call:
        if not isinstance(outputSel, self.OutputSelect):
            return False

        # Standard output stream selected:
        if outputSel == self.OutputSelect.STDOUT:
            return dmlogger.dmlogger_conf_output_stdout(self._logger)

        # Standard error stream selected:
        elif outputSel == self.OutputSelect.STDERR:
            return dmlogger.dmlogger_conf_output_stderr(self._logger)

        # File output selected:
        elif outputSel == self.OutputSelect.FILE:
            try:
                # Keywords arguments:
                path = kwargs['path'].encode()
                name = kwargs['name'].encode()
                rotf_bydate = kwargs.get('rotf_bydate', False)
                rotf_bysize = kwargs.get('rotf_bysize', False)
                maxf_size = kwargs.get('maxf_size', 0)
            except KeyError as e:
                raise ValueError(f"Missing file output parameter: {e}")

            return dmlogger.dmlogger_conf_output_file(self._logger, path, name, rotf_bydate, rotf_bysize, maxf_size)

        # Custom output selected:
        elif outputSel == self.OutputSelect.CUSTOM:
            try:
                # Keywords arguments:
                callback = kwargs['callback']
                udata = kwargs.get('udata', None)
            except KeyError as e:
                raise ValueError(f"Missing custom output parameter: {e}")

            # Callback check:
            if not isinstance(callback, CUSTOM_CALLBACK):
                raise TypeError("callback must be of type CUSTOM_CALLBACK")

            return dmlogger.dmlogger_conf_output_custom(self._logger, callback, udata)

        # Unknown option selected:
        else:
            return False

    def configureQueue(self, capacity: int, overflowPolicy: OverflowPolicy, timeout: int) -> bool:
        """ Function to configure the queue handled internally by the logger to write entries.
        
        Args:
            - capacity(int): Number of entries that can be stored at once.
            - overflowPolicy(OverflowPolicy): Strategy to follow in case the queue is full and a new entry
            is queued. Possibly options are:
                -> DROP: Discards the new entry if the queue is full.
                -> OVERWRITE: Overwrite the oldest entry if the queue if full.
                -> WAIT: Blocks the log function until the entry has space to enter the queue.
                -> WAIT_TIMEOUT: Same as WAIT but after a period of timeout, it discards the entry as DROP.   
            - timeout(int): Number of seconds to wait when overflow policy is WAIT_TIMEOUT.
            
        Return:
            - bool: True if both, capacity and overflow policy configuration succeeded, false otherwise. """
        # In case overflowPolicy is invalid, ignore the call:
        if not isinstance(overflowPolicy, self.OverflowPolicy):
            return False

        # Or between both return values to detect configuration errors:
        ret_cap = dmlogger.dmlogger_conf_queue_capacity(self._logger, capacity)
        ret_ofp = dmlogger.dmlogger_conf_queue_ofpolicy(self._logger, overflowPolicy, timeout)
        return ret_cap and ret_ofp

    def configureMinlvl(self, lvl: Level) -> bool:
        """ Function to configure the minimum level allowed to be logged. Any log with lower level than 
        the configured here will be discarded.
        
        Args:
            - lvl(Level): Minimum level to be logged, possible options are:
                -> DEBUG, NOTIFY, INFO, WARNING, ERROR and FATAL 
                
        Return:
            - bool: False if configuration failed, true otherwise. """
        # In case lvl is not valid, ignore call:
        if not isinstance(lvl, self.Level):
            return False

        return dmlogger.dmlogger_conf_logger_minlvl(self._logger, lvl)

    def log(self, lvl: Level, msg: str) -> None:
        """ Function to add to the logger queue a new entry with the given level and message.
        
        Args:
            - lvl(Level): Level of the message to be logged.
            - msg(str): String with the message to be logged. """
        dmlogger.dmlogger_log(self._logger, lvl, msg.encode())

    def flush(self) -> bool:
        """ Function to flush the entire logger queue into output. ¡Blocks until is done or fails after 4 secs! """
        return dmlogger.dmlogger_flush(self._logger)


if __name__ == "__main__":
    logger = DMLogger()
    logger.configureOutput(logger.OutputSelect.STDOUT)
    logger.configureQueue(200, logger.OverflowPolicy.DROP, 0)
    logger.configureMinlvl(logger.Level.DEBUG)

    usuario = input("Nombre de usuario: ")
    logger.log(logger.Level.INFO, f"({usuario}) Todo correcto!")