<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN">
<!-- translation 1.31 -->

<html>
  <head>
    <meta name="generator" content="HTML Tidy, see www.w3.org" />
    <meta http-equiv="Content-Type"
    content="text/html; charset=iso-8859-1" />

    <title>Compilación e Instalación de Apache</title>
  </head>
<!-- Background white, links blue (unvisited), navy (visited), red (active) -->
  <BODY
   BGCOLOR="#FFFFFF"
   TEXT="#000000"
   LINK="#0000FF"
   VLINK="#000080"
   ALINK="#FF0000"
  >
<!--#include virtual="header.html" -->

    <h1 align="CENTER">Compilación e Instalación de Apache 1.3</h1>

    <p>Este documento cubre la compilación e instalación de Apache
    en sistemas Unix, usando el método manual de construcción e
    instalación. Si desea usar la interfaz estilo autoconf, deberá
    leer el fichero <code>INSTALL</code> en el directorio raíz de
    la distribución
    fuente de Apache. Para la compilación e instalación en
    plataformas específicas, consulte</p>

    <ul>
      <li><a href="windows.html">Usar Apache con Microsoft
      Windows</a></li>

      <li><a href="netware.html">Usar Apache con Novell Netware
      5</a></li>

      <li><a href="mpeix.html">Usar Apache con HP MPE/iX</a></li>

      <li><a href="unixware.html">Compilación de Apache bajo
      UnixWare</a></li>

      <li><a href="readme-tpf.html">Vistazo general de la versión
      TPF de Apache</a></li>
    </ul>

    <h2>Bajarse Apache</h2>

    <p>La información de la última versión de Apache puede
    encontrarla en <a href="http://www.apache.org/">
    http://www.apache.org/</a>. En esta web podrá encontrar las
    versiones finales, versiones beta e información de sitios y
    réplicas en la web y por ftp anónimo.</p>

    <p>Si se ha bajado la distribución binaria, vaya a <a
    href="#install">Instalación de Apache</a>. Si no es así lea la
    siguiente sección como compilar el servidor.</p>

    <h2>Compilación de Apache</h2>

    <p>La compilación de Apache consiste en tres pasos. Primero
    seleccionar qué <strong>módulos</strong> de Apache quiere
    incluir en el servidor. Segundo crear una configuración para su
    sistema operativo. Tercero compilar el ejecutable.</p>

    <p>Toda la configuración de Apache está en el directorio
    <code>src</code> de la distribución. Vaya al
    directorio <code>src</code>.</p>

    <ol>
      <li>
        <p>Seleccione módulos para compilar, en el fichero de
        <code>configuración</code> de Apache. Descomente las líneas
        correspondientes a los módulos opcionales que desee incluir
        (entre las líneas <code>AddModule</code> al final del fichero), o escriba
        nuevas líneas correspondientes a módulos adicionales que
        haya bajado o programado. (Vea <a href="misc/API.html">
        API.html</a> para ver la documentación preliminar de cómo
        escribir módulos Apache). Los usuarios avanzados pueden
        comentar los módulos por defecto si están seguros de que no
        los necesitan (tenga cuidado, ya que algunos de estos
        módulos son necesarios para el buen funcionamiento y una
        correcta seguridad del servidor).</p>

        <p>Debería leer también las instrucciones del fichero de
        <code>Configuración</code> para comprobar si necesita
        configurar unas <code>líneas</code> u otras.</p>
      </li>

      <li>
        <p>Configure Apache para su sistema operativo. Usted puede
        ejecutar un script como el mostrado más abajo.  Aunque si
        esto falla o usted tiene algún requerimiento especial
        (<i>por ejemplo</i> incluir una librería adicional exigida por
        un módulo opcional) puede editarlo para utilizar en el
        fichero de <code>Configuración</code> las siguientes
        opciones: <code>EXTRA_CFLAGS, LIBS,
        LDFLAGS,INCLUDES.</code></p>

        <p>Ejecute el script de <code>configuración</code>:</p>

        <blockquote>
<pre>
    % Configure
    Using 'Configuration' as config file
     + configured for &lt;whatever&gt; platform
     + setting C compiler to &lt;whatever&gt; *
     + setting C compiler optimization-level to &lt;whatever&gt; *
     + Adding selected modules
     + doing sanity check on compiler and options
    Creating Makefile in support
    Creating Makefile in main
    Creating Makefile in os/unix
    Creating Makefile in modules/standard
</pre>
        </blockquote>

        <p>(*: Dependiendo de la configuración y de su sistema. El
        resultado podría no coincidir con el mostrado; no hay
        problema).</p>

        <p>Esto genera un fichero <code>Makefile</code>
	 a ser usado en el tercer
        paso. También crea un <code>Makefile</code> en el
	 directorio <code>support</code>,
        para la compilación de programas de soporte.</p>

        <p>(Si quiere mantener varias configuraciones, puede
        indicarle a <code>Configure</code> una de las opciones en un
        fichero, como <code>Configure -fichero
        configuración.ai</code>).</p>
      </li>

      <li>Escriba <code>make</code>.</li>
    </ol>

    <p>Los módulos de la distribución de Apache son aquellos que
    hemos probado y utilizado regularmente varios miembros del grupo
    de desarrollo de Apache. Los módulos adicionales (creados por
    miembros del grupo o por terceras personas) para necesidades o
    funciones específicas están disponibles en &lt;<a
    href="http://www.apache.org/dist/httpd/contrib/modules/">http://www.apache.org/dist/httpd/contrib/modules/</a>&gt;.
    Hay instrucciones en esa página para añadir estos módulos en el
    núcleo de Apache.</p>

    <h2><a id="install" name="install">Instalación de
    Apache</a></h2>

    <p>Tendrá un fichero binario llamado <code>hhtpd</code> en el
    directorio <code>src</code>. Una distribución binaria de Apache
    ya traerá este fichero.</p>

    <p>El próximo paso es instalar el programa y configurarlo.
    Apache esta diseñado para ser configurado y ejecutado desde los
    directorios donde fue compilado. Si quiere ejecutarlo desde otro
    lugar, cree un directorio y copie los directorios
    <code>conf</code>, <code>logs</code> e <code>icons</code>. En
    cualquier caso debería leer las <a
    href="misc/security_tips.html#serverroot">sugerencias de
    seguridad</a> que describen cómo poner los permisos del
    directorio raíz.</p>

    <p>El paso siguiente es editar los ficheros de configuración del
    servidor. Consiste en configurar varias
    <strong>directivas</strong> en los tres ficheros
    principales. Por defecto, estos ficheros están en el directorio
    <code>conf</code> y se llaman <code>srm.conf</code>,
     <code>access.conf</code> y <code>httpd.conf</code>. Para ayudarle a
    comenzar, hay ejemplos de estos ficheros en el directorio de la
    distribución, llamados <code>srm.conf-dist</code>,
    <code>access.conf-dist</code> y <code>httpd.conf-dist</code>.
    Copie o renombre estos ficheros a los correspondientes nombres
    sin la terminación <code>-dist</code>. Edite cada uno de
    ellos. Lea los comentarios cuidadosamente. Un error en la
    configuración de estos ficheros podría provocar fallos en el
    servidor o volverlo inseguro. Tendrá también un fichero
    adicional en el directorio <code>conf</code> llamado
    <code>mime.conf</code>. Este fichero normalmente no tiene que
    ser editado.</p>

    <p>Primero edite el fichero <code>http.conf</code>. Este
    configura atributos generales del servidor: el número de puerto,
    el usuario que lo ejecuta, <i>etc.</i> El siguiente a editar
    es <code>srm.conf</code>; este fichero configura la raíz del
    árbol de los documentos, funciones especiales como HTML
    analizado sintácticamente por el servidor, mapa de imagen,
    <i>etc.</i> Finalmente, edite <code>access.conf</code> que
    configura los accesos.</p>

    <p>Además de estos tres ficheros, el comportamiento del servidor
    puede ser modificado directorio a directorio usando los ficheros
    <code>.htaccess</code> para los directorios en los que acceda el
    servidor.</p>

    <h3>¡Configure el sistema de tiempo correctamente!</h3>

    <p>Una operación de un servidor web requiere un tiempo concreto,
    ya que algunos elementos del protocolo HTTP se expresan en
    función de la hora y el día. Por eso, es hora de investigar la
    configuración de NTP o de otro sistema de sincronización de su
    Unix o lo que haga de equivalente en NT.</p>

    <h2>Programas de soporte para la compilación</h2>

    <p>Además del servidor principal <code>httpd</code> que se
    compila y configura como hemos visto, Apache incluye programas
    de soporte. Estos no son compilados por defecto. Los programas
    de soporte están en el directorio <code>support</code>. Para
    compilar esos programas, entre en el directorio indicado y
    ejecute el comando:</p>
        <blockquote>
<pre>
    make
</pre>
        </blockquote>
    <!--#include virtual="footer.html" -->
  </body>
</html>


