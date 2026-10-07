# Retired platform material

This folder preserves earlier teaching requirements, scenario plans, platform documentation,
and release evidence. It is not the current New Employee contract. Current customer decisions
are in `platform/requirements/`; current code and B8 interface headers remain in `platform/`.

The archived Python check is available to maintainers from the repository root:

```sh
python internal/history/platform/check_contracts.py --root .
```

It checks consistency of the retired requirement snapshot and selected current source inventories.
The active platform CTest no longer runs it as a customer acceptance check.
