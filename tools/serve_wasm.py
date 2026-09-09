#!/usr/bin/env python3
# Copyright (c) 2026 Tomasz Sterna (smokku)
"""Serve the Emscripten build directory for local testing.

The web build drives audio from a Wasm AudioWorklet, which runs on shared memory,
which browsers only hand out to cross-origin isolated pages.  A plain
`python -m http.server` does not send the headers that takes; this one does.

Without them the emulator still starts and runs -- only the worklet thread fails to
come up, so the page is silent and raises the speaker-off icon (see saudio_setup() in
src/common/sokol_audio_worklet.c, which names the missing isolation in the console).
The same headers have to come from wherever the build is really hosted -- there is
no client-side fallback and there cannot be one.

The default port is deliberately *not* 8080.  Service worker registrations are
scoped per origin, and every other dev server on the machine also answers on
127.0.0.1:8080 -- a worker some other project registered there stays registered for
ours, intercepting fetches and probing for its own script.

The web build takes its ROM from the page query string (emu.html?file=roms/X.xex),
but roms/ lives in the repo rather than the build directory, so requests under
/roms/ are served from the repo checkout.
"""
import argparse
import functools
import http.server
import os
from pathlib import Path
import sys

REPO_ROOT = Path(__file__).resolve().parents[1]
ROMS_ROOT = str(REPO_ROOT / "roms")


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        ".wasm": "application/wasm",
        ".js": "text/javascript",
        ".mjs": "text/javascript",
    }

    def translate_path(self, path):
        # /roms/... comes from the repo, everything else from the build directory.
        # Delegate first: the base class already strips the query and fragment,
        # percent-decodes, normalises and drops every "..", so the result cannot
        # escape self.directory and the remap below cannot escape roms/ either.
        full = super().translate_path(path)
        rel = os.path.relpath(full, self.directory)
        if rel == "roms" or rel.startswith("roms" + os.sep):
            return os.path.join(ROMS_ROOT, rel[len("roms") :].lstrip(os.sep))
        return full

    def end_headers(self):
        # cross-origin isolation: without these the shared memory is refused.
        # CORP matters specifically because audioWorklet.addModule() re-fetches
        # emu.js, and under COEP that fetch needs the header too.
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Resource-Policy", "same-origin")
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--port", type=int, default=6816)
    ap.add_argument("--bind", default="127.0.0.1")
    ap.add_argument("dir", nargs="?", default="wasm")
    args = ap.parse_args()
    serve_dir = Path(args.dir)
    if not serve_dir.is_dir():
        sys.exit(f"{args.dir}: not a directory")
    if not (serve_dir / "emu.html").is_file():
        sys.exit(f"{args.dir}: no emu.html here -- build the Emscripten target first")

    handler = functools.partial(Handler, directory=serve_dir)
    with http.server.ThreadingHTTPServer((args.bind, args.port), handler) as httpd:
        print(f"Serving {serve_dir} at http://{args.bind}:{args.port}/emu.html")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
