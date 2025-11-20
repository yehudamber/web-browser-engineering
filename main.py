import socket
import ssl

class URL:
    def __init__(self, url: str):
        self.scheme, url = url.split("://", 1)
        assert self.scheme in ["http", "https"], "Only http and https schemes are supported"
        if self.scheme == "http":
            self.port = 80
        else:
            self.port = 443
        if "/" not in url:
            self.host = url
            self.path = "/"
        else:
            self.host, url = url.split("/", 1)
            self.path = "/" + url
        if ":" in self.host:
            self.host, port = self.host.split(":", 1)
            self.port = int(port)

    def request(self):
        s = socket.socket(family=socket.AF_INET, type=socket.SOCK_STREAM, proto=socket.IPPROTO_TCP)
        s.connect((self.host, self.port))
        if self.scheme == "https":
            ctx = ssl.create_default_context()
            s = ctx.wrap_socket(s, server_hostname=self.host)
        request = f"GET {self.path} HTTP/1.1\r\n"
        request += f"Host: {self.host}\r\n"
        request += "Connection: close\r\n"
        request += "User-Agent: WebBrowserEngineering/1.0\r\n"
        request += "\r\n"
        s.send(request.encode("utf8"))
        response = s.makefile("r", encoding="utf8", newline="\r\n")
        statusLine = response.readline()
        version, status, explanation = statusLine.split(" ", 2)
        responseHeaders = {}
        while True:
            line = response.readline()
            if line == "\r\n":
                break
            header, value = line.split(":", 1)
            responseHeaders[header.casefold()] = value.strip()
        assert "transfer-encoding" not in responseHeaders, "Chunked transfer encoding not supported"
        assert "content-encoding" not in responseHeaders, "Content encoding not supported"
        content = response.read()
        s.close()
        return content
    
def show(body: str):
    inTag = False
    for c in body:
        if c == "<":
            inTag = True
        elif c == ">":
            inTag = False
        elif not inTag:
            print(c, end="")

def load(url: URL):
    body = url.request()
    show(body)

if __name__ == "__main__":
    import sys
    load(URL(sys.argv[1]))
