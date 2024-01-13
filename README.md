# jdepp-python

Python binding for J.DepP(C++ implementation of Japanese Dependency Parsers)

## Status

W.I.P.

## Build configuration

* MeCab style POS format: `FEATURE_SEP ','`

## Precompiled model files

You can get precompiled model files(MeCab POS tagging + train with KNBC copus) from

https://github.com/lighttransport/jdepp-python/releases/tag/v0.1.0

Model file is licensed under 3-clause BSD license.

## Example

Download precompiled model file.

```
$ wget https://github.com/lighttransport/jdepp-python/releases/download/v0.1.0/knbc-mecab-jumandic-2ndpoly.tar.gz
$ tar xvf knbc-mecab-jumandic-2ndpoly.tar.gz
```

```py
import jdepp

T.B.W.
```

## Build standalone C++ app + training a model

If you just want to use J.DepP from cli(e.g. batch processing),
you can build a standalone C++ app using CMake.

We modified J.DepP source code to improve portablily(e.g. Ours works well on Windows)  

Training a model from Python binding is also not yet supported.
For a while, you can train a model by using standalone C++ jdepp app.


### Releasing

* tag it: `git tag vX.Y.Z`
* push tag: `git push --tags`

Versioning is automatically done through `setuptools_scm`

## TODO

- [ ] WASM build
- [ ] Training API support
      
## License

jdepp-python is licensed under 2-Clause BSD license.
J.DepP is licensed under GPLv2/LGPLv2.1/BSD triple license.

## Thrird party license

* pacco, cedar, opal(subcompoennts of J.DepP): GPLv2/LGPLv2.1/BSD triple license. We choose BSD license.
* io-util: MIT license.
* optparse: Unlicense https://github.com/skeeto/optparse
