# Local Dashboard: Static Overview and Flask Research Interface

This repository has **two related dashboard layers**:

- **Static overview** (`web/`): a dependency-light project overview, roadmap, examples, and command reference.
- **Flask research dashboard** (`webapp/`): the local research interface for data uploads, analysis runs, saved plots/artifacts, and bounded order-book replay.

The launcher starts the Flask application, not just a static file server.

## Run the full dashboard

Use the repository's configured Python environment (the project is developed with MSYS2 UCRT64 on Windows). From the repository root:

```bash
python -m pip install -r webapp/requirements.txt
bash scripts/run_dashboard.sh
```

The launcher checks that the static frontend and Flask app files exist and that Flask is installed. It then starts `python -m webapp.app`.

Open:

- **Research dashboard:** [http://127.0.0.1:8000/research](http://127.0.0.1:8000/research)
- **Static overview and roadmap:** [http://127.0.0.1:8000/](http://127.0.0.1:8000/)

Stop the server with `Ctrl+C`.

Choose a different port if needed:

```bash
PORT=8080 bash scripts/run_dashboard.sh
```

The default bind address is `127.0.0.1`. To override it, set `DASHBOARD_HOST`; do not expose the app to an untrusted network without adding appropriate security controls.

## Pages and API

The Flask application serves these pages:

| Path | Purpose |
|---|---|
| `/` | Static project overview and roadmap |
| `/research` | Create and inspect local research runs |
| `/data` | View/manage supported local input data |
| `/analysis` | Browse generated plots and diagnostics |
| `/replay` | Replay saved, bounded book snapshots in the browser |
| `/experiments` | Browse saved runs/experiment records |

The backend exposes JSON endpoints under `/api/`, including health, run creation/listing, run status, plots, artifacts, snapshots, and heatmap data. The exact route behavior is implemented in `webapp/routes.py`.

## What is static and what is dynamic?

The static overview in `web/` includes roadmap checkboxes saved in browser local storage, a theme toggle, and command-copy controls. Its project status text is manually maintained; it is not live build or test telemetry.

The Flask research pages call the backend to create and inspect runs. Run metadata, uploads, plots, and other generated artifacts are stored locally under `results/dashboard/` according to `webapp/config.py`. The replay page uses a bounded server-generated snapshot sequence; it does not parse the raw ITCH feed in the browser.

## Static-only alternative

If you only want to view the project overview without Flask, run:

```bash
python -m http.server 8000 --bind 127.0.0.1 --directory web
```

This serves the static overview only. It does **not** provide the Flask research pages, API endpoints, file uploads, analysis jobs, persisted runs, or browser replay data.

## Input and operational limitations

- Historical ITCH replay currently requires the **decompressed binary `.txt` file**. The runner rejects `.gz` input; decompress the feed before replay, even if a file picker accepts the suffix.
- Upload size, replay message count, snapshot count, levels per side, and heatmap limits are configurable in `webapp/config.py`.
- The dashboard is intended for local research, not as a public production service. Keep it bound to loopback for normal local use.
- Aggregate CSV observations do not contain per-price-level liquidity unless the schema explicitly supplies those levels. The UI must not infer a price ladder or heatmap from aggregate-only data.

## Relevant implementation

- `scripts/run_dashboard.sh`: environment checks and launcher
- `web/`: static overview assets
- `webapp/__init__.py`: Flask application and page routes
- `webapp/routes.py`: API endpoints and request validation
- `webapp/services/analysis_service.py`: analysis orchestration
- `webapp/services/itch_runner.py`: bounded historical ITCH replay
- `webapp/services/run_store.py`: local run metadata persistence
