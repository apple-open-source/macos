Pasos a dar para soportar un nuevo idioma
-----------------------------------------
Supongamos que vamos a soportar el idioma idioma Portugués (pt)
- Traducir las plantillas de $prefix/templates/en/*, aunque le resulte más útil traducir $prefix/templates/es/* dada la similitud de los idiomas.
- Seleccionar los ficheros que tienen cadenas a traducir, es decir, aquellos que tienen _("...") en el código fuente.
   $ find $prefix -exec grep -l "_(" {} \; > $prefix/messages/pygettext.files
- Quitar todos los ficheros de pygettext.files que no correspondan, *.pyc *.py~...
- Generar el catálogo, para ello se debe ejecutar:
   $ cd $prefix/messages
   $ $prefix/bin/pygettext.py -v `cat pygettext.files`
   $ mkdir -p pt/LC_MESSAGES
   #
   # No sería mala idea (en este caso) traducir README.es a README.pt :-)
   #
   $ mv messages.pot pt/LC_MESSAGES/catalog.pt
- traducir catalog.pt
- Generar mailman.mo:
   $ cd $prefix/messages/pt/LC_MESSAGES
   $ msgfmt -o mailman.mo catalog.pt
- Insertar en Defaults.py una línea en la variable LC_DESCRIPTIONS:
LC_DESCRIPTIONS = { 'es':     [_("Spanish (Spain)"),  'iso-8859-1'],
		    'pt':     [_("Portuguese"),       'iso-8859-1'], <----
                    'en':     [_("English (USA)"),    'us-ascii']
		   }
- Almacenar las plantillas del nuevo idioma en $prefix/templates/pt
- A partir de ahora podemos añadir a una lista el nuevo idioma:
   $ $prefix/bin/addlang -l <lista> pt


Pasos para sincronizar el catálogo
----------------------------------
- Generar el nuevo catálogo tal y como se describe antes y compararlo con el
que ya tenemos. Para compararlo tendremos que ejecutar:
   $ cd $prefix/messages
   $ $prefix/bin/pygettext.py -v `cat pygettext.files`
   $ mv messages.pot pt/LC_MESSAGES
   $ cd pt/LC_MESSAGES
   # Hay otra utilidad relacionada que hace los mismo: 'msgmerge'
   $ tupdate messages.pot catalog.pt > tmp
# Los mensajes antiguos quedan comentados al final del fichero tmp
# Los mensajes nuevos quedan sin traducir.
   $ vi tmp
# Traducir los mensajes nuevos
   $ mv tmp catalog.pt; rm messages.pot
   $ msgfmt -o mailman.mo catalog.pt

Para donar la traducción de un nuevo idioma
-------------------------------------------
      Apreciamos la donación de cualquier traducción al proyecto mailman,
      de manera que cualquiera pueda beneficiarse de tu esfuerzo. Por
      supuesto, cualquier labor realizada será reconocida públicamente,
      dentro de la documentación de Mailman. Esto es lo que hay que hacer
      para donar cualquier traducción, ya sea la primera vez que se haga o
      cualquier actualización posterior.

      Lo mejor que se puede hacer es mandar un fichero en formato 'tar' a
      <barry@zope.com> que se pueda desempaquetar en la parte superior 
      donde empieza la jerarquía de directorios del CVS.

      Tu fichero 'tar' debería tener dos directorios, donde están contenidos
      los ficheros pertenecientes a la traducción del lenguaje 'xx':
 
      templates/xx
      messages/xx
 
      En templates/xx deberían estar las plantillas, todos los ficheros .txt y
      .html traducidas en tu idioma, a partir de las plantillas en Inglés (que
      siempre son las copias primarias).

      En messages/xx solo debería haber un único directorio llamado
      LC_MESSAGES y dentro de él un fichero llamado mailman.po, que es el
      catálogo perteneciente a tu idioma. No envíes el fichero mailman.mo
      porque de eso me encargo yo.

      Prácticamente eso es todo. Si necesitas incluir un fichero README, por
      favor nómbralo como README.xx y mételo en el directorio messages/xx.
      README.xx debería estar en tu idioma.

      Puedes mandarme el fichero 'tar' por correo electrónico. Si es la
      primera vez que mandas la traducción, por favor, dime que debo poner en
      la invocación de add_language() dentro del fichero Defaults.py para
      incorporar tu idioma.
