<?xml version="1.0" encoding="ISO-8859-1"?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="es" xml:lang="es"><head>
<meta content="text/html; charset=ISO-8859-1" http-equiv="Content-Type" />
<!--
        XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
              This file is generated from xml source: DO NOT EDIT
        XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
      -->
<title>How-To / Tutoriales - Servidor HTTP Apache Versi&#243;n 2.4</title>
<link href="../style/css/manual.css" rel="stylesheet" media="all" type="text/css" title="Main stylesheet" />
<link href="../style/css/manual-loose-100pc.css" rel="alternate stylesheet" media="all" type="text/css" title="No Sidebar - Default font size" />
<link href="../style/css/manual-print.css" rel="stylesheet" media="print" type="text/css" /><link rel="stylesheet" type="text/css" href="../style/css/prettify.css" />
<script src="../style/scripts/prettify.min.js" type="text/javascript">
</script>

<link href="../images/favicon.ico" rel="shortcut icon" /></head>
<body id="manual-page" class="no-sidebar"><div id="page-header">
<p class="menu"><a href="../mod/">M&#243;dulos</a> | <a href="../mod/directives.html">Directivas</a> | <a href="http://wiki.apache.org/httpd/FAQ">Preguntas Frecuentes</a> | <a href="../glossary.html">Glosario</a> | <a href="../sitemap.html">Mapa del sitio web</a></p>
<p class="apache">Versi&#243;n 2.4 del Servidor HTTP Apache</p>
<img alt="" src="../images/feather.png" /></div>
<div class="up"><a href="../"><img title="&lt;-" alt="&lt;-" src="../images/left.gif" /></a></div>
<div id="path">
<a href="http://www.apache.org/">Apache</a> &gt; <a href="http://httpd.apache.org/">Servidor HTTP</a> &gt; <a href="http://httpd.apache.org/docs/">Documentaci&#243;n</a> &gt; <a href="../">Versi&#243;n 2.4</a></div><div id="page-content"><div id="preamble"><h1>How-To / Tutoriales</h1>
<div class="toplang">
<p><span>Idiomas disponibles: </span><a href="../en/howto/" hreflang="en" rel="alternate" title="English">&nbsp;en&nbsp;</a> |
<a href="../es/howto/" title="Espa&#241;ol">&nbsp;es&nbsp;</a> |
<a href="../fr/howto/" hreflang="fr" rel="alternate" title="Fran&#231;ais">&nbsp;fr&nbsp;</a> |
<a href="../ja/howto/" hreflang="ja" rel="alternate" title="Japanese">&nbsp;ja&nbsp;</a> |
<a href="../ko/howto/" hreflang="ko" rel="alternate" title="Korean">&nbsp;ko&nbsp;</a> |
<a href="../zh-cn/howto/" hreflang="zh-cn" rel="alternate" title="Simplified Chinese">&nbsp;zh-cn&nbsp;</a></p>
</div>
</div>
<div class="top"><a href="#page-header"><img alt="top" src="../images/up.gif" /></a></div>
<div class="section">
<h2><a name="howto" id="howto">How-To / Tutoriales</a></h2>

    

    <dl>
      <dt>Autenticaci&#243;n y Autorizaci&#243;n</dt>
      <dd>
        <p>Autenticaci&#243;n es un proceso en el cual se verifica 
		que alguien es quien afirma ser. Autorizaci&#243;n es cualquier
		proceso en el que se permite a alguien acceder donde quiere ir,
        o a obtener la informaci&#243;n que desea tener.</p>

        <p>Ver: <a href="auth.html">Autenticaci&#243;n, Autorizaci&#243;n</a></p>
      </dd>
    </dl>

    <dl>
      <dt>Control de Acceso</dt>
      <dd>
        <p>Control de acceso hace referencia al proceso de restringir, o 
		garantizar el acceso a un recurso en base a un criterio arbitrario.
		Esto se puede conseguir de distintas formas.</p>

        <p>Ver: <a href="access.html">Control de Acceso</a></p>
      </dd>
    </dl>

   <dl>
      <dt>Contenido Din&#225;mico con CGI</dt>
      <dd>
        <p>El CGI (Common Gateway Interface) es un m&#233;todo por el cual
		un servidor web puede interactuar con programas externos de 
		generaci&#243;n de contenido, a ellos nos referimos com&#250;nmente como 
		programas CGI o scripts CGI. Es un m&#233;todo sencillo para mostrar
		contenido din&#225;mico en tu sitio web. Este documento es una 
		introducci&#243;n para configurar CGI en tu servidor web Apache, y de
		inicio para escribir programas CGI.</p>

        <p>Ver: <a href="cgi.html">CGI: Contenido Din&#225;mico</a></p>
      </dd>
    </dl>

    <dl>
      <dt>Ficheros <code>.htaccess</code></dt>
      <dd>
        <p>Los ficheros <code>.htaccess</code> facilitan una forma de 
		hacer configuraciones por-directorio. Un archivo, que 
		contiene una o m&#225;s directivas de configuraci&#243;n, se coloca en un
		directorio espec&#237;fico y las directivas especificadas solo aplican
		sobre ese directorio y los subdirectorios del mismo.</p>

        <p>Ver: <a href="htaccess.html"><code>.htaccess</code> files</a></p>
      </dd>
    </dl>

    <dl>
      <dt>HTTP/2 con httpd</dt>
      <dd>
      <p>HTTP/2 es la evoluci&#243;n del protocolo de capa de aplicaci&#243;n m&#225;s conocido, HTTP. 
        Se centra en hacer un uso m&#225;s eficiente de los recursos de red sin cambiar la
		sem&#225;ntica de HTTP. Esta gu&#237;a explica como se implementa HTTP/2 en httpd,
		mostrando buenas pr&#225;cticas y consejos de configuraci&#243;n b&#225;sica.
      </p>

        <p>Ver: <a href="http2.html">Gu&#237;a HTTP/2</a></p>
      </dd>
    </dl>


    <dl>
      <dt>Introducci&#243;n a los SSI</dt>
      <dd>
        <p>Los SSI (Server Side Includes) son directivas que se colocan
		en las p&#225;ginas HTML, y son evaluadas por el servidor mientras 
		&#233;ste las sirve. Le permiten a&#241;adir contenido generado 
		din&#225;micamente a una p&#225;gina HTML existente, sin tener que servir
		la p&#225;gina entera a trav&#233;s de un programa CGI u otro m&#233;todo 
		din&#225;mico.</p>

        <p>Ver: <a href="ssi.html">Server Side Includes (SSI)</a></p>
      </dd>
    </dl>

    <dl>
      <dt>Directorios web Por-usuario</dt>
      <dd>
        <p>En sistemas con m&#250;ltiples usuarios, cada usuario puede tener
		su directorio "home" compartido usando la directiva
		<code class="directive"><a href="../mod/mod_userdir.html#userdir">UserDir</a></code>. Aquellos
		que visiten la URL <code>http://example.com/~username/</code>
		obtendr&#225;n contenido del directorio del usuario "<code>username</code>"
		que se encuentra en el directorio "home" del sistema.</p>

        <p>Ver: <a href="public_html.html">
		Directorios Web de Usuario (<code>public_html</code>)</a></p>
      </dd>
    </dl>

    <dl>
      <dt>Gu&#237;a de Proxy Inverso</dt>
      <dd>
        <p>Apache httpd ofrece muchas posibilidades como proxy inverso. Usando la
		directiva <code class="directive"><a href="../mod/mod_proxy.html#proxypass">ProxyPass</a></code> as&#237; como
		<code class="directive"><a href="../mod/mod_proxy.html#balancermember">BalancerMember</a></code> puede crear
		sofisticadas configuraciones de proxy inverso que proveen de alta 
		disponibilidad, balanceo de carga, clustering basado en la nube y 
		reconfiguraci&#243;n din&#225;mica en caliente.</p>

        <p>Ver: <a href="reverse_proxy.html">Gu&#237;a de Proxy Inverso</a></p>
      </dd>
    </dl>

  </div></div>
<div class="bottomlang">
<p><span>Idiomas disponibles: </span><a href="../en/howto/" hreflang="en" rel="alternate" title="English">&nbsp;en&nbsp;</a> |
<a href="../es/howto/" title="Espa&#241;ol">&nbsp;es&nbsp;</a> |
<a href="../fr/howto/" hreflang="fr" rel="alternate" title="Fran&#231;ais">&nbsp;fr&nbsp;</a> |
<a href="../ja/howto/" hreflang="ja" rel="alternate" title="Japanese">&nbsp;ja&nbsp;</a> |
<a href="../ko/howto/" hreflang="ko" rel="alternate" title="Korean">&nbsp;ko&nbsp;</a> |
<a href="../zh-cn/howto/" hreflang="zh-cn" rel="alternate" title="Simplified Chinese">&nbsp;zh-cn&nbsp;</a></p>
</div><div id="footer">
<p class="apache">Copyright 2024 The Apache Software Foundation.<br />Licencia bajo los t&#233;rminos de la <a href="http://www.apache.org/licenses/LICENSE-2.0">Apache License, Version 2.0</a>.</p>
<p class="menu"><a href="../mod/">M&#243;dulos</a> | <a href="../mod/directives.html">Directivas</a> | <a href="http://wiki.apache.org/httpd/FAQ">Preguntas Frecuentes</a> | <a href="../glossary.html">Glosario</a> | <a href="../sitemap.html">Mapa del sitio web</a></p></div><script type="text/javascript"><!--//--><![CDATA[//><!--
if (typeof(prettyPrint) !== 'undefined') {
    prettyPrint();
}
//--><!]]></script>
</body></html>