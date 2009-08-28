# ru.msg --
#
#	Russian-language messages for KHIM
#
# Contributed by Konstantin Khomoutov <flatworm@users.sourceforge.net>.
# Proof-read and edited by Serge Yudin <talking_zero@mail.ru>.
#
# Copyright (c) 2005 by Kevin B. Kenny.  All rights reserved.
#
# Refer to the file "license.terms" for the terms and conditions of
# use and redistribution of this file, and a DISCLAIMER OF ALL WARRANTEES.
#
# $Id: ru.msg,v 1.1 2007/06/08 19:24:31 kennykb Exp $
# $Source: /cvsroot/tcllib/tklib/modules/khim/ru.msg,v $
#
#----------------------------------------------------------------------

namespace eval ::khim {

    # If you edit HELPTEXT or {SELECT COMPOSE KEY}, also edit the corresponding
    # messages in ROOT.msg

    ::msgcat::mcset ru HELPTEXT {

	Kevin's Hacky Input Method (KHIM) --
	Нетривиальный Метод Кевина для Ввода Символов

	KHIM делает возможным ввод символов национальных алфавитов
	с клавиатуры, которая не позволяют этого делать. Он работает
	независимо от любых способов ввода, которые поддерживает ОС;
	его задача -- позволить осуществлять ввод таких символов,
	которые невозможно ввести с текущими настройками ОС данного
	компьютера, а менять их нет возможности или желания.

	Для того, чтобы использовать KHIM, следует вызвать диалог
	"Настройки KHIM" (как это сделать, зависит от приложения,
	использующего KHIM) и разрешить работу KHIM, включив переключатель
	"Использовать KHIM". Также Вам понадобится выбрать редко
	используемую клавишу на Вашей клавиатуре и назначить её
	"клавишей композиции". Для этого нужно нажать кнопку, подписанную
	"Клавиша композиции:", в диалоге настроек KHIM и затем нажать
	выбранную клавишу на клавиатуре. Имейте в виду, что если на Вашей
	клавиатуре есть клавиша, называемая "Compose", то её не следует
	выбирать в качестве клавиши композиции для KHIM -- пусть она
	продолжает вызывать тот метод ввода,
	который назначен ей операционной системой.

	После того как KHIM активирован, Вы можете вставлять символы
	национальных алфавитов в любое поле ввода, настроенное на
	использование KHIM, таким образом: нажать и отпустить клавишу
	композиции, затем ввести два символа -- "входную последовательность".
	В диалоге настроек KHIM есть окно со списком доступных
	последовательностей. Кроме того, двойное нажатие клавиши
	композиции вызывает окно диалога, позволяющее выбрать произвольный
	символ Unicode. Выбор нужного символа в этом диалоге
	осуществляется мышью либо клавишами управления курсором,
	а вставка выбранного символа -- двойным щелчком левой кнопки
	мыши на нём, либо нажатием клавиши Пробел или Ввод.

	Чтобы создать новую входную последовательность для использования
	совместно с клавишей композиции окройте диалог настроек KHIM,
	введите два символа в поле "Входная последовательность" и
	соответствующий ей символ в поле "Символ", затем нажмите
	кнопку "Изменить". (Вы можете копировать и вставлять целевой
	символ из другого приложения. Также можно воспользоваться
	встроенным в KHIM диалогом выбора символов Unicode, нажав
	кнопку "Unicode..." или дважды нажав клавишу композиции.)
	Для удаления входной последовательности выберите её в списке
	и дажмите "Удалить".

    }

    ::msgcat::mcset ru {SELECT COMPOSE KEY} [string map [list \n\t \n] {
	Нажмите клавишу,
	которую вы хотите
	использовать в качестве
	"Клавиши композиции"
    }]

    ::msgcat::mcset ru {Apply} {Применить}

    ::msgcat::mcset ru {Cancel} {Отменить}

    ::msgcat::mcset ru {Change} {Изменить}

    ::msgcat::mcset ru {Character} {Символ}

    ::msgcat::mcset ru {Compose Key} {Клавиша композиции}

    ::msgcat::mcset ru {Compose key:} {Клавиша композиции:}

    ::msgcat::mcset ru {Composed sequence must be two characters long} \
	{Входная последовательность должна состоять из двух символов}

    ::msgcat::mcset ru {Delete} {Удалить}

    ::msgcat::mcset ru {KHIM Help} {Справка по KHIM}

    ::msgcat::mcset ru {Help...} {Справка...}

    ::msgcat::mcset ru {Input key sequence} {Входная последовательность}

    ::msgcat::mcset ru {Insert Character} {Выберите символ}

    ::msgcat::mcset ru {Invalid sequence} {Неправильная комбинация}

    ::msgcat::mcset ru {Key sequences} {Комбинации клавиш}

    ::msgcat::mcset ru {KHIM Controls} {Настройки KHIM}

    ::msgcat::mcset ru {OK} {OK}

    ::msgcat::mcset ru {Select code page:} {Выберите страницу кодов:}

    ::msgcat::mcset ru {Unicode...} {Unicode...}

    ::msgcat::mcset ru {Use KHIM} {Использовать KHIM}

}

# vim:ft=tcl:ts=8:sw=4:sts=4:noet
# Local Variables:
# mode: tcl
# End:
