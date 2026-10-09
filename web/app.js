(() => {
  const storageKey = 'hybrid-lob-dashboard-checks-v1';
  const checks = Array.from(document.querySelectorAll('[data-task]'));
  const toast = document.getElementById('toast');
  let toastTimer;

  function showToast(message) {
    toast.textContent = message;
    toast.classList.add('show');
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => toast.classList.remove('show'), 1900);
  }

  function loadChecks() {
    try {
      const saved = JSON.parse(localStorage.getItem(storageKey) || '{}');
      checks.forEach(input => { input.checked = Boolean(saved[input.dataset.task]); });
    } catch (_) {
      // If local storage is unavailable or invalid, the checklist still works for this page load.
    }
    updateProgress();
  }

  function saveChecks() {
    const saved = {};
    checks.forEach(input => { saved[input.dataset.task] = input.checked; });
    try { localStorage.setItem(storageKey, JSON.stringify(saved)); } catch (_) { /* no-op */ }
    updateProgress();
  }

  function updateProgress() {
    const complete = checks.filter(input => input.checked).length;
    const total = checks.length;
    const percent = total ? Math.round((complete / total) * 100) : 0;
    document.getElementById('completedCount').textContent = complete;
    document.getElementById('totalCount').textContent = total;
    document.getElementById('progressPercent').textContent = `${percent}%`;
    document.getElementById('progressFill').style.width = `${percent}%`;
    const label = document.getElementById('progressLabel');
    label.textContent = percent === 100
      ? 'All checklist items marked complete. Run the final clean setup and test pass.'
      : percent >= 60
        ? 'Good progress. Confirm every checked item has evidence in the repository.'
        : 'Start by finishing the current workflow transition.';
    document.querySelectorAll('.roadmap-card').forEach(card => {
      const tasks = Array.from(card.querySelectorAll('[data-task]'));
      card.classList.toggle('all-done', tasks.length > 0 && tasks.every(input => input.checked));
    });
  }

  async function copyText(value) {
    try {
      await navigator.clipboard.writeText(value);
      showToast('Copied to clipboard');
    } catch (_) {
      const helper = document.createElement('textarea');
      helper.value = value;
      helper.setAttribute('readonly', '');
      helper.style.position = 'fixed';
      helper.style.opacity = '0';
      document.body.appendChild(helper);
      helper.select();
      const copied = document.execCommand('copy');
      helper.remove();
      showToast(copied ? 'Copied to clipboard' : 'Copy failed — select the command manually');
    }
  }

  checks.forEach(input => input.addEventListener('change', saveChecks));
  document.querySelectorAll('[data-copy]').forEach(button => {
    button.addEventListener('click', () => copyText(button.dataset.copy));
  });

  document.getElementById('resetProgress').addEventListener('click', () => {
    if (!window.confirm('Reset all roadmap checkboxes in this browser?')) return;
    checks.forEach(input => { input.checked = false; });
    saveChecks();
    showToast('Checklist reset');
  });

  document.getElementById('dismissNotice').addEventListener('click', () => {
    document.querySelector('.notice-strip').hidden = true;
  });

  const sidebar = document.getElementById('sidebar');
  document.getElementById('mobileMenu').addEventListener('click', () => sidebar.classList.toggle('open'));
  document.querySelectorAll('.nav-link').forEach(link => {
    link.addEventListener('click', () => {
      document.querySelectorAll('.nav-link').forEach(item => item.classList.remove('active'));
      link.classList.add('active');
      sidebar.classList.remove('open');
    });
  });

  document.getElementById('themeToggle').addEventListener('click', () => {
    document.body.classList.toggle('dark');
    try { localStorage.setItem('hybrid-lob-dashboard-dark', document.body.classList.contains('dark') ? '1' : '0'); } catch (_) { /* no-op */ }
  });
  try {
    if (localStorage.getItem('hybrid-lob-dashboard-dark') === '1') document.body.classList.add('dark');
  } catch (_) { /* no-op */ }

  const sectionIds = ['overview', 'roadmap', 'examples', 'commands', 'architecture'];
  if ('IntersectionObserver' in window) {
    const observer = new IntersectionObserver(entries => {
      const visible = entries.filter(entry => entry.isIntersecting).sort((a, b) => b.intersectionRatio - a.intersectionRatio)[0];
      if (!visible) return;
      document.querySelectorAll('.nav-link').forEach(link => {
        link.classList.toggle('active', link.getAttribute('href') === `#${visible.target.id}`);
      });
    }, { rootMargin: '-15% 0px -70% 0px', threshold: [0, .2, .5] });
    sectionIds.forEach(id => { const section = document.getElementById(id); if (section) observer.observe(section); });
  }

  loadChecks();
})();
