# CoPPer - Evaluation Patches

This directory contains benchmark patches used in evaluating CoPPer.
It also contains a wrapper program for SWISH++ called `swish-batch`.
In Ubuntu Linux, install the `swish++` package; use `search++` as the `<search_binary>` parameter.

Git patches are available for the following benchmarks:

* NU-MineBench-3.0.1: [Website](http://cucis.ece.northwestern.edu/projects/DMS/MineBench.html)
* parsec-3.0-beta-20130728: [Website](http://parsec.cs.princeton.edu/)
* STREAM: [Website](http://www.cs.virginia.edu/stream/)

To apply patches, download the correct benchmark versions and run (replacing `<benchmark_dir>` and `</path/to/benchmark.patch>` appropriately):

```sh
cd <benchmark_dir>
git init
git add .
git commit -m "Initial commit"
git apply --whitespace=nowarn </path/to/benchmark.patch>
git add .
git commit -m "Add CoPPer"
```

The benchmarks integrate with this `copper-eval` project, which must be compiled/installed (along with its dependencies) before the modified benchmarks can be compiled.
