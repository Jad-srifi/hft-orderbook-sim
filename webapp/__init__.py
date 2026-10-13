from __future__ import annotations


def create_app(test_config: dict | None = None):
    """Create the Flask application. Service modules remain importable without Flask installed."""
    from flask import Flask, jsonify, render_template, send_from_directory
    from werkzeug.exceptions import RequestEntityTooLarge

    from .config import Config
    from .routes import register_routes

    root = Config.ROOT
    app = Flask(
        __name__,
        template_folder=str(root / "webapp" / "templates"),
        static_folder=str(root / "web" / "static"),
        static_url_path="/static",
    )
    app.config.from_object(Config)
    if test_config:
        app.config.update(test_config)

    app.config["RUNS_ROOT"].mkdir(parents=True, exist_ok=True)
    app.config["UPLOADS_ROOT"].mkdir(parents=True, exist_ok=True)

    @app.get("/")
    def project_overview():
        return send_from_directory(app.config["WEB_ROOT"], "index.html")

    # Preserve the original static overview's relative asset URLs.
    @app.get("/styles.css")
    def legacy_stylesheet():
        return send_from_directory(app.config["WEB_ROOT"], "styles.css")

    @app.get("/app.js")
    def legacy_script():
        return send_from_directory(app.config["WEB_ROOT"], "app.js")

    @app.get("/research")
    def research_dashboard():
        return render_template("dashboard.html", active_page="research")

    @app.get("/data")
    def data_page():
        return render_template("dataset.html", active_page="data")

    @app.get("/analysis")
    def analysis_page():
        return render_template("analysis.html", active_page="analysis")

    @app.get("/replay")
    def replay_page():
        return render_template("replay.html", active_page="replay")

    @app.get("/experiments")
    def experiments_page():
        return render_template("experiments.html", active_page="experiments")

    register_routes(app)

    @app.errorhandler(RequestEntityTooLarge)
    def too_large(_error):
        return jsonify({"error": f"Upload exceeds the configured limit of {app.config['MAX_CONTENT_LENGTH'] // (1024 * 1024)} MB."}), 413

    return app
