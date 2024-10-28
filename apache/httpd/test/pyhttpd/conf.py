from typing import Dict, Any

from pyhttpd.env import HttpdTestEnv


class HttpdConf(object):

    def __init__(self, env: HttpdTestEnv, extras: Dict[str, Any] = None):
        """ Create a new httpd configuration.
        :param env: then environment this operates in
        :param extras: extra configuration directive with ServerName as key and
                       'base' as special key for global configuration additions.
        """
        self.env = env
        self._indents = 0
        self._lines = []
        self._extras = extras.copy() if extras else {}
        if 'base' in self._extras:
            self.add(self._extras['base'])
        self._tls_engine_ports = set()

    def __repr__(self):
        s = '\n'.join(self._lines)
        return f"HttpdConf[{s}]"

    def install(self):
        self.env.install_test_conf(self._lines)

    def replacetlsstr(self, line):
            l = line.replace("TLS_", "")
            l = l.replace("\n", " ")
            l = l.replace("\\", " ")
            l = " ".join(l.split())
            l = l.replace(" ", ":")
            l = l.replace("_", "-")
            l = l.replace("-WITH", "")
            l = l.replace("AES-", "AES")
            l = l.replace("POLY1305-SHA256", "POLY1305")
            return l

    def replaceinstr(self, line):
        if line.startswith("TLSCiphersPrefer"):
            # the "TLS_" are changed into "".
            l = self.replacetlsstr(line)
            l = l.replace("TLSCiphersPrefer:", "SSLCipherSuite ")
        elif line.startswith("TLSCiphersSuppress"):
            # like SSLCipherSuite but with :!
            l = self.replacetlsstr(line)
            l = l.replace("TLSCiphersSuppress:", "SSLCipherSuite !")
            l = l.replace(":", ":!")
        elif line.startswith("TLSCertificate"):
            l = line.replace("TLSCertificate", "SSLCertificateFile")
        elif line.startswith("TLSProtocol"):
            # mod_ssl is different (+ no supported and 0x code have to be translated)
            l = line.replace("TLSProtocol", "SSLProtocol")
            l = l.replace("+", "")
            l = l.replace("default", "all")
            l = l.replace("0x0303", "1.2") # need to check 1.3 and 1.1
        elif line.startswith("SSLProtocol"):
            l = line # we have that in test/modules/tls/test_05_proto.py
        elif line.startswith("TLSHonorClientOrder"):
            # mod_ssl has SSLHonorCipherOrder on = use server off = use client.
            l = line.lower()
            if "on" in l:
                l = "SSLHonorCipherOrder off"
            else:
                l = "SSLHonorCipherOrder on"
        elif line.startswith("TLSEngine"):
            # In fact it should go in the corresponding VirtualHost... Not sure how to do that.
            l = "SSLEngine On"
        else:
            if line != "":
                l = line.replace("TLS", "SSL")
            else:
                l = line
        return l

    def add(self, line: Any):
        # make we transform the TLS to SSL if we are using mod_ssl
        if isinstance(line, str):
            if not HttpdTestEnv.has_shared_module("tls"):
                line = self.replaceinstr(line)
            if self._indents > 0:
                line = f"{'  ' * self._indents}{line}"
            self._lines.append(line)
        else:
            if not HttpdTestEnv.has_shared_module("tls"):
                new = []
                previous = ""
                for l in line:
                    if previous.startswith("SSLCipherSuite"):
                        if l.startswith("TLSCiphersPrefer") or l.startswith("TLSCiphersSuppress"):
                            # we need to merge it   
                            l = self.replaceinstr(l)
                            l = l.replace("SSLCipherSuite ", ":")
                            previous = previous + l
                            continue
                        else:
                            if self._indents > 0:
                                previous = f"{'  ' * self._indents}{previous}"
                            new.append(previous)
                            previous = ""
                    l = self.replaceinstr(l)
                    if l.startswith("SSLCipherSuite"):
                        previous = l
                        continue
                    if self._indents > 0:
                        l = f"{'  ' * self._indents}{l}"
                    new.append(l)
                if previous != "":
                    if self._indents > 0:
                        previous = f"{'  ' * self._indents}{previous}"
                    new.append(previous)
                self._lines.extend(new)
            else:
                if self._indents > 0:
                    line = [f"{'  ' * self._indents}{l}" for l in line]
                self._lines.extend(line)
        return self

    def add_certificate(self, cert_file, key_file, ssl_module=None):
        if ssl_module is None:
            ssl_module = self.env.ssl_module
        if ssl_module == 'mod_ssl':
            self.add([
                f"SSLCertificateFile {cert_file}",
                f"SSLCertificateKeyFile {key_file if key_file else cert_file}",
            ])
        elif ssl_module == 'mod_tls':
            self.add(f"TLSCertificate {cert_file} {key_file if key_file else ''}")
        elif ssl_module == 'mod_gnutls':
            self.add([
                f"GnuTLSCertificateFile {cert_file}",
                f"GnuTLSKeyFile {key_file if key_file else cert_file}",
            ])
        else:
            raise Exception(f"unsupported ssl module: {ssl_module}")

    def add_vhost(self, domains, port=None, doc_root="htdocs", with_ssl=None,
                  with_certificates=None, ssl_module=None):
        self.start_vhost(domains=domains, port=port, doc_root=doc_root,
                         with_ssl=with_ssl, with_certificates=with_certificates,
                         ssl_module=ssl_module)
        self.end_vhost()
        return self

    def start_vhost(self, domains, port=None, doc_root="htdocs", with_ssl=None,
                    ssl_module=None, with_certificates=None):
        if not isinstance(domains, list):
            domains = [domains]
        if port is None:
            port = self.env.https_port
        if ssl_module is None:
            ssl_module = self.env.ssl_module
        if with_ssl is None:
            with_ssl = self.env.https_port == port
        if with_ssl and ssl_module == 'mod_tls' and port not in self._tls_engine_ports:
            self.add(f"TLSEngine {port}")
            self._tls_engine_ports.add(port)
        self.add("")
        self.add(f"<VirtualHost *:{port}>")
        self._indents += 1
        self.add(f"ServerName {domains[0]}")
        for alias in domains[1:]:
            self.add(f"ServerAlias {alias}")
        self.add(f"DocumentRoot {doc_root}")
        if with_ssl:
            if ssl_module == 'mod_ssl':
                self.add("SSLEngine on")
            elif ssl_module == 'mod_gnutls':
                self.add("GnuTLSEnable on")
            if with_certificates is not False:
                for cred in self.env.get_credentials_for_name(domains[0]):
                    self.add_certificate(cred.cert_file, cred.pkey_file, ssl_module=ssl_module)
        if domains[0] in self._extras:
            self.add(self._extras[domains[0]])
        return self
                  
    def end_vhost(self):
        self._indents -= 1
        self.add("</VirtualHost>")
        self.add("")
        return self

    def add_proxies(self, host, proxy_self=False, h2proxy_self=False):
        if proxy_self or h2proxy_self:
            self.add("ProxyPreserveHost on")
        if proxy_self:
            self.add([
                f"ProxyPass /proxy/ http://127.0.0.1:{self.env.http_port}/",
                f"ProxyPassReverse /proxy/ http://{host}.{self.env.http_tld}:{self.env.http_port}/",
            ])
        if h2proxy_self:
            self.add([
                f"ProxyPass /h2proxy/ h2://127.0.0.1:{self.env.https_port}/",
                f"ProxyPassReverse /h2proxy/ https://{host}.{self.env.http_tld}:self.env.https_port/",
            ])
        return self
    
    def add_vhost_test1(self, proxy_self=False, h2proxy_self=False):
        domain = f"test1.{self.env.http_tld}"
        self.start_vhost(domains=[domain, f"www1.{self.env.http_tld}"],
                         port=self.env.http_port, doc_root="htdocs/test1")
        self.end_vhost()
        self.start_vhost(domains=[domain, f"www1.{self.env.http_tld}"],
                         port=self.env.https_port, doc_root="htdocs/test1")
        self.add([
            "<Location /006>",
            "    Options +Indexes",
            "</Location>",
        ])
        self.add_proxies("test1", proxy_self, h2proxy_self)
        self.end_vhost()
        return self

    def add_vhost_test2(self):
        domain = f"test2.{self.env.http_tld}"
        self.start_vhost(domains=[domain, f"www2.{self.env.http_tld}"],
                         port=self.env.http_port, doc_root="htdocs/test2")
        self.end_vhost()
        self.start_vhost(domains=[domain, f"www2.{self.env.http_tld}"],
                         port=self.env.https_port, doc_root="htdocs/test2")
        self.add([
            "<Location /006>",
            "    Options +Indexes",
            "</Location>",
        ])
        self.end_vhost()
        return self

    def add_vhost_cgi(self, proxy_self=False, h2proxy_self=False):
        domain = f"cgi.{self.env.http_tld}"
        if proxy_self:
            self.add(["ProxyStatus on", "ProxyTimeout 5",
                      "SSLProxyEngine on", "SSLProxyVerify none"])
        if h2proxy_self:
            self.add(["SSLProxyEngine on", "SSLProxyCheckPeerName off"])
        self.start_vhost(domains=[domain, f"cgi-alias.{self.env.http_tld}"],
                         port=self.env.https_port, doc_root="htdocs/cgi")
        self.add_proxies("cgi", proxy_self=proxy_self, h2proxy_self=h2proxy_self)
        self.end_vhost()
        self.start_vhost(domains=[domain, f"cgi-alias.{self.env.http_tld}"],
                         port=self.env.http_port, doc_root="htdocs/cgi")
        self.add("AddHandler cgi-script .py")
        self.add_proxies("cgi", proxy_self=proxy_self, h2proxy_self=h2proxy_self)
        self.end_vhost()
        return self

    @staticmethod
    def merge_extras(e1: Dict[str, Any], e2: Dict[str, Any]) -> Dict[str, Any]:
        def _concat(v1, v2):
            if isinstance(v1, str):
                v1 = [v1]
            if isinstance(v2, str):
                v2 = [v2]
            v1.extend(v2)
            return v1

        if e1 is None:
            return e2.copy() if e2 else None
        if e2 is None:
            return e1.copy()
        e3 = e1.copy()
        for name, val in e2.items():
            if name in e3:
                e3[name] = _concat(e3[name], val)
            else:
                e3[name] = val
        return e3
