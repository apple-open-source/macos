# short and simple

import PyRSS2Gen

from distutils.core import setup

setup(name = "PyRSS2Gen",
      version = ".".join(map(str, PyRSS2Gen.__version__)),
      py_modules = ["PyRSS2Gen"])
