#!/usr/bin/env python3
"""Builds public/index.html from index.src.html by inlining the icon sprite.

Icons come from the Android app (mobile/icons, Material Symbols Rounded), so
the website and the app look the same. Run after editing index.src.html:

    python3 build.py && npx wrangler deploy
"""
import pathlib
import re

HERE = pathlib.Path(__file__).parent
ICONS = HERE.parent / "mobile" / "icons"

# simple brand marks for the three platforms
OS_ICONS = {
    "os-windows": ('0 0 24 24', '<path fill="currentColor" d="M3 5.5 10.5 4.4v7.1H3zm0 13 7.5 1.1v-7H3zm8.4 1.2L21 21v-8.4h-9.6zm0-15.4v7.2H21V3z"/>'),
    "os-apple": ('0 0 24 24', '<path fill="currentColor" d="M16.4 12.7c0-2.5 2.1-3.7 2.2-3.8-1.2-1.7-3-2-3.7-2-1.6-.2-3 .9-3.8.9s-2-.9-3.3-.9C6.1 7 4.5 8 3.6 9.6c-1.8 3.1-.5 7.8 1.3 10.3.9 1.2 1.9 2.6 3.2 2.6 1.3-.1 1.8-.8 3.3-.8s2 .8 3.3.8c1.4 0 2.2-1.3 3.1-2.5 1-1.4 1.4-2.8 1.4-2.9 0 0-2.8-1.1-2.8-4.4zM13.9 5.3c.7-.9 1.2-2 1-3.2-1 0-2.3.7-3 1.6-.7.8-1.2 2-1.1 3.1 1.2.1 2.3-.6 3.1-1.5z"/>'),
    "os-android": ('0 0 24 24', '<path fill="currentColor" d="M17.6 9.5 19.4 6.4a.4.4 0 0 0-.7-.4l-1.9 3.2A11.2 11.2 0 0 0 12 8.2c-1.7 0-3.3.4-4.8 1L5.3 6a.4.4 0 1 0-.7.4l1.8 3.1A10.5 10.5 0 0 0 1 18h22a10.5 10.5 0 0 0-5.4-8.5zM7 15.3a1 1 0 1 1 0-2 1 1 0 0 1 0 2zm10 0a1 1 0 1 1 0-2 1 1 0 0 1 0 2z"/>'),
}

src = (HERE / "index.src.html").read_text()
used = sorted(set(re.findall(r'href="#i-([a-z_]+)"', src)))

symbols = []
for name in used:
    svg = (ICONS / f"{name}.svg").read_text()
    viewbox = re.search(r'viewBox="([^"]+)"', svg).group(1)
    body = re.search(r"<svg[^>]*>(.*)</svg>", svg, re.S).group(1)
    body = body.replace("<path ", '<path fill="currentColor" ')
    symbols.append(f'<symbol id="i-{name}" viewBox="{viewbox}">{body}</symbol>')
for name, (viewbox, body) in OS_ICONS.items():
    symbols.append(f'<symbol id="{name}" viewBox="{viewbox}">{body}</symbol>')

sprite = '<svg width="0" height="0" style="position:absolute" aria-hidden="true">' + "".join(symbols) + "</svg>"
(HERE / "public" / "index.html").write_text(src.replace("<!--SPRITE-->", sprite))
print(f"public/index.html: {len(used)} icons")
