# de.msg --
#
#	German-language messages for KHIM
#
# Copyright (c) 2006 by Andreas Kupries.  All rights reserved.
#
# Refer to the file "license.terms" for the terms and conditions of
# use and redistribution of this file, and a DISCLAIMER OF ALL WARRANTEES.
#
# $Id: de.msg,v 1.2 2006/09/07 13:18:13 kennykb Exp $
# $Source: /cvsroot/tcllib/tklib/modules/khim/de.msg,v $
#
#----------------------------------------------------------------------

namespace eval ::khim {

    ::msgcat::mcset de HELPTEXT {

	Kevin's Hacky Input Method (KHIM)

	KHIM ermöglicht die Eingabe von internationalen Zeichen
	mittels einer Tastatur, welches diese nicht direkt
	unterstützt. Es ist unabhängig von allen Eingabemethoden,
	welche vom Betriebssystem bereitgestellt werden (könnten),
	und für den Fall gedacht, wenn der Anwender keine Kontrolle
	über die Tastatur hat und dennoch Text in anderen Sprachen
	eingeben muß.

	Um KHIM zu benutzen, ist es notwendig, den KHIM Steuer-Dialog zu
	öffnen (dies ist abhängig von der Anwendung) und dann KHIM
	durch 'ticken' der 'Benutze KHIM'-Checkbox zu aktivieren. Es
	ist weiterhin notwendig, eine Taste als die Kombinier-Taste zu
	wählen. Die gewählte Taste sollte im Normalgebrauch selten
	genutzt werden. Die Auswahl selbst besteht aus zwei Schritten.
	Zuerst muß der Knopf "Kombinier-Taste:" gedrückt werden,
	dann die gewünschte Taste.

	Allgemein gesprochen, wenn die benutzte Tastatur eine Taste
	"Compose" besitzt, dann sollte diese _nicht_ als die
	Kombinier-Taste für KHIM gewählt werden. Dies stellt sicher,
	das diese Taste weiterhin vom Betriebssystem genutzt werden
	kann, um dessen eventuelle Eingabemethoden zu aktivieren.

	Sobald KHIM aktiviert wurde, können in jedem Widget, welches
	für die Benutzung von KHIM konfiguriert wurde, internationale
	Zeichen eingegeben werden. Dies geschieht durch Drücken der
	gewählten Kombinier-Taste, gefolgt von zwei weiteren Zeichen,
	welche das gewünschte Zeichen identifizieren. Der KHIM
	Steuer-Dialog stellt eine Liste der bekannten Zeichenfolgen
	zur Verfügung. Zusätzlich ist es möglich einen Dialog zu
	öffnen, welcher die Auswahl beliebiger Zeichen in einer Tabelle
	erlaubt. Dies geschieht durch zweimaliges Drücken der
	Kombinier-Taste. Navigation in der Tabelle geschieht mit der
	Maus oder den Kursor-Tasten. Das einzufügende Zeichen kann
	mit Doppel-Klick gewählt werden, durch Drücken der
	Leer-Taste, oder durch Drücken der Enter- (oder Return-)Taste.

	Es ist auch möglich, die Liste der direkt anwählbären Zeichen
	zu erweitern. Dies geschieht im KHIM Steuer-Dialog durch Eingabe
	der zwei Zeichen für den Kode im Eingabefeld
	"Eingabezeichenfolge", des gewünschten Zeichens im Feld
	"Zeichen", gefolgt vom Drücken des Knopfes "Ändern".

	(Bezüglich der Herkunft des gewünschten Zeichens: Es kann aus
	einer anderen Anwendung kopiert werden, oder man benutze den Knopf
	"Unicode..." (oder drücke die Kombinier-Taste zweimal), um es
	aus der Tabelle aller Zeichen auszuwählen.)

	Eine Zeichenfolge wird gelöscht durch Auswahl der Folge in
	der Liste aller Zeichenfolgen, gefolgt von der
	Lösch/Entferne-Taste.
    }

    ::msgcat::mcset de {SELECT COMPOSE KEY} [string map [list \n\t \n] {
	Bitte drücken Sie
	die Taste, welche Sie
	als Kombinier-Taste
	verwenden wollen.
    }]

    ::msgcat::mcset de {Apply} {Anwenden}

    ::msgcat::mcset de {Cancel} {Abbrechen}

    ::msgcat::mcset de {Change} {Ändern}

    ::msgcat::mcset de {Character} {Zeichen}

    ::msgcat::mcset de {Compose Key} {Kombinier-Taste}

    ::msgcat::mcset de {Compose key:} {Kombinier-Taste:}

    ::msgcat::mcset de {Composed sequence must be two characters long} \
	{Die Eingabezeichenfolge muß aus zwei Zeichen bestehen}

    ::msgcat::mcset de {Delete} {Löschen}

    ::msgcat::mcset de {Help...} {Hilfe...}

    ::msgcat::mcset de {Input key sequence} {Eingabezeichenfolge}

    ::msgcat::mcset de {Insert Character} {Zeichen einfügen}

    ::msgcat::mcset de {Invalid sequence} {Ungültige Zeichenfolge}

    ::msgcat::mcset de {Key sequences} {Zeichenfolgen}

    ::msgcat::mcset de {KHIM Controls} {KHIM Steuerung}

    ::msgcat::mcset de {OK} {OK}

    ::msgcat::mcset de {Select code page:} {Wähle Code-Seite:}

    ::msgcat::mcset de {Unicode...} {Unicode...}

    ::msgcat::mcset de {Use KHIM} {Benutze KHIM}

}

# Local Variables:
# mode: tcl
# End:

