(() => {
  const api = () => window.LobDashboard;
  const $ = (selector) => document.querySelector(selector);

  function ensureFeedback(form) {
    let status = $("#run-feedback");
    if (!status) {
      status = document.createElement("div");
      status.id = "run-feedback";
      status.className = "run-feedback";
      status.hidden = true;
      $("#upload-error")?.before(status);
    }
    return status;
  }

  function showStatus(node, run) {
    node.hidden = false;

    const state = run.status || "queued";
    const progress = run.progress;

    const bar =
      progress == null
        ? ""
        : `<div class="run-progress-track"><span style="width:${Math.max(
            0,
            Math.min(100, progress),
          )}%"></span></div>`;

    const workflowWarning = run.summary?.workflow_warning
      ? `<div class="form-warning"><strong>Notice.</strong> ${api().escapeHtml(
          run.summary.workflow_warning,
        )}</div>`
      : "";

    const number = (value) => Number(value || 0).toLocaleString();

    const limit = Number(run.config?.message_limit ?? 0);
    const messageLimit = limit > 0 ? ` / ${number(limit)}` : " / full file";

    const counter = (label, value, suffix = "") =>
      `<div class="run-counter"><span>${label}</span><strong>${number(
        value,
      )}${suffix}</strong></div>`;

    const counters =
      run.input_kind === "itch"
        ? `<div class="run-counter-grid" aria-live="polite">
            ${counter("RAW MESSAGES READ", run.messages_read, messageLimit)}
            ${counter("ORDER BOOK APPLIED", run.applied_messages)}
            ${counter(
              "IGNORED — DIFFERENT STOCK",
              run.ignored_different_stock_locate,
            )}
            ${counter("NON-APPLIED MESSAGES", run.non_applied_messages)}
            ${counter("FAILED APPLICATIONS", run.failed_applications)}
            ${counter("PARSER ERRORS", run.parse_errors)}
            ${counter("MAPPING ERRORS", run.mapping_errors)}
            ${counter("MESSAGES ACCOUNTED", run.messages_accounted)}
            ${counter("SNAPSHOTS CAPTURED", run.snapshots_captured)}
          </div>`
        : "";

    node.innerHTML = `
      <div class="feedback-top">
        <span class="status-pill status-${
          ["completed", "running", "queued", "failed"].includes(state)
            ? state
            : "queued"
        }">${api().escapeHtml(state)}</span>
        <strong>${api().escapeHtml(run.original_filename || run.id)}</strong>
      </div>
      <p>${api().escapeHtml(run.message || "")}${
        progress == null ? "" : ` (${progress}%)`
      }</p>
      ${bar}
      ${counters}
      ${
        run.error
          ? `<div class="form-error">${api().escapeHtml(run.error)}</div>`
          : ""
      }
      ${state === "completed" ? workflowWarning : ""}
      ${
        state === "completed"
          ? `<div class="feedback-links">
              <a href="/analysis?run_id=${encodeURIComponent(run.id)}">Open plots</a>
              <a href="/replay?run_id=${encodeURIComponent(run.id)}">Open replay</a>
              <a href="/experiments?run_id=${encodeURIComponent(run.id)}">Run metadata</a>
            </div>`
          : ""
      }
    `;
  }

  async function pollRun(runId, node, form) {
    while (true) {
      await new Promise((resolve) => setTimeout(resolve, 900));

      let response;

      try {
        response = await api().fetchJson(
          `/api/runs/${encodeURIComponent(runId)}`,
        );
      } catch (error) {
        const message = document.createElement("p");
        message.textContent = error.message || "Could not refresh run status.";
        node.appendChild(message);
        return;
      }

      const run = response.run;
      showStatus(node, run);

      if (["completed", "failed"].includes(run.status)) {
        const submit = $("#submit-run");

        if (submit) {
          submit.disabled = false;
          submit.textContent = "▶ Run analysis";
        }

        await api()
          .loadRuns()
          .catch(() => {});

        return;
      }
    }
  }

  function initForm(form) {
    const fileInput = form.querySelector("#source-file");
    const drop = form.querySelector("#drop-zone");
    const selected = form.querySelector("#selected-file");
    const error = form.querySelector("#upload-error");
    const feedback = ensureFeedback(form);
    const submit = form.querySelector("#submit-run");
    const localPanel = form.querySelector("#local-source-panel");
    const uploadPanel = form.querySelector("#upload-source-panel");
    const localSelect = form.querySelector("#local-source-select");
    const localStatus = form.querySelector("#local-source-status");
    const refreshButton = form.querySelector("#refresh-local-inputs");

    const showFile = (file) => {
      if (selected) {
        selected.textContent = file
          ? `${file.name} · ${(file.size / 1024 / 1024).toFixed(2)} MB`
          : "No file selected";
      }
    };

    const inputMode = () =>
      form.querySelector('input[name="input_mode"]:checked')?.value || "local";

    async function loadLocalInputs() {
      if (!localSelect) return;

      if (localStatus) {
        localStatus.textContent = "Scanning data/ and results/…";
      }

      if (refreshButton) refreshButton.disabled = true;

      try {
        const payload = await api().fetchJson("/api/local-inputs");
        const files = payload.files || [];
        const current = localSelect.value;

        localSelect.innerHTML =
          '<option value="">Select a project-local file</option>' +
          files
            .map((file) => {
              const mb = Number(file.size_bytes || 0) / 1024 / 1024;
              const size =
                mb >= 1024
                  ? `${(mb / 1024).toFixed(2)} GB`
                  : `${mb.toFixed(1)} MB`;

              return `<option value="${api().escapeHtml(
                file.path,
              )}">${api().escapeHtml(file.path)} · ${size}</option>`;
            })
            .join("");

        if (files.some((file) => file.path === current)) {
          localSelect.value = current;
        }

        if (localStatus) {
          localStatus.textContent = files.length
            ? `${files.length} local input${
                files.length === 1 ? "" : "s"
              } found. Large files are read in place; they are not sent through the browser.`
            : "No supported files found. Put an ITCH .txt/.gz under data/ or a documented CSV under results/, then refresh.";
        }
      } catch (exception) {
        localSelect.innerHTML =
          '<option value="">Could not load local files</option>';

        if (localStatus) {
          localStatus.textContent = `${
            exception.message
          } Use upload mode only for files below the configured upload limit.`;
        }
      } finally {
        if (refreshButton) refreshButton.disabled = false;
      }
    }

    function updateMode() {
      const local = inputMode() === "local";

      if (localPanel) localPanel.hidden = !local;
      if (uploadPanel) uploadPanel.hidden = local;
      if (localSelect) localSelect.required = local;
      if (fileInput) fileInput.required = !local;
      if (error) error.hidden = true;
    }

    form
      .querySelectorAll('input[name="input_mode"]')
      .forEach((radio) => radio.addEventListener("change", updateMode));

    refreshButton?.addEventListener("click", loadLocalInputs);

    fileInput?.addEventListener("change", () => showFile(fileInput.files?.[0]));

    ["dragenter", "dragover"].forEach((type) =>
      drop?.addEventListener(type, (event) => {
        event.preventDefault();
        drop.classList.add("dragover");
      }),
    );

    ["dragleave", "drop"].forEach((type) =>
      drop?.addEventListener(type, (event) => {
        event.preventDefault();
        drop.classList.remove("dragover");
      }),
    );

    drop?.addEventListener("drop", (event) => {
      const file = event.dataTransfer?.files?.[0];
      if (!file || !fileInput) return;

      const data = new DataTransfer();
      data.items.add(file);
      fileInput.files = data.files;

      showFile(file);
    });

    form.addEventListener("submit", async (event) => {
      event.preventDefault();

      const localMode = inputMode() === "local";

      if (localMode && !localSelect?.value) {
        if (error) {
          error.textContent =
            "Select a local project file first, or switch to upload mode for a smaller file.";
          error.hidden = false;
        }
        return;
      }

      if (!localMode && !fileInput?.files?.length) {
        if (error) {
          error.textContent = "Choose a file first.";
          error.hidden = false;
        }
        return;
      }

      if (error) error.hidden = true;
      feedback.hidden = true;

      if (submit) {
        submit.disabled = true;
        submit.textContent = "Submitting…";
      }

      const formData = new FormData(form);

      const parameters = {
        stock_locate: Number(formData.get("stock_locate") || 14),
        message_limit: Number(formData.get("message_limit") || 100000),
        snapshot_interval: Number(formData.get("snapshot_interval") || 20),
        price_scale: Number(formData.get("price_scale") || 100),
      };

      try {
        const response = localMode
          ? await api().fetchJson("/api/runs/local", {
              method: "POST",
              headers: { "Content-Type": "application/json" },
              body: JSON.stringify({
                ...parameters,
                path: localSelect.value,
              }),
            })
          : await api().fetchJson("/api/runs", {
              method: "POST",
              body: formData,
            });

        const run = response.run;

        showStatus(feedback, run);

        if (submit) submit.textContent = "Processing…";

        await pollRun(run.id, feedback, form);
      } catch (exception) {
        if (error) {
          error.textContent = exception.message;
          error.hidden = false;
        }

        if (submit) {
          submit.disabled = false;
          submit.textContent = "▶ Run analysis";
        }
      }
    });

    updateMode();
    loadLocalInputs();
  }

  document.addEventListener("DOMContentLoaded", () => {
    document.querySelectorAll("#upload-form").forEach(initForm);
  });
})();
