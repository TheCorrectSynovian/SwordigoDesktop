# Implementation Plan – Replace Placeholder Images with Real Swordigo Assets

## Goal
Replace all placeholder images (e.g., `hero.png`, `og-image.png`) with actual Swordigo fan‑art assets from `src/assets/`. Update HTML, CSS, and meta tags so the website displays the correct visuals and improves SEO.

## User Review Required
[!IMPORTANT]
- Confirm which asset you want as the primary hero image (e.g., `pic0.png` or `launcher_bg.png`).
- Approve replacing `og-image.png` with a real image (suggest using `pic0.png`).

## Open Questions
- Do you want a separate thumbnail for the OG image, or reuse the hero image?
- Should we keep any existing `assets/` subfolders for icons, or move everything directly under `web/assets/`?

## Proposed Changes
### 1. Assets
- **Copy** all files from `src/assets/` → `web/assets/` (already done).
- **Delete** placeholder files `hero.png` and `og-image.png` if they exist.

### 2. CSS (`web/style.css`)
- Update the `.hero__bg` rule to use `--hero-image` variable pointing to `url('assets/pic0.png')`.
- Add fallback background color matching the new palette.

### 3. HTML (`web/*.html`)
- Replace `<img src="assets/hero.png" ...>` with `<img src="assets/pic0.png" ...>`.
- Update all `<meta property="og:image" ...>` and `<meta name="twitter:image" ...>` to `https://thecorrectsynovian.github.io/SwordigoDesktop/web/assets/pic0.png` (or chosen image).
- Verify that any other pages referencing `assets/hero.png` are corrected.

### 4. SEO JSON‑LD
- Update the `"screenshot"` field to point to the chosen hero image URL.

### 5. Verify
- Load the site locally and ensure all images appear.
- Check the browser dev tools for 404 requests.
- Validate Open Graph tags with the Facebook Sharing Debugger (optional).

## Verification Plan
### Automated Tests
- Run `npm run lint` (if lint config exists) to catch broken paths.
- Use `grep` to ensure no remaining `hero.png` or `og-image.png` strings.

### Manual Verification
- Open `index.html` in a browser and confirm the hero image shows.
- Inspect meta tags in page source.
- Verify other pages (`download.html`, `technical.html`, `research.html`, `changelog.html`) display the correct image.

---
*Once you approve, I will apply the changes.*
