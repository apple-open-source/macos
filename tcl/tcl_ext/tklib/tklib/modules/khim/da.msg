# da.msg --
#
#	Danish-language messages for KHIM
#
# Copyright (c) 2005 by Kevin B. Kenny.  All rights reserved.
# Translation by Torsten Berg
#
# Refer to the file "license.terms" for the terms and conditions of
# use and redistribution of this file, and a DISCLAIMER OF ALL WARRANTEES.
#
# $Id: da.msg,v 1.1 2006/09/05 13:48:49 kennykb Exp $
# $Source: /cvsroot/tcllib/tklib/modules/khim/da.msg,v $
#
#----------------------------------------------------------------------

namespace eval ::khim {

    ::msgcat::mcset da {Apply} {Anvend}

    ::msgcat::mcset da {Cancel} {Annuller}

    ::msgcat::mcset da {Change} {Ændre}

    ::msgcat::mcset da {Character} {Tegn}

    ::msgcat::mcset da {Compose Key} {Compose taste}

    ::msgcat::mcset da {Compose key:} {Compose taste:}

    ::msgcat::mcset da {Composed sequence must be two characters long} \
	{Compose tegnfølgen skal bestå af to tegn}

    ::msgcat::mcset da {Delete} {Slet}

    ::msgcat::mcset da {Help...} {Hjælp...}

    ::msgcat::mcset da HELPTEXT {

	Kevin's Hacky Input Method (KHIM)

	KHIM gør det muligt at indtaste internationale tegn med et tastatur
	som ikke understøtter disse. Dette fungerer uafhængigt af en
	bestående indtast metode som styresystemet måtte understøtte.
	Det er tænkt til at hjælpe, hvis du ikke har kontrol over tilordningen
	på dit tastatur og dog har brug for at indtaste tekst i andre sprog.
	
	For at benytte KHIM, vis kontrolpanelet (hvordan det gøres, afhænger af
	dit program) og aktiver KHIM ved at afkrydse "Benyt KHIM". Du skal vælge
	en taste	der kun sjældent benyttes på dit tastatur og fastlægge denne som
	"Compose" taste for at sætte tegn sammen med. Tryk dertil på knappen
	markeret med "Compose taste:" og tryk derefter på den taste du ønsker at
	fastlægge. Generellt skulle det ikke være den taste som normalt benyttes
	til at konstruere tegn med; denne taste vil fortsat opråbe din lokale
	systems indtast metode.

	Når KHIM er aktiveret kan du indtaste internationale tegn i enhver
	kontrol der er konfigureret til at bruge KHIM ved at trykke på den valgte
	"Compose" taste fulgt af to bestemte tegn. Rullelisten til venstre
	på KHIM kontrolpanelet viser de tegnfølger der står til rådighed. Hvis
	du trykker "Compose" tasten to gange kommer der et vindue frem hvor
	du kan vælge vilkårlige symboler fra en unicode tabel. Du kan navigere
	rundt i selve tabellen ved enten at benytte markøren eller markørtasterne.
	Du kan udvælge det markerede tegn ved at doppelt-klikke på symbolet eller
	ved at trykke på mellemrums-, enter- eller returtasten.

	Ny tegnfølger kan defineres ved at indtaste en følge af to tegn i feltet
	markeret med "Indtast tegnfølge" og det ønskede symbol i feltet markeret
	med "Tegn" og derefter trykke på "Ændre". Du kan også kopiere og indsætte
	et symbol fra et andet program eller benytte "Unicode..." knappen
	(eller trykke "Compose" tasten to gange) for at hente et symbol fra
	tabellen med alle unicode koder. Tryk på "Slet" knappen for at fjerne
	en tegnfølge.

    }

    ::msgcat::mcset da {Input key sequence} {Indtast tegnfølge}

    ::msgcat::mcset da {Insert Character} {Indtast et tegn}

    ::msgcat::mcset da {Invalid sequence} {Ugyldig tegnfølge}

    ::msgcat::mcset da {Key sequences} {Tegnfølger}

    ::msgcat::mcset da {KHIM Controls} {KHIM kontrolpanel}

    ::msgcat::mcset da {OK} {O.k.}

    ::msgcat::mcset da {Select code page:} {Vælg kode side:}

    ::msgcat::mcset da {SELECT COMPOSE KEY} [string map [list \n\t \n] {
	Tryk på den taske
	du ønsker at bruge  
	som "Compose" taste.
    }]

    ::msgcat::mcset da {Unicode...} {Unicode...}

    ::msgcat::mcset da {Use KHIM} {Benyt KHIM}

}

# Local Variables:
# mode: tcl
# End:
