/*
	* Multideck Corporation
	* Copyright @2002  All rights reserved.
	* @author Frederick N. Brier
*/

/*
	* Constant used to generate SOAP XML for parameters.
*/
SoapParms.prototype.XSI_TYPE_STRING = 'xsi:type="xsd:string"';

/**
	* The add() method adds a new parameter to the list.
	* @param value is the value of the parameter.
	* @param varType is the XML type of the parameter.  If it is not
	* specified, the it defaults to type xsd:string.
*/
SoapParms.prototype.addParm = function( value, xsiType )
{
	var parmXsiType = SoapParms.prototype.XSI_TYPE_STRING;
	if ( null == value )
		return;
	if ( null != xsiType	)
		{
		parmXsiType = 'xsi:type="xsd:' + xsiType + '"';
		}
	var argStr = "arg" + this.parmArray.length;
	this.parmArray.push( new XML( "<" + argStr + " " + parmXsiType + ">" + value +
		"</" + argStr + ">" ) );
}


/**
	* The reset() method clears the state of the object and allows it to be
	* reused if so desired.
*/
SoapParms.prototype.reset = function()
{
	this.parmArray = new Array();
}


/**
	* Constructor for SoapParms object.
*/
function SoapParms()
{
	this.parmArray = new Array();
}   // of Class SoapParms

