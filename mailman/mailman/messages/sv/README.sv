I allmänhet behöver du göra två saker för att lägga till översättningar 
för ett språk i Mailman. Du måste översätta meddelandekatalogen och du 
måste översätta mallarna.

För att översätta meddelandekatalogen gör du en kopia av filen 
/messages/mailman.pot. Kopian döper du till mailman.po och lägger
i underkatalogen messages/xx/LC_MESSAGES. Sen redigerar du filen och 
lägger till översättningarna för varje meddelande som finns i katalogen.
Att ha ett bra verktyg i denna del av arbetet är en god hjälp, till exempel 
po-mode for Emacs.

När du lagt till dina översättningar, kan du köra msgfmt via din .po-fil 
för att generera messages/xx/LC_MESSAGE/mailman.mo.

Nästa steg är att skapa underkatalogen templates/xx och översätta alla
filerna i templates/en/*.{html,txt}. Dessa bör du också återföra till
Mailman-projektet.

För att uppmärksamma Mailman och dina listor på det nya språket, följer du 
direktiven i avsnittet ovan.

ÖVERSÄTTNINGSTIPS
 	Fråga: Hur ska bokstäver och tecken som inte ingår i ASCII-alfabetet, 
	till exempel franskans cedilj, hanteras i kataloger och mallar?

	Svar: Alla meddelanden som är avsedda för att visas på webben
	kan innehålla en begreppshänvisning i HTML när det
	behövs. Meddelanden som är avsedda för e-post bör uttryckligen
	använda specialtecken utanför ASCII-alfabetet. Detta gäller
	för både meddelandekatalogen och mallarna.
