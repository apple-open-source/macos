# cs.msg --
#
#	Czech-language messages for KHIM
#
# Copyright (c) 2005 by Kevin B. Kenny.  All rights reserved.
# Translation by Michal Mestan <mestan@dix.cz>
#
# Refer to the file "license.terms" for the terms and conditions of
# use and redistribution of this file, and a DISCLAIMER OF ALL WARRANTEES.
#
# $Id: cs.msg,v 1.1 2006/12/06 17:28:12 kennykb Exp $
# $Source: /cvsroot/tcllib/tklib/modules/khim/cs.msg,v $
#
#----------------------------------------------------------------------

namespace eval ::khim {

    ::msgcat::mcset cs HELPTEXT {

	Kevin's Hacky Input Method (KHIM)

	KHIM vám umožňuje zadávat mezinárodní znaky včetně těch,
	které na vaší klávesnici nejspou. Pracuje nezávisle na
	požité vstupní metodě kterou váš operační systém může
	poskytovat. Je hlavně určen pro případy, kdy nemáte možnost
	měnit mapování klávesnice a přesto potřebujete zapsat
	cizojazyčný text.

	K nastavení KHIM slouží "Ovládací panel KHIM" (jeho vyvolání
	je závaislé na konkrétní aplikaci), zde povolte KHIM zaškrtnutím
	políčka "Používat KHIM". Mužete si zde též vybrat klávesu na
	vaší klávesnici, kterou bude KHIM používat jako "mrtvá"
	stisknutím talčítka "Mrtvá klávesa" a poté stisktnutím
	patřičné klávesy. Obecně lze říci, nemůže to být jakákoliv
	"mrtvá" klávasa vaší klávesové mapy, jelikož při stisknutí
	je volána vstupní metoda operačního systému.

	Když je KHIM povolen, můžete vkládat mezinárodní znaky
	v jakémkoliv přípravku, který je nastaven tak, aby používal
	KHIM stiknutním "mrtvé" klávesy následované posloupností
	dvou znaků. Seznam v "Ovládacím panelu KHIM" zobrazuje
	dostupné posloupnosti. Pokud stisknete "mrtvou" klávesu
	dvakrát, zobrazí se vám dialog, ve kterém můžete vybrat
	žádaný symbol z unokódové mapy. V mapě se můžete pohybovat
	kursorovými klávesami, dvojklikem myší, mezerníkem či klávesou
	enter vybraný znak vložíte do textu.

	Chcete-li další znaky vkládat pomocí mrtvé klávesy, otevřete
	"Ovládací panel KHIM" vložte dva znaky do pole "Posloupnost 
	kláves" a požadovaný znak, který chcete vložit do pole 
	"Vkládaný znak" a stisknětě talčítko "Změnit" (vkládaný
	znak můžete nakopírovat z jiné aplikace nebo poučít talčítko
	"Unikód..." (nebo stisknout dvakrát "mrtvou" klávesu) a vybrat
	znak z unikódové mapy). K odstranění klávesové posloupnosti
	slouží tlačítko "Smazat".

    }

    ::msgcat::mcset cs {SELECT COMPOSE KEY} [string map [list \n\t \n] {
	Stiskněte klávesu 
	kterou chcete používat 
	jako "mrtvou" klávesu.
    }]

    ::msgcat::mcset cs {Apply} "Použít"

    ::msgcat::mcset cs {Cancel} "Zrušit"

    ::msgcat::mcset cs {Change} "Změnit"

    ::msgcat::mcset cs {Character} "Znak"

    ::msgcat::mcset cs {Compose Key} "Mrtvá klávesa"

    ::msgcat::mcset cs {Compose key:} "Mrtvá klávesa:"

    ::msgcat::mcset cs {Composed sequence must be two characters long} \
	"Vkládaný znak je vždy šložen z posloupnosti zdvou nzaků"

    ::msgcat::mcset cs {Delete} "Smazat"

    ::msgcat::mcset cs {Help...} "Nápověda..."

    ::msgcat::mcset cs {Input key sequence} "Zadajete posloupnost kláves"

    ::msgcat::mcset cs {Insert Character} "Vkládaný znak"

    ::msgcat::mcset cs {Invalid sequence} "Chybná posloupnost"

    ::msgcat::mcset cs {Key sequences} "Posloupnosti kláves"

    ::msgcat::mcset cs {KHIM Controls} "Ovládací panel KHIM"

    ::msgcat::mcset cs {OK} {OK}

    ::msgcat::mcset cs {Select code page:} "Výběr kódové stránky:"

    ::msgcat::mcset cs {Unicode...} "Unikód..."

    ::msgcat::mcset cs {Use KHIM} "Používat KHIM"

}

# Local Variables:
# mode: tcl
# End:


