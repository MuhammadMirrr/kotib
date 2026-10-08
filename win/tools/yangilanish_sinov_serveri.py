#!/usr/bin/env python3
"""Avto-yangilanish (Windows) uchdan-uchga sinov serveri — `stat.mirqobilov.com` va
`cdn.mirqobilov.com` oʻrnida, Mac'da, 127.0.0.1:8443.

    python3 -I win/tools/yangilanish_sinov_serveri.py <ildiz>

<ildiz> ichida:
    javob.json         imzolangan siyosat (`reliz.sh imzola … --kalit-fayli <sinov kaliti>`)
    srv/dl/win/*.exe   manifestdagi fayllar (`dl/win/…` yoʻli bilan)
    tls/srv.crt, .key  ikkala nom uchun sertifikat (oʻz sinov CA'mizdan)
    rejim              oddiy | buzuq_fayl | buzuq_imzo | uzilish | sekin | yoq
    server.log         har soʻrov shu yerga yoziladi

VM'ni ulash, sinov build'i va tartib — AGENTS.md → «Update end-to-end test stand».
"""
import http.server, ssl, os, sys, time, threading
ILDIZ = sys.argv[1]
JAVOB = os.path.join(ILDIZ, "javob.json")
uzildi = set()  # «uzilish» har fayl uchun bir marta (server qayta ishga tushguncha)
def rejim():
    try: return open(os.path.join(ILDIZ, "rejim")).read().strip()
    except OSError: return "oddiy"
def log(s):
    with open(os.path.join(ILDIZ, "server.log"), "a") as f: f.write(time.strftime("%H:%M:%S ") + s + "\n")
class H(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    def log_message(self, *a): pass
    def javob(self, kod, tana=b"", tur="application/json", sarlavha=None):
        self.send_response(kod); self.send_header("Content-Type", tur)
        self.send_header("Content-Length", str(len(tana)))
        for k, v in (sarlavha or {}).items(): self.send_header(k, v)
        self.end_headers(); self.wfile.write(tana)
    def do_POST(self):
        n = int(self.headers.get("Content-Length") or 0); self.rfile.read(n)
        log(f"POST {self.headers.get('Host')}{self.path}"); self.javob(204)
    def do_HEAD(self): self.do_GET(head=True)
    def do_GET(self, head=False):
        host = (self.headers.get("Host") or "").split(":")[0]; r = rejim()
        log(f"{'HEAD' if head else 'GET'} {host}{self.path} rejim={r} range={self.headers.get('Range')}")
        if host == "stat.mirqobilov.com":
            if self.path.startswith("/v1/yangilanish/win"):
                if r == "yoq": return self.javob(404, b'{"xato":"yoq"}')
                t = open(JAVOB, "rb").read()
                if r == "buzuq_imzo": t = t.replace(b'"m":"eyJ', b'"m":"eyK', 1)
                return self.javob(200, t)
            return self.javob(204)
        if host == "cdn.mirqobilov.com" and self.path.startswith("/dl/win/"):
            p = os.path.join(ILDIZ, "srv", self.path.lstrip("/"))
            if not os.path.isfile(p): return self.javob(404)
            d = bytearray(open(p, "rb").read())
            if r == "buzuq_fayl": d[len(d)//2] ^= 1
            bosh, kod, sarl = 0, 200, {"Accept-Ranges": "bytes"}
            rg = self.headers.get("Range")
            if rg and rg.startswith("bytes="):
                bosh = int(rg[6:].split("-")[0])
                if bosh >= len(d): return self.javob(416, b"", sarlavha={"Content-Range": f"bytes */{len(d)}"})
                kod = 206; sarl["Content-Range"] = f"bytes {bosh}-{len(d)-1}/{len(d)}"
            qism = bytes(d[bosh:])
            self.send_response(kod); self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(qism)))
            for k, v in sarl.items(): self.send_header(k, v)
            self.end_headers()
            if head: return
            if r == "uzilish" and p not in uzildi and bosh == 0:
                uzildi.add(p); self.wfile.write(qism[: len(qism)//2]); self.wfile.flush()
                log(f"  uzildi {len(qism)//2} baytdan keyin"); self.close_connection = True
                self.connection.shutdown(2); return
            if r == "sekin":
                for i in range(0, len(qism), 256 * 1024):
                    self.wfile.write(qism[i:i + 256 * 1024]); self.wfile.flush(); time.sleep(1.5)
            else:
                self.wfile.write(qism)
            log(f"  yuborildi {len(qism)} bayt ({kod})"); return
        self.javob(404)
srv = http.server.ThreadingHTTPServer(("127.0.0.1", 8443), H)
kx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
kx.load_cert_chain(os.path.join(ILDIZ, "tls/srv.crt"), os.path.join(ILDIZ, "tls/srv.key"))
srv.socket = kx.wrap_socket(srv.socket, server_side=True)
log("server ishga tushdi"); srv.serve_forever()
