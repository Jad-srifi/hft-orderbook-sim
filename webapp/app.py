from __future__ import annotations

import os

from . import create_app

app = create_app()


if __name__ == "__main__":
    host = os.environ.get("DASHBOARD_HOST", "127.0.0.1")
    port = int(os.environ.get("PORT", "8000"))
    # Keep the app local-only and disable the interactive debugger.
    app.run(host=host, port=port, debug=False, threaded=True)
