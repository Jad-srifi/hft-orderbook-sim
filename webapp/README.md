# Flask backend

Run from repository root with `python -m webapp.app`. See `../docs/STATIC_DASHBOARD.md` for installation, API surface, data formats, configuration limits and the local security boundary.

Application factory: `create_app()` in `webapp/__init__.py`.
Route registration: `webapp/routes.py`.
Validation and run storage: `webapp/services/file_service.py` and `run_store.py`.
Analysis/replay serialization: `analysis_service.py` and `replay_service.py`.
Existing Matplotlib discovery and safe artifact listing: `plot_service.py`.

Large ITCH inputs should be selected via the project-local inputs list (files beneath `data/` or `results/`), not uploaded. The browser upload endpoint has a configurable default cap of 512 MB; local-file runs read the source in place and keep run outputs separate. See [the dashboard guide](../docs/STATIC_DASHBOARD.md) for supported inputs, launch instructions, routes, and security boundaries.


### If the legacy research workflow imports `pytest`

`pytest` is a development/test dependency and is not required to launch or use the dashboard. If `examples/itch_research_workflow.py` (or a module it imports) raises `ModuleNotFoundError: pytest`, the dashboard records a warning in `workflow.log` and falls back to the Chapter 12 `examples/_common.py` replay adapter. The replay and dashboard-generated observation plots can still be produced, but the original workflow's complete plot set will be absent until its reusable implementation is separated from `tests/python/test_itch_research_workflow.py`. The dashboard surfaces this fallback warning rather than silently claiming the legacy workflow ran.
