#!/usr/bin/env python3
"""Regression contract for Finder and single-instance file opening."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN_SOURCE = ROOT / "sources" / "main.cpp"


def main() -> int:
    source = MAIN_SOURCE.read_text(encoding="utf-8")
    start = source.index("int launchApplication(")
    end = source.index("\n}\n#endif", start)
    launch_application = source[start:end]

    assert "app->setActivationWindow(&w, true);" in launch_application
    assert "&QtSingleApplication::messageReceived" in launch_application
    assert "&MainWindow::openFiles" in launch_application
    assert "SIGNAL(openFile" not in launch_application
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
