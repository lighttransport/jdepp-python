from jdepp_ext import *

# load setptools_scm generated _version.py
try:
    from ._version import version, __version__
    from ._version import version_tuple
except:
    __version__ = version = '0.0.0.dev'
    __version_tuple__ = version_tuple = (0, 0, 0, 'dev', 'git')

import os
import sys
from pathlib import Path

from .jdepp_tools import *

__all__ = ['to_tree', 'to_dot']


class Jdepp:
    def __init__(self):

        self._jdepp = JdeppExt()

    def load_model(self, dict_path: Path):
        return self._jdepp.load_model(str(dict_path))

    def parse_from_postagged(self, input_postagged: str):
        if isinstance(input_postagged, list):
            # Assume each line ends with newline '\n'
            input_postagged = "".join(input_postagged)

        return self._jdepp.parse_from_postagged(input_postagged)

    def parse_from_postagged_batch(self, input_postaggeds: str):
        if not isinstance(input_postaggeds, list):
            sys.stderr.write("Input must be List[str]") 
            return None

        return self._jdepp.parse_from_postagged_batch(input_postaggeds)

    # for internal debugging.
    def _run(self):
        self._jdepp.run()


