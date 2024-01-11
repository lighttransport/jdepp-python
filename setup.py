import sys

from setuptools import setup
from pybind11.setup_helpers import Pybind11Extension

# Should be False in the release
dev_mode = False

jdepp_compile_args=[
  ]

if sys.platform.startswith('win32'):
  # Assume MSVC
  pass
else:
  jdepp_compile_args.append("-std=c++11")


if dev_mode:
  jdepp_compile_args.append('-O0')
  jdepp_compile_args.append('-g')
  jdepp_compile_args.append('-fsanitize=address')

ext_modules = [
    Pybind11Extension("jdepp_ext", ["jdepp/io-util.cc", "jdepp/python-binding-jdepp.cc", "jdepp/classify.cc", "jdepp/kernel.cc", "jdepp/linear.cc", "jdepp/pdep.cc"],
      include_dirs=['.'],
      extra_compile_args=jdepp_compile_args,
    ),
]

setup(
    name="jdepp",
    packages=['jdepp'],
    url="https://github.com/lighttransport/jdepp-python/",
    ext_modules=ext_modules,
    long_description=open("./README.md", 'r', encoding='utf8').read(),
    long_description_content_type='text/markdown',
    # NOTE: entry_points are set in pyproject.toml
    #entry_points={
    #    'console_scripts': [
    #        "jdepp=jdepp.main:main"
    #    ]
    #},
    license_files= ('LICENSE', 'jdepp.BSD', 'jdepp.GPL', 'jdepp.LGPL'),
    install_requires=[])
