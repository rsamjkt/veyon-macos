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
//   /unduh/<tag>/<file>       file tertentu dari rilis mana pun
//
// Rilis baru: unggah file ke R2 dengan awalan tag-nya, lalu ubah RELEASE.

const RELEASE = {
	tag: "v1.3.1",
	files: {
		windows: "AruniControl-Setup-1.3.1-Diana-Windows-x64.exe",
		"windows-zip": "AruniControl-Server-1.3.1-Diana-Windows-x64.zip",
		macos: "AruniControl-1.3.1-Diana-macOS-arm64.zip",
		android: "AruniControl-1.3.1-Diana-Android-arm64.apk",
		sha256: "SHA256SUMS.txt",
	},
};

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
		let key;
		if (RELEASE.files[path]) {
			key = `${RELEASE.tag}/${RELEASE.files[path]}`;
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
