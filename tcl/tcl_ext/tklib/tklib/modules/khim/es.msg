# es.msg --
#
#	Spanish-language messages for KHIM
#
# Copyright (c) 2008 by Emiliano Gavilán.
# Proofreading and corrections by Miguel Sofer.
#
# Refer to the file "license.terms" for the terms and conditions of
# use and redistribution of this file, and a DISCLAIMER OF ALL WARRANTEES.
#
# $Id: es.msg,v 1.3 2008/05/30 02:00:19 kennykb Exp $
# $Source: /cvsroot/tcllib/tklib/modules/khim/es.msg,v $
#
#----------------------------------------------------------------------

namespace eval ::khim {

    ::msgcat::mcset es HELPTEXT {

	Kevin's Hacky Input Method (KHIM)

	KHIM permite ingresar caracteres internacionales desde un teclado
	que no soporta esta funcionalidad. Funciona independientemente de
	cualquier método de entrada que su sistema operativo pueda proveer;
	su finalidad es permitirle ingresar caracteres en otros lenguajes,
	aun cuando no tenga control del mapeo de su teclado.

	Para usar KHIM, seleccione el diálogo de control de KHIM
	(la forma de lograr esto depende de su aplicación) y habilite
	el uso de KHIM seleccionando "Usar KHIM". También necesitará
	seleccionar una tecla que sea raramente usada y designarla como
	tecla "Componer" presionando el botón con la leyenda "Tecla Componer:"
	y luego la tecla que quiere asignar a esta función. Ésta tecla no
	debe ser la tecla designada como la tecla de composición de su
	teclado; dicha tecla seguirá invocando cualquier método de entrada
	que su sistema operativo provea.

	Una vez que KHIM esté habilitado, podrá ingresar caracteres
	internacionales en cualquier widget que este configurado para
	usar KHIM presionando la tecla designada como Componer seguida
	de una secuencia de dos teclas. La lista en el control de KHIM
	muestra todas las secuencias disponibles. Además, si presiona
	la tecla "Componer" dos veces, se mostrará un diálogo que le
	permitirá ingresar cualquier carácter arbitrario desde un mapa
	de caracteres Unicode. Dicho mapa puede navegarse utilizando
	el ratón o las teclas de dirección, y se puede seleccionar el
	carácter deseado con un doble click, la barra espaciadora o la
	tecla Return (Enter).

	Para definir una nueva secuencia para utilizar con la tecla
	"Componer", seleccione el control de KHIM, ingrese dos	teclas en
	secuencia en la entrada "Secuencia de teclas", el carácter deseado
	en la entrada "Carácter", y luego presione la tecla "Cambiar".
	(Usted puede copiar y pegar dicho carácter desde otra aplicación,
	o presionar el botón "Unicode..." (o presione la tecla "Componer"
	dos veces) para seleccionar el carácter desde el mapa de los
	caracteres Unicode disponibles). Para borrar una secuencia,
	selecciónela de la lista y presione "Borrar".

    }

    ::msgcat::mcset es {SELECT COMPOSE KEY} [string map [list \n\t \n] {
	Por favor presione 
	la tecla que desee
	usar como tecla
	"Componer".
    }]

    ::msgcat::mcset es {Apply} {Aplicar}

    ::msgcat::mcset es {Cancel} {Cancelar}

    ::msgcat::mcset es {Change} {Cambiar}

    ::msgcat::mcset es {Character} {Carácter}

    ::msgcat::mcset es {Compose Key} {Tecla Componer}

    ::msgcat::mcset es {Compose key:} {Tecla Componer:}

    ::msgcat::mcset es {Composed sequence must be two characters long} \
	{La secuencia de composición debe ser de dos teclas}

    ::msgcat::mcset es {Delete} {Borrar}

    ::msgcat::mcset es {KHIM Help} {Ayuda de KHIM}

    ::msgcat::mcset es {Help...} {Ayuda...}

    ::msgcat::mcset es {Input key sequence} {Secuencia de teclas de entrada}

    ::msgcat::mcset es {Insert Character} {Insertar carácter}

    ::msgcat::mcset es {Invalid sequence} {Secuencia inválida}

    ::msgcat::mcset es {Key sequences} {Secuencias de teclas}

    ::msgcat::mcset es {KHIM Controls} {Controles de KHIM}

    ::msgcat::mcset es {OK} {Aceptar}

    ::msgcat::mcset es {Select code page:} {Seleccionar página de código:}

    ::msgcat::mcset es {Unicode...} {Unicode...}

    ::msgcat::mcset es {Use KHIM} {Usar KHIM}

}