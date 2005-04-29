BAD_CONFIG_FILE
	"er staat onzin in het bestand %s\n"
CONFIG_OPEN_ERROR
	"Waarschuwing: ik kan mijn configuratiebestand %s niet openen.\n"
PARSE_ERROR_IN_CONFIG
	"Fout bij het interpreteren van het configuratiebestand.\n"
INCOMPAT
	"de vlaggen %s en %s zijn onverenigbaar.\n"
NO_ALTERNATE
	"Jammer - er is geen ondersteuning voor andere systemen meevertaald."
NO_COMPRESS
	"Bij het vertalen van man was gevraagd om automatische compressie\n\
van cat pagina's, maar het configuratiebestand definieert COMPRESS niet.\n"
NO_NAME_FROM_SECTION
	"Welke handboek blz wil je uit sectie %s?\n"
NO_NAME_NO_SECTION
	"Welke handboek blz wilt u hebben?\n"
NO_SUCH_ENTRY_IN_SECTION
	"Ik kan geen blz over %s in sectie %s van het handboek vinden.\n"
NO_SUCH_ENTRY
	"Ik heb niets over %s, geloof ik.\n"
PAGER_IS
	"\nIk zal %s als pagineer programma gebruiken.\n"
SYSTEM_FAILED
	"Fout bij het uitvoeren van de formatteer- of vertoonopdracht.\n\
De exit status van %s was %d.\n"
VERSION
	"%s, versie %s\n\n"
OUT_OF_MEMORY
	"Geen geheugen meer - kon van malloc geen %d bytes krijgen\n"
ROFF_CMD_FROM_FILE_ERROR
	"Ontleed fout in het *roff commando uit bestand %s\n"
MANROFFSEQ_ERROR
	"Ontleed fout bij het lezen van MANROFFSEQ.  Ik zal de standaard volgorde gebruiken.\n"
ROFF_CMD_FROM_COMMANDLINE_ERROR
	"Ontleed fout in het *roff commando op de opxdrachtregel.\n"
UNRECOGNIZED_LINE
	"Onbegrepen regel in het configuratiebestand (genegeerd)\n%s\n"
GETVAL_ERROR
	"man-config.c: interne fout: ik kan de string %s niet vinden\n"
FOUND_MANDIR
	"man directory %s gevonden\n"
FOUND_MAP
	"manpad afbeelding %s --> %s gevonden\n"
FOUND_CATDIR
	"het overeenkomstige catdir is %s\n"
LINE_TOO_LONG
	"Te lange regel in het configuratiebestand\n"
SECTION
	"\nhoofdstuk: %s\n"
UNLINKED
	"%s verwijderd\n"
GLOBBING
	"glob %s\n"
EXPANSION_FAILED
	"Poging [%s] om de man bladzijde te decomprimeren faalde\n"
OPEN_ERROR
	"Ik kan de man bladzijde %s niet openen\n"
READ_ERROR
	"Fout bij het lezen van de man bladzijde %s\n"
FOUND_EQN
	"eqn(1) aanwijzing gevonden\n"
FOUND_GRAP
	"grap(1) aanwijzing gevonden\n"
FOUND_PIC
	"pic(1) aanwijzing gevonden\n"
FOUND_TBL
	"tbl(1) aanwijzing gevonden\n"
FOUND_VGRIND
	"vgrind(1) aanwijzing gevonden\n"
FOUND_REFER
	"refer(1) aanwijzing gevonden\n"
ROFF_FROM_COMMAND_LINE
	"ontleed aanwijzingen van de opdrachtregel\n"
ROFF_FROM_FILE
	"ontleed aanwijzingen uit het bestand %s\n"
ROFF_FROM_ENV
	"ontleed aanwijzingen uit de omgeving\n"
USING_DEFAULT
	"gebruik de standaard preprocessor volgorde\n"
PLEASE_WAIT
	"Ik ben de bladzijde aan het formatteren, even geduld a.u.b...\n"
CHANGED_MODE
	"De mode van %s is gewijzigd in %o\n"
CAT_OPEN_ERROR
	"Kon niet schrijven in %s.\n"
PROPOSED_CATFILE
	"Ik zal indien nodig in het bestand %s schrijven\n"
IS_NEWER_RESULT
	"status van is_newer() = %d\n"
TRYING_SECTION
	"probeer hoofdstuk %s\n"
SEARCHING
	"\nzoek in %s\n"
ALREADY_IN_MANPATH
	"maar %s zit al in het zoekpad\n"
CANNOT_STAT
	"Waarschuwing: stat %s faalt!\n"
IS_NO_DIR
	"Waarschuwing: %s is geen directory!\n"
ADDING_TO_MANPATH
	"voeg %s toe aan het zoekpad\n"
PATH_DIR
	"\npad directory %s "
IS_IN_CONFIG
	"is in het configuratiebestand\n"
IS_NOT_IN_CONFIG
	"is niet in het configuratiebestand\n"
MAN_NEARBY
	"maar er is een man directory in de buurt\n"
NO_MAN_NEARBY
	"en ik zie geen man directory in de buurt\n"
ADDING_MANDIRS
	"\nvoeg de standaard man directories toe\n\n"
CATNAME_IS
	"cat_name in convert_to_cat () is: %s\n"
NO_EXEC
	"\nIk zou de volgende opdracht uitvoeren:\n  %s\n"
USAGE1
	"aanroep: %s [-adfhktwW] [hoofdstuk] [-M zoekpad] [-P prog] [-S hoofdstukken]\n\t"
USAGE2
	"[-m systeem] "
USAGE3
	"[-p string] naam ...\n\n"
USAGE4
	"  a : vind alle passende bladzijden\n\
  c : (her)formatteer, zelfs als een recente geformatteerde versie bestaat\n\
  d : druk af wat man aan het doen is - om fouten op te sporen\n\
  D : idem, maar formatteer en vertoon ook de bladzijden\n\
  f : hetzelfde als whatis(1)\n\
  h : druk deze hulpboodschap af\n\
  k : hetzelfde als apropos(1)\n\
  K : zoek een woord in alle pagina's\n"
USAGE5
	"  t : gebruik troff om bladzijden te formatteren (om ze af te drukken)\n"
USAGE6
	"\
  w : uit welke bestanden komen de bladzijde(n) die vertoond zouden worden?\n\
      (als geen naam is opgegeven: druk af in welke directories wordt gezocht)\n\
  W : idem, maar geef alleen de padnamen\n\n\
  C bestand: gebruik `bestand' als configuratiebestand\n\
  M pad    : geef een expliciet zoekpad voor man bladzijden\n\
  P prog   : gebruik het programma `prog' als pagineerprogramma\n\
  S lijst  : lijst met door dubbele punten gescheiden hoofdstuknamen\n"
USAGE7
	"  m systeem: zoek man bladzijden van een ander systeem\n"
USAGE8
	"  p string : vertel welke preprocessoren de tekst moeten voorbewerken\n\
               e - [n]eqn(1)   p - pic(1)    t - tbl(1)\n\
               g - grap(1)     r - refer(1)  v - vgrind(1)\n"
USER_CANNOT_OPEN_CAT
	"en de gebruiker kan het cat bestand ook niet openen\n"
USER_CAN_OPEN_CAT
	"maar de gebruiker kan het cat bestand openen\n"
CANNOT_FORK
	"ik kon het commando _%s_ niet starten\n"
WAIT_FAILED
	"fout bij het wachten op mijn kind _%s_\n"
GOT_WRONG_PID
	"vreemd ..., ik wachtte op mijn kind maar kreeg een ander terug\n"
CHILD_TERMINATED_ABNORMALLY
	"fatale fout: het commando _%s_ werd abnormaal beëindigd\n"
IDENTICAL
        "Man bladzijde %s is hetzelfde als %s\n"
MAN_FOUND
	"ik heb de volgende bladzijde(n) gevonden:\n"
NO_TROFF
	"fout: er is geen TROFF commando in %s gespecificeerd\n"
NO_CAT_FOR_NONSTD_LL
	"ik maak geen cat pagina aan, vanwege de afwijkende lijnlengte\n"
