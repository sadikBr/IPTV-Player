#!/usr/bin/env python3
"""Serves an MPEG-TS file like a live IPTV channel, at playback speed, and
cuts the connection on purpose to test reconnecting.

usage: stream_server.py FILE SECONDS_OF_MEDIA DROPS [DROP_AFTER]
The first DROPS connections are closed after DROP_AFTER seconds (default 5);
later connections stream the whole file. Prints the port, then one line per
connection."""
import http.server
import socketserver
import sys
import time

path, media_secs, drops = sys.argv[1], float(sys.argv[2]), int(sys.argv[3])
drop_after = float(sys.argv[4]) if len(sys.argv) > 4 else 5.0
data = open(path, "rb").read()
rate = len(data) / media_secs
count = 0


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        global count
        count += 1
        n = count
        print(f"connection {n}", flush=True)
        self.send_response(200)
        self.send_header("Content-Type", "video/mp2t")
        self.end_headers()
        limit = drop_after if n <= drops else media_secs + 5
        start = time.time()
        sent = 0
        try:
            while sent < len(data) and time.time() - start < limit:
                target = min(len(data), int((time.time() - start + 2) * rate))
                if target > sent:
                    self.wfile.write(data[sent:target])
                    sent = target
                time.sleep(0.05)
        except OSError:
            pass
        print(f"closed {n}", flush=True)


class Server(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True


server = Server(("127.0.0.1", 0), Handler)
print(f"port {server.server_address[1]}", flush=True)
server.serve_forever()
