from .jdepp_ext import *

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


class Jdepp:
    def __init__(self):

        self._jdepp = JdeppExt()

    def load_model(self, dict_path: Path):
        return self._jdepp.load_model(str(dict_path))

    def parse_from_postagged(self, input_postagged: str):
        return self._jdepp.parse_from_postagged(input_postagged)

    # for internal debugging.
    def _run(self):
        self._jdepp.run()


