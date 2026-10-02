// AruniControl website - shared behaviour of the home page and the guide.
// Only transform/opacity are animated; everything driven by scroll runs in
// one requestAnimationFrame per frame.
(() => {
	document.documentElement.classList.remove("no-js");
	const reduced = matchMedia("(prefers-reduced-motion: reduce)").matches;
	const $ = (s, root = document) => root.querySelector(s);
	const $$ = (s, root = document) => [...root.querySelectorAll(s)];
	const clamp = (v, a = 0, b = 1) => Math.min(b, Math.max(a, v));

	// ---------- navigation ----------
	const gnav = $("#gnav");
	$("#gnav-menu")?.addEventListener("click", () => {
		const open = gnav.classList.toggle("open");
		$("#gnav-menu").setAttribute("aria-expanded", open);
		document.body.style.overflow = open ? "hidden" : "";
	});
	$$("#gnav-sheet a").forEach((a) => a.addEventListener("click", () => {
		gnav.classList.remove("open");
		document.body.style.overflow = "";
	}));
	const lnav = $("#lnav");

	// ---------- reveal on scroll (cards of a row one after the other) ----------
	$$(".bento .bento-card, .downloads .download, .sec-grid .sec-item, .exam-facts > div").forEach((el) => {
		const index = [...el.parentElement.children].indexOf(el);
		el.style.transitionDelay = `${(index % 3) * 100}ms`;
	});
	const io = new IntersectionObserver((entries) => entries.forEach((e) => {
		if (e.isIntersecting) {
			e.target.classList.add("in");
			io.unobserve(e.target);
		}
	}), { threshold: 0.15, rootMargin: "0px 0px -60px 0px" });
	$$(".reveal").forEach((el) => io.observe(el));

	// ---------- scroll-driven scenes ----------
	const heroStage = $("#hero-stage");
	const story = $(".story");
	const storyZoom = $("#story-zoom");
	const storyLock = $("#story-lock");
	const captions = $$(".story-caption");
	const dots = $$(".story-progress i");
	const parallax = $$(".parallax");
	let ticking = false;

	function frame() {
		ticking = false;
		const vh = innerHeight;
		lnav?.classList.toggle("stuck", lnav.getBoundingClientRect().top <= 0 && scrollY > 60);

		if (reduced) {
			return;
		}

		if (heroStage) {
			const p = clamp(scrollY / (vh * 0.9));
			heroStage.style.setProperty("--p", p.toFixed(4));
		}

		if (story) {
			const rect = story.getBoundingClientRect();
			const total = rect.height - vh;
			const p = clamp(-rect.top / total);
			const step = p < 0.34 ? 0 : p < 0.67 ? 1 : 2;
			captions.forEach((c, i) => c.classList.toggle("active", i === step));
			dots.forEach((d, i) => d.classList.toggle("on", i === step));
			// zoom into one computer, then lock the class
			const zoom = 1 + 1.6 * clamp((p - 0.22) / 0.3) - 1.6 * clamp((p - 0.62) / 0.12);
			storyZoom.style.transform = `scale(${zoom.toFixed(4)})`;
			storyLock.classList.toggle("on", p > 0.7);
		}

		parallax.forEach((el) => {
			const rect = el.getBoundingClientRect();
			const center = rect.top + rect.height / 2 - vh / 2;
			el.style.transform = `translate3d(0, ${(center * parseFloat(el.dataset.speed || 0)).toFixed(1)}px, 0)`;
		});
	}
	const requestFrame = () => {
		if (!ticking) {
			ticking = true;
			requestAnimationFrame(frame);
		}
	};
	addEventListener("scroll", requestFrame, { passive: true });
	addEventListener("resize", requestFrame);
	frame();
	// ---------- highlights carousel ----------
	const carousel = $("#carousel");
	if (carousel) {
		const cards = $$(".hl-card", carousel);
		const dotsBox = $("#carousel-dots");
		cards.forEach((card, i) => {
			const dot = document.createElement("button");
			dot.setAttribute("aria-label", `Kartu ${i + 1}`);
			dot.addEventListener("click", () => card.scrollIntoView({ behavior: "smooth", inline: "center", block: "nearest" }));
			dotsBox.appendChild(dot);
		});
		const step = () => cards[0].getBoundingClientRect().width + 20;
		$("#carousel-prev").addEventListener("click", () => carousel.scrollBy({ left: -step(), behavior: "smooth" }));
		$("#carousel-next").addEventListener("click", () => carousel.scrollBy({ left: step(), behavior: "smooth" }));
		const update = () => {
			const center = carousel.scrollLeft + carousel.clientWidth / 2;
			let active = 0;
			cards.forEach((card, i) => {
				if (Math.abs(card.offsetLeft + card.offsetWidth / 2 - center) < Math.abs(cards[active].offsetLeft + cards[active].offsetWidth / 2 - center)) {
					active = i;
				}
			});
			$$("button", dotsBox).forEach((d, i) => d.classList.toggle("on", i === active));
			$("#carousel-prev").disabled = carousel.scrollLeft < 8;
			$("#carousel-next").disabled = carousel.scrollLeft + carousel.clientWidth > carousel.scrollWidth - 8;
		};
		carousel.addEventListener("scroll", () => requestAnimationFrame(update), { passive: true });
		update();
	}

	// ---------- terminal types itself when it comes into view ----------
	const terminal = $("#terminal-live");
	if (terminal) {
		const lines = $$(".t-line", terminal);
		if (reduced) {
			lines.forEach((l) => (l.textContent = l.dataset.text));
		} else {
			const typeLine = (i) => {
				if (i >= lines.length) return;
				const line = lines[i];
				const text = line.dataset.text;
				const fast = i > 0;
				line.classList.add("typing");
				let n = 0;
				const tick = () => {
					n += fast ? text.length : 2;
					line.textContent = text.slice(0, n);
					if (n < text.length) {
						setTimeout(tick, 22);
					} else {
						line.classList.remove("typing");
						setTimeout(() => typeLine(i + 1), i === 0 ? 500 : 650);
					}
				};
				tick();
			};
			new IntersectionObserver((entries, obs) => {
				if (entries[0].isIntersecting) {
					obs.disconnect();
					setTimeout(() => typeLine(0), 400);
				}
			}, { threshold: 0.5 }).observe(terminal);
		}
	}

	// ---------- copy buttons ----------
	$$("[data-copy]").forEach((button) => button.addEventListener("click", async () => {
		const text = document.getElementById(button.dataset.copy).textContent;
		try {
			await navigator.clipboard.writeText(text);
			button.textContent = "Tersalin";
			button.classList.add("done");
			setTimeout(() => { button.textContent = "Salin"; button.classList.remove("done"); }, 1800);
		} catch {
			getSelection().selectAllChildren(document.getElementById(button.dataset.copy));
		}
	}));

	// ---------- recommend the download for this device ----------
	const ua = navigator.userAgent;
	const os = /Android/i.test(ua) ? "android" : /Mac/i.test(ua) && !/iPhone|iPad/.test(ua) ? "mac" : /Windows/i.test(ua) ? "windows" : null;
	if (os) {
		$(`.download[data-os="${os}"]`)?.classList.add("recommended");
		const hero = $("#hero-download");
		if (hero) {
			hero.href = { windows: "/unduh/windows", mac: "/unduh/macos", android: "/unduh/android" }[os];
			hero.querySelector("span").textContent = { windows: "Unduh untuk Windows", mac: "Unduh untuk Mac", android: "Unduh untuk Android" }[os];
		}
	}

	// ---------- guide: tabs ----------
	const tabs = $$('[role="tab"]');
	function selectTab(tab) {
		if (!tab) return;
		tabs.forEach((t) => {
			const on = t === tab;
			t.setAttribute("aria-selected", on);
			document.getElementById(t.getAttribute("aria-controls")).hidden = !on;
		});
	}
	tabs.forEach((t) => t.addEventListener("click", () => selectTab(t)));
	$$("[data-tab]").forEach((a) => a.addEventListener("click", () => selectTab(document.getElementById(a.dataset.tab))));
	if (os && tabs.length) {
		selectTab(document.getElementById({ windows: "t-windows", mac: "t-mac", android: "t-android" }[os]));
	}

	// ---------- guide: table of contents follows the reading position ----------
	const toc = $("#guide-toc");
	if (toc) {
		const links = $$("a", toc);
		const sections = links.map((a) => document.getElementById(a.hash.slice(1))).filter(Boolean);
		const spy = new IntersectionObserver((entries) => entries.forEach((e) => {
			if (e.isIntersecting) {
				links.forEach((a) => a.classList.toggle("on", a.hash === `#${e.target.id}`));
			}
		}), { rootMargin: "-30% 0px -60% 0px" });
		sections.forEach((s) => spy.observe(s));
	}

	// ---------- lightbox ----------
	const box = $("#lightbox");
	if (box) {
		document.addEventListener("click", (e) => {
			if (e.target.classList?.contains("zoomable")) {
				box.querySelector("img").src = e.target.currentSrc || e.target.src;
				box.classList.add("open");
			} else if (box.contains(e.target)) {
				box.classList.remove("open");
			}
		});
		addEventListener("keydown", (e) => e.key === "Escape" && box.classList.remove("open"));
	}
})();
