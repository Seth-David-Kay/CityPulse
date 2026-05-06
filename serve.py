#!/usr/bin/env python3
import os
from http.server import HTTPServer, SimpleHTTPRequestHandler

class no_cache_handler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cache-Control", "no-cache, no-store, must-revalidate")
        super().end_headers()

os.chdir("build")
HTTPServer(("", 8001), no_cache_handler).serve_forever()
