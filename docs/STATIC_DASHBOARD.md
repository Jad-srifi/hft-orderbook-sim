# Hybrid LOB Simulator — Local Dashboard

A static, dependency-free frontend for navigating the project roadmap, examples and common terminal commands.

## Add to the repository

Copy the `web/` directory and `scripts/run_dashboard.sh` into the root of `hybrid-lob-simulator/`. Merge the new script into your existing `scripts/` directory; do not replace your other scripts.

Expected placement:

```text
hybrid-lob-simulator/
├── examples/
├── scripts/
│   └── run_dashboard.sh
└── web/
    ├── index.html
    ├── styles.css
    └── app.js
```

## Run locally (MSYS2 UCRT64)

From the repository root:

```bash
chmod +x scripts/run_dashboard.sh
./scripts/run_dashboard.sh
```

Open `http://127.0.0.1:8000` in your browser and stop the server with `Ctrl+C`.

Use another port if needed:

```bash
PORT=8080 ./scripts/run_dashboard.sh
```

Alternative if you do not want to use the launcher script:

```bash
python -m http.server 8000 --bind 127.0.0.1 --directory web
```

## What it does

- Responsive overview, target data-flow diagram, project roadmap, examples list and command references.
- Copy buttons for commands and local URL.
- Roadmap checkboxes saved in browser local storage.
- Light/dark theme toggle.
- No Node.js, framework, external CDN, database or cloud service required.

## Important limitations

This first version is a static frontend. It does not inspect repository files, run builds/tests from the browser, start research jobs, or read generated result files. The displayed project status is a manually maintained snapshot, not live telemetry. The command buttons copy terminal commands for you to run yourself. A backend API can be added later if you want job launching, live logs, diagnostics and plots from the interface.
