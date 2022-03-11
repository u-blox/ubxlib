import os
import logging
import sys
import coloredlogs
from logging import StreamHandler

# The logging format used when writing to file
FILE_FMT='%(asctime)s %(name)-12s %(levelname)-7s - %(message)s'

# The logging format used when writing to console
CONSOLE_FMT='%(asctime)s %(name)-12s - %(message)s'


class ULog():
    class UNameFilter(logging.Filter):
        """Logging filter that replaces record name with filename"""
        def filter(self, record):
            record.name = record.filename
            return True

    class UConsoleHandler(StreamHandler):
        """Console logging handler that prints to stdout via file descriptor"""
        def __init__(self, stdout):
            StreamHandler.__init__(self)
            self.stdout = stdout

        def emit(self, record):
            msg = self.format(record)
            stdout_fd = self.stdout.fileno()
            os.write(stdout_fd, str.encode(msg + "\n"))

    class ULogWrapper:
        """Logging wrapper"""
        def __init__(self, level):
            self._level = level
            self._buffer = ""

        def write(self, string):
            self._buffer += string
            self.flush()

        def flush(self):
            if len(self._buffer) > 0:
                last_is_eol = self._buffer[-1] == '\n'
                lines = self._buffer.splitlines()
                if last_is_eol:
                    self._buffer = ""
                else:
                    self._buffer = lines[-1]
                    lines = lines[:-1]

                for line in lines:
                    self._level(line.rstrip())


    _logging_is_setup = False

    @staticmethod
    def setup_logging(debug_file=None, redirect_stdio=True):
        """Setup the logging
        This will configure the standard Python logging system
        in the way we want.
        If debug_file is set all log events (including DEBUG level)
        will be written to this file.
        If redirect_stdio is set to True stdout will be redirected
        to a logging module called "stdout" and stderr to a module
        called "stderr". Even when redirected stdout and stderr
        messages will still be printed to the console"""
        if ULog._logging_is_setup:
            raise Exception("Logging is already setup")

        # Get the root logger
        logger = logging.getLogger()
        logger.setLevel(logging.DEBUG)

        # Setup colored logs
        coloredlogs.install()

        # Setup the console handler
        console_handle = ULog.UConsoleHandler(sys.stdout)
        console_handle.setLevel(logging.INFO)
        console_handle.setFormatter(coloredlogs.ColoredFormatter(fmt=CONSOLE_FMT))
        # Replace the default console handler with our own
        # Ours prevents loops when stdio redirect are used
        logger.handlers = [console_handle]
        logger.addFilter(ULog.UNameFilter())

        if debug_file:
            # Setup a file handle that writes all messages to file
            debug_file_handler = logging.FileHandler(debug_file, mode='w')
            debug_file_handler.setLevel(logging.DEBUG)
            debug_file_handler.setFormatter(logging.Formatter(fmt=FILE_FMT))
            logger.addHandler(debug_file_handler)

        if redirect_stdio:
            # When redirect_stdio is True we will log stdout messages as info and stderr as error
            stdout_logger = logging.getLogger("stdout")
            stderr_logger = logging.getLogger("stderr")
            sys.stdout = ULog.ULogWrapper(stdout_logger.info)
            sys.stderr = ULog.ULogWrapper(stderr_logger.error)

        ULog._logging_is_setup = True

    @staticmethod
    def get_logger(name=None) -> logging.Logger:
        """Return a new logger with a name
        If no name are specified you will get the root logger"""
        if not ULog._logging_is_setup:
            ULog.setup_logging()
        return logging.getLogger(name)
