from jdepp_ext import *

import os
import sys
from pathlib import Path

class Jdepp:
    def __init__(self):

        self._jdepp = JdeppExt()

    def load_model(self, dict_path: Path):
        self._jdepp.load_model(str(dict_path))


