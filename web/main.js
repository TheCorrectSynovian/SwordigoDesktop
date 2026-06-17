/* =============================================
   SWORDIGO DESKTOP — SHARED JAVASCRIPT
   ============================================= */

// ---- STARS CANVAS ----
(function initStars() {
  const canvas = document.getElementById('stars-canvas');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  let stars = [];
  let W, H;

  function resize() {
    W = canvas.width = window.innerWidth;
    H = canvas.height = window.innerHeight;
  }

  function createStars() {
    stars = [];
    const n = Math.min(Math.floor((W * H) / 3000), 500);
    for (let i = 0; i < n; i++) {
      stars.push({
        x: Math.random() * W,
        y: Math.random() * H,
        r: Math.random() * 1.4 + 0.2,
        a: Math.random(),
        speed: Math.random() * 0.003 + 0.001,
        drift: (Math.random() - 0.5) * 0.1,
      });
    }
  }

  function draw() {
    ctx.clearRect(0, 0, W, H);
    for (const s of stars) {
      s.a += s.speed;
      s.x += s.drift;
      if (s.x > W) s.x = 0;
      if (s.x < 0) s.x = W;
      const alpha = (Math.sin(s.a) * 0.5 + 0.5) * 0.8 + 0.1;
      ctx.beginPath();
      ctx.arc(s.x, s.y, s.r, 0, Math.PI * 2);
      ctx.fillStyle = `rgba(180, 160, 255, ${alpha})`;
      ctx.fill();
    }
    requestAnimationFrame(draw);
  }

  resize();
  createStars();
  draw();
  window.addEventListener('resize', () => { resize(); createStars(); });
})();

// ---- NAVBAR SCROLL ----
(function initNavbar() {
  const navbar = document.querySelector('.navbar');
  if (!navbar) return;
  window.addEventListener('scroll', () => {
    navbar.classList.toggle('scrolled', window.scrollY > 40);
  }, { passive: true });
})();

// ---- HAMBURGER MENU ----
(function initHamburger() {
  const btn = document.querySelector('.hamburger');
  if (!btn) return;
  const nav = document.querySelector('.navbar__nav');
  const cta = document.querySelector('.navbar__cta');
  btn.addEventListener('click', () => {
    nav && nav.classList.toggle('open');
    cta && cta.classList.toggle('open');
  });
})();

// ---- SCROLL REVEAL ----
(function initReveal() {
  const observer = new IntersectionObserver(
    (entries) => entries.forEach(e => {
      if (e.isIntersecting) { e.target.classList.add('visible'); observer.unobserve(e.target); }
    }),
    { threshold: 0.12 }
  );
  document.querySelectorAll('.reveal').forEach(el => observer.observe(el));
})();

// ---- TABS ----
(function initTabs() {
  document.querySelectorAll('.tabs').forEach(tabGroup => {
    const buttons = tabGroup.querySelectorAll('.tab-btn');
    buttons.forEach(btn => {
      btn.addEventListener('click', () => {
        const target = btn.dataset.tab;
        buttons.forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        document.querySelectorAll('.tab-panel').forEach(p => {
          p.classList.toggle('active', p.id === target);
        });
      });
    });
  });
})();

// ---- ACTIVE NAV LINK ----
(function initActiveNav() {
  const path = window.location.pathname.split('/').pop() || 'index.html';
  document.querySelectorAll('.navbar__nav a').forEach(a => {
    const href = a.getAttribute('href');
    if (href === path || (path === '' && href === 'index.html')) {
      a.classList.add('active');
    }
  });
})();

// ---- SMOOTH COUNTER ANIMATION ----
function animateCounter(el, target, duration = 1800) {
  const start = performance.now();
  const isFloat = target % 1 !== 0;
  function step(now) {
    const progress = Math.min((now - start) / duration, 1);
    const ease = 1 - Math.pow(1 - progress, 3);
    const val = target * ease;
    el.textContent = isFloat ? val.toFixed(1) : Math.round(val).toLocaleString();
    if (progress < 1) requestAnimationFrame(step);
  }
  requestAnimationFrame(step);
}

// Trigger counters when visible
(function initCounters() {
  const counters = document.querySelectorAll('[data-counter]');
  if (!counters.length) return;
  const obs = new IntersectionObserver(entries => {
    entries.forEach(e => {
      if (e.isIntersecting) {
        const el = e.target;
        animateCounter(el, parseFloat(el.dataset.counter));
        obs.unobserve(el);
      }
    });
  }, { threshold: 0.5 });
  counters.forEach(c => obs.observe(c));
})();

// ---- COPY CODE BUTTON ----
document.addEventListener('click', e => {
  if (e.target.matches('.copy-btn')) {
    const code = e.target.closest('.code-block').querySelector('code, pre');
    if (!code) return;
    const text = code.textContent;
    const done = () => {
      const orig = e.target.textContent;
      e.target.textContent = '✓ Copied!';
      setTimeout(() => e.target.textContent = orig, 2000);
    };
    if (navigator.clipboard && window.isSecureContext) {
      navigator.clipboard.writeText(text).then(done);
    } else {
      // Fallback for non-HTTPS
      const ta = document.createElement('textarea');
      ta.value = text;
      ta.style.position = 'fixed';
      ta.style.opacity = '0';
      document.body.appendChild(ta);
      ta.select();
      document.execCommand('copy');
      document.body.removeChild(ta);
      done();
    }
  }
});

// ---- RANDOM TERRAIN BACKGROUNDS ----
(function initRandomTerrains() {
  const terrains = [
    'assets/caves_bg_flipped.png',
    'assets/forest_bg_flipped.png',
    'assets/grasslands_bg_flipped.png',
    'assets/graveyard_bg_flipped.png',
    'assets/grove_bg_flipped.png'
  ];
  
  // Select all main page sections (except the hero) and team cards/download blocks
  const sections = document.querySelectorAll('section:not(.hero), .download-card');
  sections.forEach((sec, idx) => {
    const terrain = terrains[idx % terrains.length];
    sec.style.position = 'relative';
    sec.style.backgroundImage = `url('${terrain}')`;
    sec.style.backgroundSize = 'cover';
    sec.style.backgroundPosition = 'center';
    sec.style.backgroundRepeat = 'no-repeat';
    sec.style.zIndex = '1';
    
    // Create overlay if it doesn't exist
    let overlay = sec.querySelector('.section-overlay');
    if (!overlay) {
      overlay = document.createElement('div');
      overlay.className = 'section-overlay';
      overlay.style.position = 'absolute';
      overlay.style.inset = '0';
      overlay.style.background = 'rgba(10, 27, 43, 0.9)'; // Dark overlay for text readability
      overlay.style.zIndex = '-1';
      overlay.style.pointerEvents = 'none';
      sec.insertBefore(overlay, sec.firstChild);
    }
  });
})();
