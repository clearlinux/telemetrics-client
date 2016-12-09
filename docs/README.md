
## Notes on manual pages

The manual pages are generated using `ronn(1)`. To recreate them,
run `make manpages` in the toplevel  folder. If you want to edit
the documentation for upstream changes, make sure to edit the `*.md`
files and not the shipped nroff output files.


## Generating HTML documentation

At toplevel, run 'doxygen docs/Doxyfile'. The documentation is
generated in the docs/html/ directory.
