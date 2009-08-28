# pl.msg --
#
#	Polish-language messages for KHIM
# Contributed by Irek Chmielowiec <irek.ch (at) gmail.com>
# Copyright (c) 2005 by Kevin B. Kenny.  All rights reserved.
#
# Refer to the file "license.terms" for the terms and conditions of
# use and redistribution of this file, and a DISCLAIMER OF ALL WARRANTEES.
#
#----------------------------------------------------------------------

namespace eval ::khim {

    # If you edit HELPTEXT or {SELECT COMPOSE KEY}, also edit the corresponding
    # messages in ROOT.msg

    ::msgcat::mcset pl HELPTEXT {

	Kevin's Hacky Input Method (KHIM)

	KHIM pozwala na wprowadzanie znaków narodowych i symboli z
	klawiatury która tego nie obsługuje. Działa niezależnie od
	metody wprowadzania znaków jaką może oferować system operacyjny;
	jest przewidziany do sytuacji kiedy nie ma się kontroli nad
	odwzorowaniem klawiszy a istnieje potrzeba wprowadzania tekstu w
	różnych językach.

	Aby móc używać KHIM, uruchom okno Ustawień KHIM (sposób jego
	uruchomienia zależy od używanej aplikacji) i włącz KHIM
	zaznaczając opcję "Włącz KHIM". Należy także wybrać klawisz
	który jest rzadko używany i przypisać mu funkcję klawisza
	"sterującego" naciskając przycisk "Klawisz sterujący:",
	a następnie naciskając klawisz któremu chcemy tę funkcję
	przypisać. Mówiąc ogólnie, nie powinien to być klawisz
	ustawiony jako sterujący dla używanego układu klawiatury (np.
	Alt); klawisz ten będzie działał niezależnie od metody
	wprowadzania znaków oferowanej przez lokalny system operacyjny.

	Kiedy KHIM jest już uruchomiony, można wprowadzać znaki narodowe
	i symbole w każdym elemencie interfejsu użytkownika, który
	został ustawiony do korzystania z KHIM, naciskając klawisz
	sterujący razem z sekwencją dwóch znaków. Lista wyboru w
	ustawieniach KHIM pokazuje dostępne kombinacje. Dodatkowo, po
	dwukrotnym naciśnięciu klawisza sterującego pojawi się okno
	pozwalające na bezpośrednie wstawienie wybranego znaku z tablicy
	symboli Unicode. W oknie tablicy można nawigować pomiędzy
	znakami używając klawiszy kursorów lub za pomocą myszy a wybór
	znaku następuje przez dwukrotne kliknięcie, naciśnięcie spacji
	lub naciśnięcie klawisza Enter (Return).

	Aby zdefiniować nową sekwencję do użycia z klawiszem sterującym
	należy uruchomić ustawienia KHIM, wprowadzić dwa znaki w polu 
	"Sekwencja znaków" i znak jaki chcemy uzyskać w polu "Wynik"
	oraz nacisnąć przycisk "Zmień". (Można także skopiować i
	wkleić znak z innej aplikacji lub użyć przycisku "Tablica
	Unicode..." (także przez dwukrotne naciśnięcie klawisza
	sterującego) do wyboru znaku z tablicy wszystkich dostępnych
	kodów Unicode.) Aby usunąć sekwencję znaków, należy wybrać ją z
	listy i nacisnąć "Usuń".

    }

    ::msgcat::mcset pl {SELECT COMPOSE KEY} [string map [list \n\t \n] {
	Proszę nacisnąć klawisz 
	który ma być używany 
	jako sterujący.
    }]

    ::msgcat::mcset pl {Apply} {Zastosuj}

    ::msgcat::mcset pl {Cancel} {Anuluj}

    ::msgcat::mcset pl {Change} {Zmień}

    ::msgcat::mcset pl {Character} {Wynik}

    ::msgcat::mcset pl {Compose Key} {Klawisz sterujący}

    ::msgcat::mcset pl {Compose key:} {Klawisz sterujący:}

    ::msgcat::mcset pl {Composed sequence must be two characters long} \
	{Sekwecja znaków musi być dwuelementowa}

    ::msgcat::mcset pl {Delete} {Usuń}

    ::msgcat::mcset pl {KHIM Help} {Pomoc KHIM}

    ::msgcat::mcset pl {Help...} {Pomoc...}

    ::msgcat::mcset pl {Input key sequence} {Sekwencja znaków}

    ::msgcat::mcset pl {Insert Character} {Wstaw znak}

    ::msgcat::mcset pl {Invalid sequence} {Nieprawidłowa sekwencja}

    ::msgcat::mcset pl {Key sequences} {Sekwencje znaków}

    ::msgcat::mcset pl {KHIM Controls} {Ustawienia KHIM}

    ::msgcat::mcset pl {OK} {OK}

    ::msgcat::mcset pl {Select code page:} {Wybierz kodowanie:}

    ::msgcat::mcset pl {Unicode...} {Tablica Unicode...}

    ::msgcat::mcset pl {Use KHIM} {Włącz KHIM}

}

# vim:ft=tcl:ts=8:sw=4:sts=4:noet
# Local Variables:
# mode: tcl
# End:
