# Benchmark jdepp-python

## Dataset

Wiki40b

## Requirements

* Python
* Conda

## Install

```
$ python -m pip install -r requirements.txt
```

## Prepare data

We use huggingface datasets to download wiki40b, ja_sentence_splitter to split text into sentences(we use `ja_sentence_splitter` for speed. not all text are correctly splitted though), jagger for POS tagging.

Download and unpack jagger model file: https://github.com/lighttransport/jagger-python/releases/tag/v0.1.0

Then,

Run `prepare_dataset.py`


## Benchmark in J.DepP

Download and extract jdepp model file.  https://github.com/lighttransport/jdepp-python/releases/tag/v0.1.0
(`2ndpoly` recommended)

Then,

```
$ python run-jdepp.py
```


EoL.
