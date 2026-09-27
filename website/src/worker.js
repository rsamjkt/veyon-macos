// Website unduhan AruniControl.
//
// Semua halaman adalah file statis di public/. Worker ini hanya melayani
// /unduh/*, yang mengambil installer dari bucket R2:
//
//   /unduh/windows            installer Windows terbaru (.exe)
//   /unduh/windows-zip        versi portabel Windows (.zip)
//   /unduh/macos              macOS Apple Silicon (.zip)
//   /unduh/android            Android (.apk)
//   /unduh/sha256             daftar checksum SHA-256
//   /unduh/versi.json         versi terbaru + URL + SHA-256 (update otomatis)
//   /unduh/<tag>/<file>       file tertentu dari rilis mana pun
//
// Rilis baru: unggah file ke R2 dengan awalan tag-nya, lalu ubah RELEASE.

// Each download may come from its own release (e.g. while one platform's
// build is still pending); the update manifest only lists the platforms
// whose package belongs to the current tag, so older clients never
// "update" to the version they already run.
const RELEASE = {
	tag: "v1.5.0",
	codename: "Fiona",
	files: {
		windows: { tag: "v1.5.0", name: "AruniControl-Setup-1.5.0-Fiona-Windows-x64.exe" },
		"windows-zip": { tag: "v1.5.0", name: "AruniControl-Server-1.5.0-Fiona-Windows-x64.zip" },
		macos: { tag: "v1.5.0", name: "AruniControl-1.5.0-Fiona-macOS-arm64.zip" },
		android: { tag: "v1.5.0", name: "AruniControl-1.5.0-Fiona-Android-arm64.apk" },
		sha256: { tag: "v1.5.0", name: "SHA256SUMS.txt" },
	},
};

// Read by the AruniControl updater (Windows service, macOS server, Android
// app). The checksums come from SHA256SUMS.txt of the release in R2, so a
// release is only offered once all its files are uploaded.
async function manifest(env, origin) {
	const sums = await env.FILES.get(`${RELEASE.files.sha256.tag}/${RELEASE.files.sha256.name}`);
	if (sums === null) {
		return new Response("manifest not available", { status: 503 });
	}
	const checksums = {};
	for (const line of (await sums.text()).split("\n")) {
		const [hash, name] = line.trim().split(/\s+/);
		if (hash && name) {
			checksums[name] = hash.toLowerCase();
		}
	}

	const files = {};
	for (const [platform, { tag, name }] of Object.entries(RELEASE.files)) {
		if (platform === "sha256" || tag !== RELEASE.tag || !checksums[name]) {
			continue;
		}
		const head = await env.FILES.head(`${tag}/${name}`);
		if (head === null) {
			continue;
		}
		files[platform] = {
			url: `${origin}/unduh/${tag}/${name}`,
			name,
			sha256: checksums[name],
			size: head.size,
		};
	}

	return Response.json({
		version: RELEASE.tag.replace(/^v/, ""),
		codename: RELEASE.codename,
		tag: RELEASE.tag,
		files,
	}, { headers: { "Cache-Control": "public, max-age=300" } });
}

const CONTENT_TYPES = {
	apk: "application/vnd.android.package-archive",
	exe: "application/vnd.microsoft.portable-executable",
	zip: "application/zip",
	txt: "text/plain; charset=utf-8",
};

export default {
	async fetch(request, env) {
		const url = new URL(request.url);
		if (!url.pathname.startsWith("/unduh/")) {
			return env.ASSETS.fetch(request);
		}
		if (request.method !== "GET" && request.method !== "HEAD") {
			return new Response("method not allowed", { status: 405, headers: { Allow: "GET, HEAD" } });
		}

		const path = decodeURIComponent(url.pathname.slice("/unduh/".length));
		if (path === "versi.json") {
			return manifest(env, url.origin);
		}
		let key;
		if (RELEASE.files[path]) {
			key = `${RELEASE.files[path].tag}/${RELEASE.files[path].name}`;
		} else if (/^v\d+\.\d+\.\d+\/[A-Za-z0-9._-]+$/.test(path)) {
			key = path;
		} else {
			return new Response("file tidak ditemukan", { status: 404 });
		}

		const object = await env.FILES.get(key, { range: request.headers, onlyIf: request.headers });
		if (object === null) {
			return new Response("file tidak ditemukan", { status: 404 });
		}

		const name = key.split("/").pop();
		const extension = name.split(".").pop().toLowerCase();
		const headers = new Headers();
		object.writeHttpMetadata(headers);
		headers.set("Content-Type", CONTENT_TYPES[extension] ?? "application/octet-stream");
		headers.set("Content-Disposition", extension === "txt" ? "inline" : `attachment; filename="${name}"`);
		headers.set("ETag", object.httpEtag);
		headers.set("Accept-Ranges", "bytes");
		headers.set("Cache-Control", "public, max-age=3600");

		// onlyIf did not match (If-None-Match etc.): body is absent
		if (!("body" in object)) {
			return new Response(null, { status: 304, headers });
		}

		let status = 200;
		if (object.range && request.headers.has("Range")) {
			const range = object.range;
			const suffix = range.suffix !== undefined;
			const offset = suffix ? object.size - range.suffix : (range.offset ?? 0);
			const length = suffix ? range.suffix : (range.length ?? object.size - offset);
			headers.set("Content-Range", `bytes ${offset}-${offset + length - 1}/${object.size}`);
			headers.set("Content-Length", String(length));
			status = 206;
		} else {
			headers.set("Content-Length", String(object.size));
		}

		return new Response(request.method === "HEAD" ? null : object.body, { status, headers });
	},
};
