/*
   +----------------------------------------------------------------------+
   | PHP version 4.0                                                      |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997, 1998, 1999, 2000 The PHP Group                   |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Frank M. Kromann    <fmk@swwwing.com>                       |
   |          Daniel Beulshausen  <daniel@php4win.de>                     |
   +----------------------------------------------------------------------+
 */

/* $Id: printer.c,v 1.1.1.1 2001/01/25 04:59:54 wsanchez Exp $ */

#include "php.h"
#include "php_ini.h"
#include "ext/standard/php_string.h"
#include "ext/standard/info.h"
#include "ext/standard/php_math.h"

#ifdef HAVE_PRINTER

#include "php_printer.h"

#ifdef ZTS
int printer_globals_id;
#else
php_printer_globals printer_globals;
#endif

static int printer_id, brush_id, pen_id, font_id;

#ifdef PHP_WIN32
/* windows headers */
#include <windows.h>
#include <wingdi.h>
#include <winspool.h>

/* some protos */
char *get_default_printer(void);
COLORREF hex_to_rgb(char * hex);
int get_pen_style(char *style);
int get_hatch_style(char *hatch);
static void destroy_ressources(zend_rsrc_list_entry *resource);
DWORD _print_enumvalue(char * EnumStr);


function_entry printer_functions[] = {
	PHP_FE(printer_open,    NULL)
	PHP_FE(printer_close,    NULL)
	PHP_FE(printer_write,    NULL)
	PHP_FE(printer_name,    NULL)
	PHP_FE(printer_list,    NULL)
	PHP_FE(printer_set_option,    NULL)
	PHP_FE(printer_get_option,    NULL)
	PHP_FE(printer_create_dc,    NULL)
	PHP_FE(printer_delete_dc,    NULL)
	PHP_FE(printer_start_doc,    NULL)
	PHP_FE(printer_end_doc,    NULL)
	PHP_FE(printer_start_page,    NULL)
	PHP_FE(printer_end_page,    NULL)
	PHP_FE(printer_create_pen,    NULL)
	PHP_FE(printer_delete_pen,    NULL)
	PHP_FE(printer_select_pen,    NULL)
	PHP_FE(printer_create_brush,    NULL)
	PHP_FE(printer_delete_brush,    NULL)
	PHP_FE(printer_select_brush,    NULL)
	PHP_FE(printer_logical_fontheight,    NULL)
	PHP_FE(printer_draw_roundrect,    NULL)
	PHP_FE(printer_draw_rectangle,    NULL)
	PHP_FE(printer_draw_text,    NULL)
	PHP_FE(printer_draw_elipse,    NULL)
	PHP_FE(printer_create_font,    NULL)
	PHP_FE(printer_delete_font,    NULL)
	PHP_FE(printer_select_font,    NULL)
	{NULL, NULL, NULL}
};

zend_module_entry printer_module_entry = {
	"printer",
	printer_functions,
	PHP_MINIT(printer),
	NULL,
	NULL,
	NULL,
	PHP_MINFO(printer),
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_PRINTER
ZEND_GET_MODULE(printer)
#endif


PHP_MINFO_FUNCTION(printer)
{
	char *default_printer = get_default_printer();
	php_info_print_table_start();
	php_info_print_table_row(2, "Printer Support", "enabled");
	php_info_print_table_row(2, "Default printing device", default_printer ? default_printer : "<b>not detected</b>");
	php_info_print_table_row(2, "Module state", "experimental");
	php_info_print_table_row(2, "RCS Version", "$Id: printer.c,v 1.1.1.1 2001/01/25 04:59:54 wsanchez Exp $");
	php_info_print_table_end();
}

PHP_MINIT_FUNCTION(printer)
{
	printer_id = zend_register_list_destructors_ex(destroy_ressources, NULL, "printer", module_number);
	pen_id = zend_register_list_destructors_ex(destroy_ressources, NULL, "printer pen", module_number);
	font_id = zend_register_list_destructors_ex(destroy_ressources, NULL, "printer font", module_number);
	brush_id = zend_register_list_destructors_ex(destroy_ressources, NULL, "printer brush", module_number);
	return SUCCESS;
}


/* {{{ proto int proto(string printername)
   Return a handle or false if connection failed */
PHP_FUNCTION(printer_open)
{
	pval **arg1;
	printer *resource;

	PRINTERLS_FETCH();

	resource = (printer *)emalloc(sizeof(printer));

	/* use the given printer */
	if( zend_get_parameters_ex(1, &arg1) != FAILURE ) {
      	convert_to_string_ex(arg1);
      	resource->name = (*arg1)->value.str.val;
	}
	/* use detected default */
	else {
		resource->name = get_default_printer();
	}


	if( OpenPrinter(resource->name, &resource->handle, NULL) != 0 ) {
		resource->device = emalloc(DocumentProperties(NULL, NULL, resource->name, NULL, NULL, 0));
		DocumentProperties(NULL, resource->handle, resource->name, resource->device, NULL, DM_OUT_BUFFER);

		/* set DOCINFO defaults */
		resource->info.lpszDocName = "PHP generated Document";
		resource->info.lpszOutput = NULL;
		resource->info.lpszDatatype = "TEXT";
		resource->info.fwType = 0;
		resource->info.cbSize = sizeof(resource->info);
		resource->dc = CreateDC(NULL, resource->name, NULL, resource->device);
		ZEND_REGISTER_RESOURCE(return_value, resource, PRINTERG(printer_id));
	}
	else {
		php_error(E_WARNING,"couldn't connect to the printer [%s]", resource->name);
		RETURN_FALSE;
	}
}
/* }}} */


/* {{{ proto bool proto(int handle)
   Close the printer connection */
PHP_FUNCTION(printer_close)
{
	pval **arg1;
	printer *resource;

	PRINTERLS_FETCH();

	if ( zend_get_parameters_ex(1, &arg1) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(resource, printer *, arg1, -1, "Printer Handle", PRINTERG(printer_id));

	if( ClosePrinter(resource->handle) != 0 ) {
		RETURN_TRUE;
	}
	else {
		php_error(E_WARNING,"the printer connection to [%s] couldn't be closed", resource->name);
		RETURN_FALSE;
	}
}
/* }}} */


/* {{{ proto bool printer_write(int handle,string content)
   Write directly to the printer */
PHP_FUNCTION(printer_write)
{
	pval **arg1, **arg2;
	printer *resource;
	LPDWORD recieved = NULL;
	int sd, sp = 0;

	PRINTERLS_FETCH();

	if ( zend_get_parameters_ex(2, &arg1, &arg2) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}


	ZEND_FETCH_RESOURCE(resource, printer *, arg1, -1, "Printer Handle", PRINTERG(printer_id));
	convert_to_string_ex(arg2);
	resource->data = estrdup((*arg2)->value.str.val);

	/* reflect DOCINFO to DOC_INFO_1 */
	resource->info1.pDocName = estrdup(resource->info.lpszDocName);
	resource->info1.pDatatype = estrdup(resource->info.lpszDatatype);
	resource->info1.pOutputFile = NULL; 

	sd = StartDocPrinter(resource->handle, 1, (LPSTR)&resource->info1);
	sp = StartPagePrinter(resource->handle);

	if( sd && sp ) {
		WritePrinter(resource->handle, resource->data, strlen(resource->data), (unsigned long*)&recieved);
		EndPagePrinter(resource->handle);
		EndDocPrinter(resource->handle);
		RETURN_TRUE;
	}
	else {
		php_error(E_WARNING,"couldn't allocate the printerjob" );
		RETURN_FALSE;
	}
}
/* }}} */


/* {{{ proto string printer_name(int handle)
   Return the printer name */
PHP_FUNCTION(printer_name)
{
	pval **arg1;
	printer *resource;

	PRINTERLS_FETCH();

	if ( zend_get_parameters_ex(1, &arg1) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(resource, printer *, arg1, -1, "Printer Handle", PRINTERG(printer_id));
	RETURN_STRING(resource->name,1);
}
/* }}} */


/* {{{ proto array printer_list(string EnumType [, string Name [, int Level]])
   Return an array of printers attached to the server */
PHP_FUNCTION(printer_list)
{
	zval *Printer, *Printer_ptr, **arg1, **arg2, **arg3;
	char InfoBuffer[8192];
	LPTSTR Name;
	PRINTER_INFO_1 *P1;
	PRINTER_INFO_2 *P2;
	PRINTER_INFO_4 *P4;
	PRINTER_INFO_5 *P5;
	DWORD bNeeded = sizeof(InfoBuffer), cReturned, i, EnumVal;
	int argc = ZEND_NUM_ARGS(), Level=1;
	int LevvelsAllowed[] = {0, 1, 1, 0, 1, 1};

	if(zend_get_parameters_ex(argc, &arg1, &arg2, &arg3) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_string_ex(arg1);
	EnumVal = _print_enumvalue((*arg1)->value.str.val);
	if (argc > 1) {
		convert_to_string_ex(arg2);
		Name = (*arg2)->value.str.val;
		if (strlen(Name) == 0)
			Name = NULL;
	}
	if (argc > 2) {
		convert_to_long_ex(arg3);
		Level = (int)(*arg3)->value.lval;
		if (!LevvelsAllowed[Level]) {
			RETURN_FALSE;
		}
	}

	EnumPrinters(EnumVal, Name, Level, (LPBYTE)InfoBuffer, sizeof(InfoBuffer), &bNeeded, &cReturned);

	P1 = (PRINTER_INFO_1 *)InfoBuffer;
	P2 = (PRINTER_INFO_2 *)InfoBuffer;
	P4 = (PRINTER_INFO_4 *)InfoBuffer;
	P5 = (PRINTER_INFO_5 *)InfoBuffer;

	if (array_init(return_value) == FAILURE) {
		return;
	}
	for (i = 0; i < cReturned; i++) {
		Printer = (pval *) emalloc(sizeof(pval));
		array_init(Printer);
		switch (Level) {
			case 1:
				add_assoc_string(Printer, "NAME", P1->pName, 1);
				add_assoc_string(Printer, "DESCRIPTION", P1->pDescription, 1);
				add_assoc_string(Printer, "COMMENT", P1->pComment, 1);
				P1++;
				break;
			case 2:
				if (P2->pServerName) add_assoc_string(Printer, "SERVERNAME", P2->pServerName, 1);
				if (P2->pPrinterName) add_assoc_string(Printer, "PRINTERNAME", P2->pPrinterName, 1);
				if (P2->pShareName) add_assoc_string(Printer, "SHARENAME", P2->pShareName, 1);
				if (P2->pPortName) add_assoc_string(Printer, "PORTNAME", P2->pPortName, 1);
				if (P2->pDriverName) add_assoc_string(Printer, "DRIVERNAME", P2->pDriverName, 1);
				if (P2->pComment) add_assoc_string(Printer, "COMMENT", P2->pComment, 1);
				if (P2->pLocation) add_assoc_string(Printer, "LOCATION", P2->pLocation, 1);
/*
				add_assoc_string(Printer, "DEVMODE", P2->pDevMode, 1);
*/
				if (P2->pSepFile) add_assoc_string(Printer, "SEPFILE", P2->pSepFile, 1);
				if (P2->pPrintProcessor) add_assoc_string(Printer, "PRINTPROCESSOR", P2->pPrintProcessor, 1);
				if (P2->pDatatype) add_assoc_string(Printer, "DATATYPE", P2->pDatatype, 1);
				if (P2->pParameters) add_assoc_string(Printer, "PARAMETRES", P2->pParameters, 1);
/*
				add_assoc_string(Printer, "SECURITYDESCRIPTOR", P2->pSecurityDescriptor, 1);
*/
				add_assoc_long(Printer, "ATTRIBUTES", P2->Attributes);
				add_assoc_long(Printer, "PRIORITY", P2->Priority);
				add_assoc_long(Printer, "DEFAULTPRIORITY", P2->DefaultPriority);
				add_assoc_long(Printer, "STARTTIME", P2->StartTime);
				add_assoc_long(Printer, "UNTILTIME", P2->UntilTime);
				add_assoc_long(Printer, "STATUS", P2->Status);
				add_assoc_long(Printer, "CJOBS", P2->cJobs);
				add_assoc_long(Printer, "AVERAGEPPM", P2->AveragePPM);
				P2++;
				break;
			case 4:
				add_assoc_string(Printer, "PRINTERNAME", P4->pPrinterName, 1);
				add_assoc_string(Printer, "SERVERNAME", P4->pServerName, 1);
				add_assoc_long(Printer, "ATTRIBUTES", P4->Attributes);
				P4++;
				break;
			case 5:
				add_assoc_string(Printer, "PRINTERNAME", P5->pPrinterName, 1);
				add_assoc_string(Printer, "PORTNAME", P5->pPortName, 1);
				add_assoc_long(Printer, "ATTRIBUTES", P5->Attributes);
				add_assoc_long(Printer, "DEVICENOTSELECTEDTIMEOUT", P5->DeviceNotSelectedTimeout);
				add_assoc_long(Printer, "TRANSMISSIONRETRYTIMEOUT", P5->TransmissionRetryTimeout);
				P5++;
				break;
		}
		zend_hash_index_update(return_value->value.ht, i, (void *) &Printer, sizeof(zval *), (void **) &Printer_ptr);
	}
}
/* }}} */


/* {{{ proto bool printer_set_option(int handle,string option,string value)
   Configure the printer device */
PHP_FUNCTION(printer_set_option)
{
	pval **arg1, **arg2, **arg3;
	printer *resource;

	PRINTERLS_FETCH();

	if( zend_get_parameters_ex(3, &arg1, &arg2, &arg3) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(resource, printer *, arg1, -1, "Printer Handle", PRINTERG(printer_id));
	convert_to_string_ex(arg2);

	if(!strcmp((*arg2)->value.str.val,"copies")) {
		convert_to_long_ex(arg3);
		resource->device->dmCopies = (short)(*arg3)->value.lval;
	}
	else if(!strcmp((*arg2)->value.str.val,"mode")) {
		convert_to_string_ex(arg3);
		resource->info.lpszDatatype = estrdup((*arg3)->value.str.val);
	}	
	else if(!strcmp((*arg2)->value.str.val,"title")) {
		convert_to_string_ex(arg3);
		resource->info.lpszDocName = estrdup((*arg3)->value.str.val);
	}	
	else if(!strcmp((*arg2)->value.str.val,"orientation")) {
		convert_to_long_ex(arg3);
		resource->device->dmOrientation = (short)(*arg3)->value.lval;
	}	
	else if(!strcmp((*arg2)->value.str.val,"resolution")) {
		convert_to_long_ex(arg3);
		resource->device->dmYResolution = (short)(*arg3)->value.lval;
	}	
	else if(!strcmp((*arg2)->value.str.val,"papersize")) {
		convert_to_long_ex(arg3);
		resource->device->dmPaperSize = (short)(*arg3)->value.lval;
	}	
	else if(!strcmp((*arg2)->value.str.val,"paperlength")) {
		convert_to_long_ex(arg3);
		resource->device->dmPaperLength = (*arg3)->value.lval * 254;
	}	
	else if(!strcmp((*arg2)->value.str.val,"paperwidth")) {
		convert_to_long_ex(arg3);
		resource->device->dmPaperWidth = (*arg3)->value.lval * 254;
	}	
	else if(!strcmp((*arg2)->value.str.val,"scale")) {
		convert_to_long_ex(arg3);
		resource->device->dmScale = (short)(*arg3)->value.lval;
	}
	else if(!strcmp((*arg2)->value.str.val,"bgcolor")) {
		convert_to_string_ex(arg3);
		SetBkColor(resource->dc, hex_to_rgb((*arg3)->value.str.val));
	}
	else if(!strcmp((*arg2)->value.str.val,"textalign")) {
		convert_to_long_ex(arg3);
		SetTextAlign(resource->dc, (*arg3)->value.lval);
	}
	else {
		php_error(E_WARNING,"unknown option passed to printer_set_option()");
		RETURN_FALSE;
	};

	RETURN_TRUE;
}
/* }}} */


/* {{{ proto mixed printer_get_option(int handle,string option)
   Get configured data */
PHP_FUNCTION(printer_get_option)
{
	pval **arg1, **arg2;
	printer *resource;

	PRINTERLS_FETCH();

	if( zend_get_parameters_ex(2, &arg1, &arg2) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(resource, printer *, arg1, -1, "Printer Handle", PRINTERG(printer_id));
	convert_to_string_ex(arg2);

	if(!strcmp((*arg2)->value.str.val,"copies")) {
		RETURN_LONG(resource->device->dmCopies);
	}	
	else if(!strcmp((*arg2)->value.str.val,"mode")) {
		RETURN_STRING((char*)resource->info.lpszDatatype,1);
	}	
	else if(!strcmp((*arg2)->value.str.val,"title")) {
		RETURN_STRING((char*)resource->info.lpszDocName,1);
	}	
	else if(!strcmp((*arg2)->value.str.val,"orientation")) {
		RETURN_LONG(resource->device->dmOrientation);
	}	
	else if(!strcmp((*arg2)->value.str.val,"resolution")) {
		RETURN_LONG(resource->device->dmYResolution);
	}	
	else if(!strcmp((*arg2)->value.str.val,"papersize")) {
		RETURN_LONG(resource->device->dmPaperSize);
	}	
	else if(!strcmp((*arg2)->value.str.val,"paperlength")) {
		RETURN_LONG(resource->device->dmPaperLength / 245);
	}	
	else if(!strcmp((*arg2)->value.str.val,"paperwidth")){
		RETURN_LONG(resource->device->dmPaperWidth / 245);
	}	
	else if(!strcmp((*arg2)->value.str.val,"driverversion")) {
		RETURN_LONG(resource->device->dmDriverVersion);
	}	
	else if(!strcmp((*arg2)->value.str.val,"specversion")) {
		RETURN_LONG(resource->device->dmSpecVersion);
	}	
	else if(!strcmp((*arg2)->value.str.val,"scale")) {
		RETURN_LONG(resource->device->dmScale);
	}	
	else if(!strcmp((*arg2)->value.str.val,"devicename")) {
		RETURN_STRING(resource->name,1);
	}
	else {
		php_error(E_WARNING,"unknown option passed to printer_get_option()");
		RETURN_FALSE;
	}	
}
/* }}} */


/* {{{ proto void printer_create_dc(int handle)
   Create a device content */
PHP_FUNCTION(printer_create_dc)
{
	pval **arg1;
	printer *resource;

	PRINTERLS_FETCH();

	if( zend_get_parameters_ex(1, &arg1) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(resource, printer *, arg1, -1, "Printer Handle", PRINTERG(printer_id));

	if( resource->dc != NULL ) {
		php_error(E_NOTICE,"Deleting old DeviceContext");
		DeleteDC(resource->dc);
	}
	
	resource->dc = CreateDC(NULL, resource->name, NULL, resource->device);
}
/* }}} */


/* {{{ proto bool printer_delete_dc(int handle)
   Delete a device content */
PHP_FUNCTION(printer_delete_dc)
{
	pval **arg1;
	printer *resource;

	PRINTERLS_FETCH();

	if( zend_get_parameters_ex(1, &arg1) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(resource, printer *, arg1, -1, "Printer Handle", PRINTERG(printer_id));

	if( resource->dc != NULL ) {
		DeleteDC(resource->dc);
		RETURN_TRUE;
	}
	else {
		php_error(E_WARNING,"No DeviceContext created");
		RETURN_FALSE;
	}
}
/* }}} */


/* {{{ proto bool printer_start_doc(int handle)
   Start a document */
PHP_FUNCTION(printer_start_doc)
{
	pval **arg1;
	printer *resource;

	PRINTERLS_FETCH();

	if( zend_get_parameters_ex(1, &arg1) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(resource, printer *, arg1, -1, "Printer Handle", PRINTERG(printer_id));

	if(StartDoc(resource->dc, &resource->info) < 0) {
		php_error(E_ERROR,"couldn't allocate new print job");
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */


/* {{{ proto bool printer_end_doc(int handle)
   End a document */
PHP_FUNCTION(printer_end_doc)
{
	pval **arg1;
	printer *resource;

	PRINTERLS_FETCH();

	if( zend_get_parameters_ex(1, &arg1) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(resource, printer *, arg1, -1, "Printer Handle", PRINTERG(printer_id));

	if(EndDoc(resource->dc) < 0) {
		php_error(E_ERROR,"couldn't terminate print job");
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */


/* {{{ proto bool printer_start_page(int handle)
   Start a page */
PHP_FUNCTION(printer_start_page)
{
	pval **arg1;
	printer *resource;

	PRINTERLS_FETCH();

	if( zend_get_parameters_ex(1, &arg1) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(resource, printer *, arg1, -1, "Printer Handle", PRINTERG(printer_id));

	if(StartPage(resource->dc) < 0) {
		php_error(E_ERROR,"couldn't start a new page");
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */


/* {{{ proto bool printer_end_page(int handle)
   End a page */
PHP_FUNCTION(printer_end_page)
{
	pval **arg1;
	printer *resource;

	PRINTERLS_FETCH();

	if( zend_get_parameters_ex(1, &arg1) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(resource, printer *, arg1, -1, "Printer Handle", PRINTERG(printer_id));

	if(EndPage(resource->dc) < 0) {
		php_error(E_ERROR,"couldn't end the page");
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */


/* {{{ proto int printer_create_pen(string style, int width,string color)
   Create a pen */
PHP_FUNCTION(printer_create_pen)
{
	pval **arg1, **arg2, **arg3;
	pen_struct *pen = emalloc(sizeof(pen_struct));

	PRINTERLS_FETCH();

	if( zend_get_parameters_ex(3, &arg1, &arg2, &arg3) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}

	convert_to_string_ex(arg1);
	convert_to_string_ex(arg3);
	convert_to_long_ex(arg2);

	pen->pointer = CreatePen(get_pen_style((*arg1)->value.str.val), (*arg2)->value.lval, hex_to_rgb((*arg3)->value.str.val));

	ZEND_REGISTER_RESOURCE(return_value, pen, PRINTERG(pen_id));
}
/* }}} */


/* {{{ proto void printer_delete_pen(int handle)
   Delete a pen */
PHP_FUNCTION(printer_delete_pen)
{
	pval **arg1;
	pen_struct *pen;

	PRINTERLS_FETCH();

	if( zend_get_parameters_ex(1, &arg1) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(pen, pen_struct *, arg1, -1, "Pen Handle", PRINTERG(pen_id));

	DeleteObject(pen->pointer);
}
/* }}} */


/* {{{ proto void printer_select_pen(int printerhandle, int penhandle)
   Select a pen */
PHP_FUNCTION(printer_select_pen)
{
	pval **arg1, **arg2;
	pen_struct *pen;
	printer *resource;

	PRINTERLS_FETCH();

	if( zend_get_parameters_ex(2, &arg1, &arg2) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(resource, printer *, arg1, -1, "Printer Handle", PRINTERG(printer_id));
	ZEND_FETCH_RESOURCE(pen, pen_struct *, arg1, -1, "Pen Handle", PRINTERG(pen_id));

	SelectObject(resource->dc, pen->pointer);
}
/* }}} */


/* {{{ proto int printer_create_brush(int handle)
   Create a brush */
PHP_FUNCTION(printer_create_brush)
{
	pval **arg1, **arg2;
	brush_struct *brush = emalloc(sizeof(brush_struct));

	PRINTERLS_FETCH();

	if( zend_get_parameters_ex(2, &arg1, &arg2) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}

	convert_to_string_ex(arg1);
	convert_to_string_ex(arg2);

	if(!strcmp((*arg1)->value.str.val,"solid")) {
		brush->pointer = CreateSolidBrush(hex_to_rgb((*arg2)->value.str.val));
	}
	else {
		brush->pointer = CreateHatchBrush(hex_to_rgb((*arg2)->value.str.val), get_hatch_style((*arg1)->value.str.val));
	}

	ZEND_REGISTER_RESOURCE(return_value, brush, PRINTERG(brush_id));
}
/* }}} */


/* {{{ proto void printer_delete_brush(int handle)
   Delete a brush */
PHP_FUNCTION(printer_delete_brush)
{
	pval **arg1;
	brush_struct *brush;
	
	PRINTERLS_FETCH();

	if( zend_get_parameters_ex(1, &arg1) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(brush, brush_struct *, arg1, -1, "Brush Handle", PRINTERG(brush_id));

	DeleteObject(brush->pointer);
}
/* }}} */


/* {{{ proto int printer_select_brush(int handle)
   Select a brush */
PHP_FUNCTION(printer_select_brush)
{
	pval **arg1, **arg2;
	brush_struct *brush;
	printer *resource;

	PRINTERLS_FETCH();

	if( zend_get_parameters_ex(2, &arg1, &arg2) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(resource, printer *, arg1, -1, "Printer Handle", PRINTERG(printer_id));
	ZEND_FETCH_RESOURCE(brush, brush_struct *, arg2, -1, "Brush Handle", PRINTERG(brush_id));

	SelectObject(resource->dc, brush->pointer);
}
/* }}} */


/* {{{ proto int printer_logical_fontheight(int handle, int)
   Get the logical font height */
PHP_FUNCTION(printer_logical_fontheight)
{
	pval **arg1, **arg2;
	printer *resource;

	PRINTERLS_FETCH();

	if( zend_get_parameters_ex(2, &arg1, &arg2) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(resource, printer *, arg1, -1, "Printer Handle", PRINTERG(printer_id));

	convert_to_long_ex(arg2);

	RETURN_LONG(MulDiv((*arg2)->value.lval, GetDeviceCaps(resource->dc, LOGPIXELSY), 72));
}
/* }}} */


/* {{{ proto void printer_draw_roundrect()
   Draw a roundrect */
PHP_FUNCTION(printer_draw_roundrect)
{
	pval **arg1, **arg2, **arg3, **arg4, **arg5, **arg6, **arg7; 
	printer *resource;

	PRINTERLS_FETCH();
	
	if( zend_get_parameters_ex(7, &arg1, &arg2, &arg3, &arg4, &arg5, &arg6, &arg7) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(resource, printer *, arg1, -1, "Printer Handle", PRINTERG(printer_id));

	convert_to_long_ex(arg2);
	convert_to_long_ex(arg3);
	convert_to_long_ex(arg4);
	convert_to_long_ex(arg5);
	convert_to_long_ex(arg6);
	convert_to_long_ex(arg7);

	RoundRect(resource->dc, (*arg2)->value.lval, (*arg3)->value.lval, (*arg4)->value.lval, (*arg5)->value.lval, (*arg6)->value.lval, (*arg7)->value.lval);
}
/* }}} */


/* {{{ proto void printer_draw_rectangle()
   Draw a rectangle */
PHP_FUNCTION(printer_draw_rectangle)
{
	pval **arg1, **arg2, **arg3, **arg4, **arg5; 
	printer *resource;

	PRINTERLS_FETCH();

	if( zend_get_parameters_ex(5, &arg1, &arg2, &arg3, &arg4, &arg5) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(resource, printer *, arg1, -1, "Printer Handle", PRINTERG(printer_id));

	convert_to_long_ex(arg2);
	convert_to_long_ex(arg3);
	convert_to_long_ex(arg4);
	convert_to_long_ex(arg5);

	Rectangle(resource->dc, (*arg2)->value.lval, (*arg3)->value.lval, (*arg4)->value.lval, (*arg5)->value.lval);
}
/* }}} */


/* {{{ proto void printer_draw_elipse(int left, int top, int right, int bottom)
   Draw an elipse */
PHP_FUNCTION(printer_draw_elipse)
{
	pval **arg1, **arg2, **arg3, **arg4, **arg5; 
	printer *resource;

	PRINTERLS_FETCH();

	if( zend_get_parameters_ex(5, &arg1, &arg2, &arg3, &arg4, &arg5) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(resource, printer *, arg1, -1, "Printer Handle", PRINTERG(printer_id));

	convert_to_long_ex(arg2);
	convert_to_long_ex(arg3);
	convert_to_long_ex(arg4);
	convert_to_long_ex(arg5);

	Ellipse(resource->dc, (*arg2)->value.lval, (*arg3)->value.lval, (*arg4)->value.lval, (*arg5)->value.lval);
}
/* }}} */


/* {{{ proto void printer_draw_text()
   Draw text */
PHP_FUNCTION(printer_draw_text)
{
	pval **arg1, **arg2, **arg3, **arg4; 
	printer *resource;

	PRINTERLS_FETCH();

	if( zend_get_parameters_ex(4, &arg1, &arg2, &arg3, &arg4) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(resource, printer *, arg1, -1, "Printer Handle", PRINTERG(printer_id));

	convert_to_long_ex(arg2);
	convert_to_long_ex(arg3);
	convert_to_string_ex(arg4);

	ExtTextOut(resource->dc, (*arg2)->value.lval , (*arg3)->value.lval, ETO_OPAQUE, NULL, (LPCSTR)(*arg4)->value.str.val, (*arg4)->value.str.len, NULL);
}
/* }}} */


/* {{{ proto int printer_create_font()
   Create a font */
PHP_FUNCTION(printer_create_font)
{
}
/* }}} */


/* {{{ proto void printer_delete_font(int fonthandle)
   Delete a font */
PHP_FUNCTION(printer_delete_font)
{
	pval **arg1; 
	font_struct *font;

	PRINTERLS_FETCH();

	if( zend_get_parameters_ex(1, &arg1) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}
	
	ZEND_FETCH_RESOURCE(font, font_struct *, arg1, -1, "Font Handle", PRINTERG(font_id));

	DeleteObject(font->pointer);
}
/* }}} */


/* {{{ proto void printer_select_font(int printerhandle, int fonthandle)
   Select a font */
PHP_FUNCTION(printer_select_font)
{
	pval **arg1, **arg2;
	font_struct *font;
	printer *resource;

	PRINTERLS_FETCH();

	if( zend_get_parameters_ex(2, &arg1, &arg2) == FAILURE ) {
		WRONG_PARAM_COUNT;
	}

	ZEND_FETCH_RESOURCE(resource, printer *, arg1, -1, "Printer Handle", PRINTERG(printer_id));
	ZEND_FETCH_RESOURCE(font, font_struct *, arg2, -1, "Font Handle", PRINTERG(font_id));

	SelectObject(resource->dc, font->pointer);
}
/* }}} */

/* get the default printer */
char *get_default_printer(void)
{
	PRINTER_INFO_2 *printer;
	DWORD need, received;
	char *printer_name = NULL, *strtok_buf = NULL, buffer[250];

	/* nt/2000 */
	if(GetVersion() < 0x80000000){
		GetProfileString("windows", "device", ",,,", buffer, 250);
		php_strtok_r(buffer, ",", &strtok_buf);
		printer_name = estrdup(buffer);
	}
	/* 9.x/me */
	else {
		EnumPrinters(PRINTER_ENUM_DEFAULT, NULL, 2, NULL, 0, &need, &received);
		if(need > 0) {
			printer = (PRINTER_INFO_2 *)emalloc(need+1);
			EnumPrinters(PRINTER_ENUM_DEFAULT, NULL, 2, (LPBYTE)printer, need, &need, &received);
			printer_name = printer->pDriverName;
			efree(printer);
		}
	}

	return printer_name;
}


#define hex2dec(a) (a>='A')?a-'A'+10:a-'0'

/* convert a hexadecimal number to the rgb colorref */
COLORREF hex_to_rgb(char* hex)
{
	int r = 0,g = 0,b = 0, len;

	len = strlen(hex);
	php_strtoupper(hex, len);
	if (len >= 2) {
		r = hex2dec(hex[0])*16 + hex2dec(hex[1]);
	}
	if (len >= 4) {
		g = hex2dec(hex[2])*16 + hex2dec(hex[3]);
	}
	if (len == 6) {
		b = hex2dec(hex[4])*16 + hex2dec(hex[5]);
	}

	return RGB(r,g,b);
}

/* get a pen style */
int get_pen_style(char *style)
{
	int PS = PS_SOLID;
	if(!strcmp(style,"solid")) PS = PS_SOLID;
	else if(!strcmp(style, "dash")) PS = PS_DASH;
	else if(!strcmp(style, "dot")) PS = PS_DOT;
	else if(!strcmp(style, "dashdot")) PS = PS_DASHDOT;
	else if(!strcmp(style, "dashdot2")) PS = PS_DASHDOTDOT;
	else if(!strcmp(style, "invisible")) PS = PS_NULL;
	else if(!strcmp(style, "inframe")) PS = PS_INSIDEFRAME;
	return PS;
}

/* get a hatch style brush */
int get_hatch_style(char *hatch)
{
	int HS = HS_BDIAGONAL;
	if(!strcmp(hatch, "bdiagonal")) HS = HS_BDIAGONAL;
	else if(!strcmp(hatch, "cross")) HS = HS_CROSS;
	else if(!strcmp(hatch, "diagcross")) HS = HS_DIAGCROSS;
	else if(!strcmp(hatch, "fdiagonal")) HS = HS_FDIAGONAL;
	else if(!strcmp(hatch, "horizontal")) HS = HS_HORIZONTAL;
	else if(!strcmp(hatch, "vertical")) HS = HS_VERTICAL;
	return HS;
}

/* get enumval of input, defaults to local */
DWORD _print_enumvalue(char * EnumStr)
{
	DWORD EV = PRINTER_ENUM_LOCAL;
	php_strtoupper(EnumStr, strlen(EnumStr));
	if(!strcmp(EnumStr, "PRINTER_ENUM_LOCAL")) EV = PRINTER_ENUM_LOCAL;
	else if(!strcmp(EnumStr, "PRINTER_ENUM_NAME")) EV = PRINTER_ENUM_NAME;
	else if(!strcmp(EnumStr, "PRINTER_ENUM_SHARED")) EV = PRINTER_ENUM_SHARED;
	else if(!strcmp(EnumStr, "PRINTER_ENUM_DEFAULT")) EV = PRINTER_ENUM_DEFAULT;
	else if(!strcmp(EnumStr, "PRINTER_ENUM_CONNECTIONS")) EV = PRINTER_ENUM_CONNECTIONS;
	else if(!strcmp(EnumStr, "PRINTER_ENUM_NETWORK")) EV = PRINTER_ENUM_NETWORK;
	else if(!strcmp(EnumStr, "PRINTER_ENUM_REMOTE")) EV = PRINTER_ENUM_REMOTE;
	return EV;
}


/* resource deallocation */
static void destroy_ressources(zend_rsrc_list_entry *resource)
{
	efree(resource);
}

#endif

#endif
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */