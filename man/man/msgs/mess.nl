BAD_CONFIG_FILE
	"De inhoud van bestand %s is onbegrijpelijk.\n"
CONFIG_OPEN_ERROR
	"Waarschuwing: kan configuratiebestand %s niet openen.\n"
PARSE_ERROR_IN_CONFIG
	"Syntaxfout in configuratiebestand.\n"
INCOMPAT
	"Opties %s en %s gaan niet samen.\n"
NO_ALTERNATE
	"Sorry, ondersteuning voor andere systemen is niet meegecompileerd.\n"
NO_COMPRESS
	"Deze 'man' is gecompileerd met automatische compressie van cat-pagina's,\n\
maar het configuratiebestand definieert COMPRESS niet.\n"
NO_NAME_FROM_SECTION
	"Welke man-pagina wilt u zien uit sectie %s?\n"
NO_NAME_NO_SECTION
	"Welke man-pagina wilt u zien?\n"
NO_SUCH_ENTRY_IN_SECTION
	"Er is geen pagina over '%s' in sectie %s.\n"
NO_SUCH_ENTRY
	"Er is geen pagina over '%s'.\n"
PAGER_IS
	"\n'%s' wordt gebruikt als om de uitvoer te tonen\n"
SYSTEM_FAILED
	"Fout tijdens opmaak- of uitvoeropdracht.\n\
De afsluitwaarde van '%s' was %d.\n"
VERSION
	"Dit is '%s', versie %s.\n\n\
Toont de handleiding ('man-pagina') van de gegeven opdrachtnaam.\n\n"
OUT_OF_MEMORY
	"Onvoldoende geheugen beschikbaar -- kan geen %d bytes reserveren\n"
ROFF_CMD_FROM_FILE_ERROR
	"Fout tijdens ontleden van een '*roff'-opdracht uit bestand '%s'.\n"
MANROFFSEQ_ERROR
	"Fout tijdens ontleden van MANROFFSEQ -- standaardvolgorde wordt gebruikt.\n"
ROFF_CMD_FROM_COMMANDLINE_ERROR
	"Fout tijdens ontleden van '*roff'-opdracht van de opdrachtregel.\n"
UNRECOGNIZED_LINE
	"Onbegrepen regel in het configuratiebestand -- wordt genegeerd\n%s\n"
GETVAL_ERROR
	"man-config.c: **interne fout**: kan tekenreeks '%s' niet vinden\n"
FOUND_MANDIR
	"man-map %s gevonden\n"
FOUND_MAP
	"man-pad-afbeelding %s --> %s gevonden\n"
FOUND_CATDIR
	"de overeenkomstige cat-map is %s\n"
LINE_TOO_LONG
	"Te lange regel in het configuratiebestand.\n"
SECTION
	"\nsectie: %s\n"
UNLINKED
	"%s is verwijderd\n"
GLOBBING
	"globben van %s\n"
EXPANSION_FAILED
	"Poging [%s] om de man-pagina te decomprimeren is mislukt.\n"
OPEN_ERROR
	"Kan man-pagina %s niet openen.\n"
READ_ERROR
	"Fout tijdens lezen van man-pagina %s.\n"
FOUND_EQN
	"eqn(1)-aanwijzing gevonden\n"
FOUND_GRAP
	"grap(1)-aanwijzing gevonden\n"
FOUND_PIC
	"pic(1)-aanwijzing gevonden\n"
FOUND_TBL
	"tbl(1)-aanwijzing gevonden\n"
FOUND_VGRIND
	"vgrind(1)-aanwijzing gevonden\n"
FOUND_REFER
	"refer(1)-aanwijzing gevonden\n"
ROFF_FROM_COMMAND_LINE
	"ontleden van aanwijzingen van de opdrachtregel\n"
ROFF_FROM_FILE
	"ontleden van aanwijzingen uit bestand %s\n"
ROFF_FROM_ENV
	"ontleden van aanwijzingen uit de omgeving\n"
USING_DEFAULT
	"de standaard preprocessor-volgorde wordt gebruikt\n"
PLEASE_WAIT
	"Opmaken van de pagina...\n"
CHANGED_MODE
	"De modus van %s is gewijzigd in %o.\n"
CAT_OPEN_ERROR
	"Kan niet schrijven naar %s.\n"
PROPOSED_CATFILE
	"indien nodig wordt bestand %s geschreven\n"
IS_NEWER_RESULT
	"afsluitwaarde van is_newer() = %d\n"
TRYING_SECTION
	"sectie %s wordt bekeken\n"
SEARCHING
	"\nzoeken in %s\n"
ALREADY_IN_MANPATH
	"maar %s zit al in het zoekpad\n"
CANNOT_STAT
	"Waarschuwing: kan status van bestand '%s' niet opvragen!\n"
IS_NO_DIR
	"Waarschuwing: '%s' is geen map!\n"
ADDING_TO_MANPATH
	"%s is toegevoegd aan het zoekpad\n"
PATH_DIR
	"\nzoekpad-map %s "
IS_IN_CONFIG
	"staat in het configuratiebestand\n"
IS_NOT_IN_CONFIG
	"staat niet in het configuratiebestand\n"
MAN_NEARBY
	"maar er is een man-map in de buurt\n"
NO_MAN_NEARBY
	"en er is geen man-map in de buurt\n"
ADDING_MANDIRS
	"\nstandaard man-mappen worden toegevoegd\n\n"
CATNAME_IS
	"cat_name in convert_to_cat() is: %s\n"
NO_EXEC
	"\nNiet-uitgevoerde opdracht:\n  %s\n"
USAGE1
	"Gebruik:  %s [-adfhktwW] [-M zoekpad] [-P viewer] [-S secties]\n\t     "
USAGE2
	" [-m systeem]"
USAGE3
	" [-p tekenreeks] [sectie] naam...\n\n"
USAGE4
	"  -a   alle overeenkomende pagina's tonen, niet slechts de eerste\n\
  -c   geen cat-bestanden gebruiken\n\
  -d   uitgebreide debug-informatie produceren\n\
  -D   als '-d', maar ook de pagina's tonen\n\
  -f   als 'whatis' fungeren\n\
  -h   deze hulptekst tonen\n\
  -k   als 'apropos' fungeren\n\
  -K   in alle man-pagina's naar een tekenreeks zoeken\n"
USAGE5
	"  -t   'troff' gebruiken om pagina's op te maken (om ze af te drukken)\n"
USAGE6
	"\
  -w   het volledige pad weergeven van de pagina die getoond zou worden\n\
       (als geen naam gegeven is, dan tonen welke mappen doorzocht worden)\n\
  -W   als '-w', maar alleen de bestandsnamen tonen, niet het volledige pad\n\n\
  -C bestand   te gebruiken configuratiebestand\n\
  -M pad       pad waarin naar man-pagina's gezocht moet worden\n\
  -P viewer    dit programma gebruiken om de uitvoer te tonen\n\
  -S secties   te doorzoeken secties (scheiden met dubbele punten)\n"
USAGE7
	"  -m systeem   naar man-pagina's van dit Unix-systeem zoeken\n"
USAGE8
	"  -p letters   uit te voeren voorverwerkingsprogramma's:\n\
                 e - [n]eqn   p - pic     t - tbl\n\
                 g - grap     r - refer   v - vgrind\n"
USER_CANNOT_OPEN_CAT
	"en de werkelijke gebruiker kan het cat-bestand ook niet openen\n"
USER_CAN_OPEN_CAT
	"maar de werkelijke gebruiker kan het cat-bestand wel openen\n"
CANNOT_FORK
	"Kan geen nieuw proces starten voor opdracht '%s'\n"
WAIT_FAILED
	"Fout tijdens wachten op dochterproces '%s'\n"
GOT_WRONG_PID
	"Vreemd..., kreeg een verkeerd PID tijdens wachten op dochterproces\n"
CHILD_TERMINATED_ABNORMALLY
	"Fatale fout: opdracht '%s' werd abnormaal beëindigd\n"
IDENTICAL
	"Man-pagina %s is identiek aan %s\n"
MAN_FOUND
	"Gevonden man-pagina('s):\n"
NO_TROFF
	"Fout: er is in %s geen TROFF-commando gegeven\n"
NO_CAT_FOR_NONSTD_LL
	"Geen cat-pagina aangemaakt vanwege de afwijkende regellengte.\n"
BROWSER_IS
	"\n'%s' wordt gebruikt als browser\n"
HTMLPAGER_IS
	"\n'%s' wordt gebruikt voor het omzetten van HTML-pagina's naar tekst\n"
FOUND_FILE
	"manfile_from_sec_and_dir() vond %s\n"
CALLTRACE1
	"manfile_from_sec_and_dir(map=%s, sectie=%s, naam=%s, vlaggen=0x%0x)\n"
CALLTRACE2
	"glob_for_file(map=%s, sectie=%s, naam=%s, type=0x%0x, ...)\n"
NO_MATCH
	"glob_for_file() heeft geen overeenkomsten gevonden\n"
GLOB_FOR_FILE
	"glob_for_file() geeft %s terug\n"
CALLTRACE3
	"glob_for_file_ext_glob(map=%s, sectie=%s, naam=%s, extensie=%s, hpx=%s, glob=%d, type=0x%0x)\n"
ABOUT_TO_GLOB
	"glob_for_file_ext_glob() zal %s expanderen\n"
