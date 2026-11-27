"""Entrypoint for `python -m stem_engine`."""

from __future__ import annotations

import sys

from stem_engine.cli import main

if __name__ == "__main__":
    sys.exit(main())
