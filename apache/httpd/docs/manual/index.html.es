<?xml version="1.0" encoding="ISO-8859-1"?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="es" xml:lang="es"><head>
<meta content="text/html; charset=ISO-8859-1" http-equiv="Content-Type" />
<!--
        XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
              This file is generated from xml source: DO NOT EDIT
        XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
      -->
<title>Apache HTTP Server Versi&#243;n 2.4
Documentaci&#243;n - Servidor HTTP Apache Versi&#243;n 2.4</title>
<link href="./style/css/manual.css" rel="stylesheet" media="all" type="text/css" title="Main stylesheet" />
<link href="./style/css/manual-loose-100pc.css" rel="alternate stylesheet" media="all" type="text/css" title="No Sidebar - Default font size" />
<link href="./style/css/manual-print.css" rel="stylesheet" media="print" type="text/css" /><link rel="stylesheet" type="text/css" href="./style/css/prettify.css" />
<script src="./style/scripts/prettify.min.js" type="text/javascript">
</script>

<link href="./images/favicon.ico" rel="shortcut icon" /></head>
<body id="index-page">
<div id="page-header">
<p class="menu"><a href="./mod/">M&#243;dulos</a> | <a href="./mod/directives.html">Directivas</a> | <a href="http://wiki.apache.org/httpd/FAQ">Preguntas Frecuentes</a> | <a href="./glossary.html">Glosario</a> | <a href="./sitemap.html">Mapa del sitio web</a></p>
<p class="apache">Versi&#243;n 2.4 del Servidor HTTP Apache</p>
<img alt="" src="./images/feather.png" /></div>
<div class="up"><a href="http://httpd.apache.org/docs-project/"><img title="&lt;-" alt="&lt;-" src="./images/left.gif" /></a></div>
<div id="path">
<a href="http://www.apache.org/">Apache</a> &gt; <a href="http://httpd.apache.org/">Servidor HTTP</a> &gt; <a href="http://httpd.apache.org/docs/">Documentaci&#243;n</a></div>
<div id="page-content"><h1>Apache HTTP Server Versi&#243;n 2.4
Documentaci&#243;n</h1>
<div class="toplang">
<p><span>Idiomas disponibles: </span><a href="./da/" hreflang="da" rel="alternate" title="Dansk">&nbsp;da&nbsp;</a> |
<a href="./de/" hreflang="de" rel="alternate" title="Deutsch">&nbsp;de&nbsp;</a> |
<a href="./en/" hreflang="en" rel="alternate" title="English">&nbsp;en&nbsp;</a> |
<a href="./es/" title="Espa&#241;ol">&nbsp;es&nbsp;</a> |
<a href="./fr/" hreflang="fr" rel="alternate" title="Fran&#231;ais">&nbsp;fr&nbsp;</a> |
<a href="./ja/" hreflang="ja" rel="alternate" title="Japanese">&nbsp;ja&nbsp;</a> |
<a href="./ko/" hreflang="ko" rel="alternate" title="Korean">&nbsp;ko&nbsp;</a> |
<a href="./pt-br/" hreflang="pt-br" rel="alternate" title="Portugu&#234;s (Brasil)">&nbsp;pt-br&nbsp;</a> |
<a href="./ru/" hreflang="ru" rel="alternate" title="Russian">&nbsp;ru&nbsp;</a> |
<a href="./tr/" hreflang="tr" rel="alternate" title="T&#252;rk&#231;e">&nbsp;tr&nbsp;</a> |
<a href="./zh-cn/" hreflang="zh-cn" rel="alternate" title="Simplified Chinese">&nbsp;zh-cn&nbsp;</a></p>
</div>
<form method="get" action="https://www.google.com/search"><p><input name="as_q" value="" type="text" /> <input value="Buscar en Google" type="submit" /><input value="10" name="num" type="hidden" /><input value="es" name="hl" type="hidden" /><input value="ISO-8859-1" name="ie" type="hidden" /><input value="Google Search" name="btnG" type="hidden" /><input name="as_epq" value="Versi&#243;n 2.4" type="hidden" /><input name="as_oq" value="" type="hidden" /><input name="as_eq" value="&quot;List-Post&quot;" type="hidden" /><input value="" name="lr" type="hidden" /><input value="i" name="as_ft" type="hidden" /><input value="" name="as_filetype" type="hidden" /><input value="all" name="as_qdr" type="hidden" /><input value="any" name="as_occt" type="hidden" /><input value="i" name="as_dt" type="hidden" /><input value="httpd.apache.org" name="as_sitesearch" type="hidden" /><input value="off" name="safe" type="hidden" /></p></form>
<table id="indextable"><tr><td class="col1"><div class="category"><h2><a name="release" id="release">Notas de la versi&#243;n</a></h2>
<ul><li><a href="new_features_2_4.html">Nuevas funcionalidades en Apache 2.3/2.4</a></li>
<li><a href="new_features_2_2.html">Nuevas funcionalidades en Apache 2.1/2.2</a></li>
<li><a href="new_features_2_0.html">Nuevas funcionalidades en Apache 2.0</a></li>
<li><a href="upgrading.html">Actualizar a la versi&#243;n 2.4 desde la 2.2</a></li>
<li><a href="license.html">Licencia Apache </a></li>
</ul>
</div><div class="category"><h2><a name="manual" id="manual">Manual de Referencia</a></h2>
<ul><li><a href="install.html">Compilar e Instalar</a></li>
<li><a href="invoking.html">Ejecutando Apache</a></li>
<li><a href="stopping.html">Parada y Reinicio de Apache</a></li>
<li><a href="mod/quickreference.html">Directivas de configuraci&#243;n en tiempo de ejecuci&#243;n</a></li>
<li><a href="mod/">M&#243;dulos</a></li>
<li><a href="mpm.html">M&#243;dulos de Procesamiento M&#250;ltiple (MPM)</a></li>
<li><a href="filter.html">Filtros</a></li>
<li><a href="handler.html">Handlers</a></li>
<li><a href="expr.html">Analizador de Expresiones</a></li>
<li><a href="mod/overrides.html">Sobreescritura de la clase &#237;ndice .htaccess</a></li>
<li><a href="programs/">Programas de Soporte y Servidor</a></li>
<li><a href="glossary.html">Glosario</a></li>
</ul>
</div></td><td><div class="category"><h2><a name="usersguide" id="usersguide">Gu&#237;a de Usuario</a></h2>
<ul><li><a href="getting-started.html">Empezando</a></li>
<li><a href="bind.html">Enlazando Direcciones y Puertos</a></li>
<li><a href="configuring.html">Ficheros de Configuraci&#243;n</a></li>
<li><a href="sections.html">Secciones de Configuraci&#243;n</a></li>
<li><a href="caching.html">Almacenamiento de Contenido en Cach&#233;</a></li>
<li><a href="content-negotiation.html">Negociaci&#243;n de Contenido</a></li>
<li><a href="dso.html">Objetos Compartidos Din&#225;micamente (DSO)</a></li>
<li><a href="env.html">Variables de Entorno</a></li>
<li><a href="logs.html">Ficheros de Log</a></li>
<li><a href="compliance.html">Cumplimiento del Protocolo HTTP</a></li>
<li><a href="urlmapping.html">Mapeo de URLs al Sistema de Ficheros</a></li>
<li><a href="misc/perf-tuning.html">Optimizaci&#243;n del Rendimiento</a></li>
<li><a href="misc/security_tips.html">Consejos de seguridad</a></li>
<li><a href="server-wide.html">Configuraci&#243;n b&#225;sica del Servidor</a></li>
<li><a href="ssl/">Cifrado SSL/TLS</a></li>
<li><a href="suexec.html">Ejecuci&#243;n de Suexec para CGI</a></li>
<li><a href="rewrite/">Reescritura de URL con mod_rewrite</a></li>
<li><a href="vhosts/">Servidores Virtuales</a></li>
</ul>
</div></td><td class="col3"><div class="category"><h2><a name="howto" id="howto">How-To / Tutoriales</a></h2>
<ul><li><a href="howto">&#205;ndice de Tutoriales </a></li>
<li><a href="howto/auth.html">Autenticaci&#243;n y Autorizaci&#243;n</a></li>
<li><a href="howto/access.html">Control de Acceso</a></li>
<li><a href="howto/cgi.html">CGI: Contenido Din&#225;mico</a></li>
<li><a href="howto/htaccess.html">Ficheros .htaccess</a></li>
<li><a href="howto/ssi.html">Inclusiones del Lado del Servidor (SSI)</a></li>
<li><a href="howto/public_html.html">Directorios Web por Usuario
    (public_html)</a></li>
<li><a href="howto/reverse_proxy.html">Gu&#237;a de configuraci&#243;n de Proxy Inverso</a></li>
<li><a href="howto/http2.html">Gu&#237;a HTTP/2 </a></li>
</ul>
</div><div class="category"><h2><a name="platform" id="platform">Notas Sobre Plataformas Espec&#237;ficas</a></h2>
<ul><li><a href="platform/windows.html">Microsoft Windows</a></li>
<li><a href="platform/rpm.html">Sistemas Basados en RPM (Redhat / CentOS / Fedora)</a></li>
<li><a href="platform/netware.html">Novell NetWare</a></li>
</ul>
</div><div class="category"><h2><a name="other" id="other">Otros Temas</a></h2>
<ul><li><a href="http://wiki.apache.org/httpd/FAQ">Preguntas Frecuentes</a></li>
<li><a href="sitemap.html">Mapa del Sitio</a></li>
<li><a href="developer/">Documentaci&#243;n para Desarrolladores</a></li>
<li><a href="http://httpd.apache.org/docs-project/">Contribuir en la Documentaci&#243;n</a></li>
<li><a href="misc/">Otras Notas</a></li>
<li><a href="http://wiki.apache.org/httpd/">Wiki</a></li>
</ul>
</div></td></tr></table></div>
<div class="bottomlang">
<p><span>Idiomas disponibles: </span><a href="./da/" hreflang="da" rel="alternate" title="Dansk">&nbsp;da&nbsp;</a> |
<a href="./de/" hreflang="de" rel="alternate" title="Deutsch">&nbsp;de&nbsp;</a> |
<a href="./en/" hreflang="en" rel="alternate" title="English">&nbsp;en&nbsp;</a> |
<a href="./es/" title="Espa&#241;ol">&nbsp;es&nbsp;</a> |
<a href="./fr/" hreflang="fr" rel="alternate" title="Fran&#231;ais">&nbsp;fr&nbsp;</a> |
<a href="./ja/" hreflang="ja" rel="alternate" title="Japanese">&nbsp;ja&nbsp;</a> |
<a href="./ko/" hreflang="ko" rel="alternate" title="Korean">&nbsp;ko&nbsp;</a> |
<a href="./pt-br/" hreflang="pt-br" rel="alternate" title="Portugu&#234;s (Brasil)">&nbsp;pt-br&nbsp;</a> |
<a href="./ru/" hreflang="ru" rel="alternate" title="Russian">&nbsp;ru&nbsp;</a> |
<a href="./tr/" hreflang="tr" rel="alternate" title="T&#252;rk&#231;e">&nbsp;tr&nbsp;</a> |
<a href="./zh-cn/" hreflang="zh-cn" rel="alternate" title="Simplified Chinese">&nbsp;zh-cn&nbsp;</a></p>
</div><div id="footer">
<p class="apache">Copyright 2024 The Apache Software Foundation.<br />Licencia bajo los t&#233;rminos de la <a href="http://www.apache.org/licenses/LICENSE-2.0">Apache License, Version 2.0</a>.</p>
<p class="menu"><a href="./mod/">M&#243;dulos</a> | <a href="./mod/directives.html">Directivas</a> | <a href="http://wiki.apache.org/httpd/FAQ">Preguntas Frecuentes</a> | <a href="./glossary.html">Glosario</a> | <a href="./sitemap.html">Mapa del sitio web</a></p></div><script type="text/javascript"><!--//--><![CDATA[//><!--
if (typeof(prettyPrint) !== 'undefined') {
    prettyPrint();
}
//--><!]]></script>
</body></html>