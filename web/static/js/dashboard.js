(() => {
  const $ = (selector, root = document) => root.querySelector(selector);
  const $$ = (selector, root = document) => Array.from(root.querySelectorAll(selector));
  const escapeHtml = value => String(value ?? '').replace(/[&<>"']/g, char => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[char]));
  const formatNumber = (value, digits = 2) => {
    if (value === null || value === undefined || value === '' || !Number.isFinite(Number(value))) return '—';
    return Number(value).toLocaleString(undefined, {maximumFractionDigits: digits, minimumFractionDigits: digits});
  };
  const dateLabel = value => {
    if (!value) return '—';
    const date = new Date(value);
    return Number.isNaN(date.getTime()) ? String(value) : date.toLocaleString();
  };
  const statusClass = value => `status-${['completed','running','queued','failed'].includes(value) ? value : 'queued'}`;
  const runHref = (page, id) => `${page}?run_id=${encodeURIComponent(id)}`;
  const encodePath = path => String(path).split('/').map(encodeURIComponent).join('/');

  function toast(message) {
    const node = $('#toast');
    if (!node) return;
    node.textContent = message;
    node.classList.add('show');
    clearTimeout(toast._timer);
    toast._timer = setTimeout(() => node.classList.remove('show'), 2200);
  }

  async function fetchJson(url, options) {
    const response = await fetch(url, options);
    let body;
    try { body = await response.json(); } catch (_) { body = {}; }
    if (!response.ok) throw new Error(body.error || `Request failed (${response.status})`);
    return body;
  }

  function runRow(run) {
    const summary = run.summary || {};
    const detail = run.input_kind === 'itch'
      ? `Locate ${run.config?.stock_locate ?? '—'} · limit ${run.config?.message_limit === 0 ? 'full file' : Number(run.config?.message_limit || 0).toLocaleString()}`
      : `${Number(summary.rows_validated || run.validation?.row_count || 0).toLocaleString()} CSV rows`;
    const progress = run.status === 'running' && run.progress != null ? ` · ${run.progress}%` : '';
    const liveCounters = run.input_kind === 'itch' && ['queued','running'].includes(run.status)
      ? `<small>${Number(run.messages_read || 0).toLocaleString()} messages read · ${Number(run.snapshots_captured || 0).toLocaleString()} snapshots captured</small>`
      : '';
    return `<article class="run-row">
      <div class="run-primary"><strong>${escapeHtml(run.original_filename || run.id)}</strong><small>${escapeHtml(run.id)} · ${escapeHtml(dateLabel(run.created_at))}</small><small>${escapeHtml(run.message || '')}${progress}</small>${liveCounters}</div>
      <div class="run-secondary"><span class="status-pill ${statusClass(run.status)}">${escapeHtml(run.status || 'unknown')}</span><small>${escapeHtml(run.stage || '')}</small></div>
      <div class="run-secondary">${escapeHtml(detail)}<small>${escapeHtml(run.summary?.snapshot_count != null ? `${Number(run.summary.snapshot_count).toLocaleString()} stored snapshots` : run.input_kind?.toUpperCase() || '')}</small></div>
      <div class="run-links"><a class="run-link" href="${runHref('/analysis',run.id)}">Plots</a><a class="run-link" href="${runHref('/replay',run.id)}">Replay</a><a class="run-link" href="${runHref('/experiments',run.id)}">Metadata</a></div>
      ${run.status === 'failed' ? `<div class="form-error run-error">${escapeHtml(run.error || 'Run failed.')}</div>` : ''}
    </article>`;
  }

  function fillSelect(select, runs, preferredId) {
    if (!select) return;
    const available = runs.filter(run => run.status === 'completed');
    const selected = preferredId || select.value || new URLSearchParams(location.search).get('run_id') || available[0]?.id || '';
    select.innerHTML = '<option value="">Select a completed run</option>' + available.map(run =>
      `<option value="${escapeHtml(run.id)}">${escapeHtml(run.original_filename || run.id)} · ${escapeHtml(run.id)}</option>`
    ).join('');
    if (available.some(run => run.id === selected)) select.value = selected;
  }

  async function loadRuns() {
    const data = await fetchJson('/api/runs?limit=100');
    const runs = data.runs || [];
    $$('.run-list').forEach(node => {
      node.innerHTML = runs.length ? runs.map(runRow).join('') : '<div class="empty-state">No runs yet. Upload an ITCH or observation CSV file to begin.</div>';
    });
    const total = $('#stat-total'); if (total) total.textContent = runs.length.toLocaleString();
    const complete = $('#stat-complete'); if (complete) complete.textContent = runs.filter(run => run.status === 'completed').length.toLocaleString();
    const active = $('#stat-active'); if (active) active.textContent = runs.filter(run => ['queued','running'].includes(run.status)).length.toLocaleString();
    const latest = $('#stat-latest'); if (latest) latest.textContent = runs[0] ? (runs[0].original_filename || runs[0].id) : '—';
    fillSelect($('#analysis-run-select'), runs);
    fillSelect($('#replay-run-select'), runs);
    fillSelect($('#experiment-run-select'), runs, new URLSearchParams(location.search).get('run_id') || '');
    renderExperimentTable(runs);
    return runs;
  }

  function renderExperimentTable(runs) {
    const body = $('#experiments-body');
    if (!body) return;
    if (!runs.length) { body.innerHTML = '<tr><td colspan="6" class="empty-cell">No runs have been recorded.</td></tr>'; return; }
    body.innerHTML = runs.map(run => {
      const config = run.config || {};
      const scope = run.input_kind === 'itch'
        ? `Locate ${escapeHtml(config.stock_locate)} · ${config.message_limit === 0 ? 'full file' : Number(config.message_limit || 0).toLocaleString()}`
        : 'CSV observations';
      return `<tr><td><strong>${escapeHtml(run.original_filename || run.id)}</strong><small>${escapeHtml(run.id)}</small></td>
        <td><span class="status-pill ${statusClass(run.status)}">${escapeHtml(run.status)}</span><small>${escapeHtml(run.message || '')}</small></td>
        <td>${scope}</td><td>${config.snapshot_interval ? Number(config.snapshot_interval).toLocaleString() : '—'}</td>
        <td>${escapeHtml(dateLabel(run.created_at))}</td><td><div class="table-actions"><a href="${runHref('/analysis',run.id)}">Plots</a><a href="${runHref('/replay',run.id)}">Replay</a></div></td></tr>`;
    }).join('');
  }

  async function loadAnalysis(runId) {
    const gallery = $('#plot-gallery');
    if (!gallery || !runId) return;
    gallery.innerHTML = '<div class="empty-state">Loading figures…</div>';
    try {
      const [plotResponse, artifactResponse, detailResponse] = await Promise.all([
        fetchJson(`/api/runs/${encodeURIComponent(runId)}/plots`),
        fetchJson(`/api/runs/${encodeURIComponent(runId)}/artifacts`),
        fetchJson(`/api/runs/${encodeURIComponent(runId)}`),
      ]);
      const plots = plotResponse.plots || [];
      const categories = ['all', ...new Set(plots.map(plot => plot.category))];
      const categorySelect = $('#plot-category');
      if (categorySelect) categorySelect.innerHTML = categories.map(value => `<option value="${escapeHtml(value)}">${value === 'all' ? 'All categories' : escapeHtml(value)}</option>`).join('');
      const count = $('#plot-count'); if (count) count.textContent = `${plots.length} figure${plots.length === 1 ? '' : 's'} discovered under this run's output directory.`;
      gallery.dataset.runId = runId;
      gallery._plots = plots;
      drawGallery(plots, categorySelect?.value || 'all', runId);
      const links = $('#analysis-artifact-links');
      if (links) {
        const artifacts = artifactResponse.artifacts || [];
        links.innerHTML = artifacts.map(item => `<a class="artifact-link" href="/api/runs/${encodeURIComponent(runId)}/artifacts/${encodePath(item.path)}" target="_blank" rel="noopener">${escapeHtml(item.title)}</a>`).join('');
        const source = detailResponse.run || {};
        links.insertAdjacentHTML('afterbegin', `<span class="artifact-link">${escapeHtml(source.original_filename || runId)} · ${escapeHtml(source.status)}</span>`);
      }
      const diagnostic = (artifactResponse.artifacts || []).find(item => item.path.endsWith('dashboard_diagnostics.json'));
      const diagMeta = $('#diagnostics-meta');
      if (diagnostic) {
        if (diagMeta) diagMeta.innerHTML = `Diagnostic record: <a href="/api/runs/${encodeURIComponent(runId)}/artifacts/${encodePath(diagnostic.path)}" target="_blank" rel="noopener">Open JSON</a>`;
        const button = $('#toggle-diagnostics');
        if (button) {
          button.disabled = false;
          button.onclick = async () => {
            const pre = $('#diagnostics-json');
            if (!pre.hidden) { pre.hidden = true; button.textContent = 'Show JSON'; return; }
            if (!pre.dataset.loaded) {
              const response = await fetch(`/api/runs/${encodeURIComponent(runId)}/artifacts/${encodePath(diagnostic.path)}`);
              pre.textContent = await response.text();
              pre.dataset.loaded = '1';
            }
            pre.hidden = false; button.textContent = 'Hide JSON';
          };
        }
      } else if (diagMeta) diagMeta.textContent = 'No dashboard diagnostic JSON was produced for this run.';
    } catch (error) {
      gallery.innerHTML = `<div class="empty-state">${escapeHtml(error.message)}</div>`;
    }
  }

  function drawGallery(plots, category, runId) {
    const gallery = $('#plot-gallery');
    if (!gallery) return;
    const chosen = category === 'all' ? plots : plots.filter(plot => plot.category === category);
    if (!chosen.length) { gallery.innerHTML = '<div class="empty-state">No plots in this category for this run. Missing plot outputs are not synthesized.</div>'; return; }
    gallery.innerHTML = chosen.map(plot => {
      const url = `/api/runs/${encodeURIComponent(runId)}/plots/${encodePath(plot.path)}`;
      return `<article class="plot-card"><button class="plot-image-button" data-image-url="${url}" data-image-title="${escapeHtml(plot.title)}" aria-label="Open enlarged plot: ${escapeHtml(plot.title)}"><img loading="lazy" src="${url}" alt="${escapeHtml(plot.title)}"></button><div class="plot-caption"><div><strong>${escapeHtml(plot.title)}</strong><small>${escapeHtml(plot.path)}</small></div><small>${escapeHtml(plot.category)} · ${escapeHtml(plot.format.toUpperCase())}</small></div></article>`;
    }).join('');
    $$('.plot-image-button', gallery).forEach(button => button.addEventListener('click', () => {
      const modal = $('#image-modal'); const img = $('#modal-image'); const title = $('#modal-title');
      if (!modal || !img) return;
      img.src = button.dataset.imageUrl; img.alt = button.dataset.imageTitle || 'Research plot';
      if (title) title.textContent = button.dataset.imageTitle || 'Research plot';
      modal.hidden = false; $('#close-image-modal')?.focus();
    }));
  }

  async function loadExperimentDetail(runId) {
    const node = $('#experiment-json');
    if (!node || !runId) return;
    try {
      const response = await fetchJson(`/api/runs/${encodeURIComponent(runId)}`);
      node.textContent = JSON.stringify(response.run, null, 2);
    } catch (error) { node.textContent = error.message; }
  }

  function init() {
    const menu = $('#mobile-toggle'); const sidebar = $('#research-sidebar');
    menu?.addEventListener('click', () => {
      const open = sidebar?.classList.toggle('open') || false;
      menu.setAttribute('aria-expanded', String(open));
    });
    $$('.nav-item').forEach(link => link.addEventListener('click', () => sidebar?.classList.remove('open')));
    const theme = $('#theme-toggle');
    try { if (localStorage.getItem('lob-research-theme') === 'dark') document.body.classList.add('dark'); } catch (_) {}
    theme?.addEventListener('click', () => {
      document.body.classList.toggle('dark');
      try { localStorage.setItem('lob-research-theme', document.body.classList.contains('dark') ? 'dark' : 'light'); } catch (_) {}
    });
    $('#close-image-modal')?.addEventListener('click', () => { $('#image-modal').hidden = true; });
    $('#image-modal')?.addEventListener('click', event => { if (event.target.id === 'image-modal') event.currentTarget.hidden = true; });
    document.addEventListener('keydown', event => { if (event.key === 'Escape' && $('#image-modal') && !$('#image-modal').hidden) $('#image-modal').hidden = true; });

    const category = $('#plot-category');
    category?.addEventListener('change', () => drawGallery($('#plot-gallery')?._plots || [], category.value, $('#plot-gallery')?.dataset.runId));
    $('#analysis-run-select')?.addEventListener('change', event => loadAnalysis(event.target.value));
    $('#experiment-run-select')?.addEventListener('change', event => loadExperimentDetail(event.target.value));
    $('#refresh-runs')?.addEventListener('click', () => loadRuns().then(() => toast('Run registry refreshed')).catch(err => toast(err.message)));

    loadRuns().then(runs => {
      const runId = new URLSearchParams(location.search).get('run_id');
      const analysisSelect = $('#analysis-run-select');
      if (analysisSelect && (runId || analysisSelect.value)) {
        if (runId && Array.from(analysisSelect.options).some(option => option.value === runId)) analysisSelect.value = runId;
        loadAnalysis(analysisSelect.value);
      }
      const experimentSelect = $('#experiment-run-select');
      if (experimentSelect?.value) loadExperimentDetail(experimentSelect.value);
      const refreshNeeded = runs.some(run => ['running','queued'].includes(run.status));
      if (refreshNeeded) {
        const timer = setInterval(() => loadRuns().then(updated => {
          if (!updated.some(run => ['running','queued'].includes(run.status))) clearInterval(timer);
        }).catch(() => {}), 1800);
      }
    }).catch(error => {
      $$('.run-list').forEach(node => node.innerHTML = `<div class="empty-state">API unavailable: ${escapeHtml(error.message)}</div>`);
    });
  }

  window.LobDashboard = { $, $$, escapeHtml, formatNumber, dateLabel, toast, fetchJson, loadRuns, loadAnalysis, loadExperimentDetail };
  document.addEventListener('DOMContentLoaded', init);
})();
