/*
	* Multideck Corporation
	* Copyright @2002  All rights reserved.
	* @author Frederick N. Brier
*/

/**
	* Prototype declarations for SoapOperation
*/

// String constants:
SoapOperation.prototype.loadHandlerFncName = "SoapOperation.loadHandler(): ";

/**
	* getter FaultCode - Type: String
*/
SoapOperation.prototype.getFaultCode = function()
{
	return this.faultCode;
};

/**
	* setter FaultCode - Type: String
*/
SoapOperation.prototype.setFaultCode = function( p )
{
//	if ( null != onFault )
//		this.onFault.faultCode = p;
   this.faultCode = p;
};

//---getter/setter FaultString - Type: String
SoapOperation.prototype.getFaultString = function()
{
	return this.faultString;
};

SoapOperation.prototype.setFaultString = function( p )
{
//	if ( null != onFault )
//		this.onFault.faultString = p;
	this.faultString = p;
};

//---getter/setter FaultDetail - Type: XML object
SoapOperation.prototype.getFaultDetail = function()
{
	return this.faultDetail;
};

SoapOperation.prototype.setFaultDetail = function( p )
{
//	if ( null != onFault )
//		this.onFault.faultDetail = p;
	this.faultDetail = p;
};


//---getter/setter Response - Type: XML object
SoapOperation.prototype.getResponse = function()
{
	return this.response;
};

SoapOperation.prototype.setResponse = function( p )
{
//	if ( null != onResponse )
//		this.onResponse.response = p;
	this.response = p;
};

/**
	* The loadHandler is an internal method used by the send() method.  The
	* loadHandler is assigned to the soapMsg's XML.onLoad() method attribute.
	* It parses the SOAP response placing the result in the attribute
	* "response" for the onResponse() to access and process.  Note that even
	* though this appears it is a method in the SoapOperation class, it is
	* actually used as a method within an XML object.  Thus the "this" pointer
	* when executed is the ResultXML XML object.  However, the calling method,
	* send(), does assign the SoapOperation "this" pointer to the XML object
	* in the "transaction" attribute.
*/
SoapOperation.prototype.loadHandler = function( success )
{
	var	fncName = this.transaction.loadHandlerFncName;
	var	msg;

	myDebugWindow.log( fncName + "Entered" );

	// Visit top level XML object (SOAP Envelope) from server response.
	var	envelope = this.firstChild;
	if (null != envelope)
		{
		myDebugWindow.log( fncName + "TRACE - Server response contained SOAP Envelope (" + envelope.nodeName + ")" );
		}
	else
		{
		msg = "Server response did not have a SOAP Envelope."
		this.transaction.faultString = msg;
		myDebugWindow.log( fncName + msg );
		this.transaction.onFault();
		return;
		}

	// Visit SOAP Body XML node from SOAP Envelope.
	var	body = envelope.firstChild;
	if (null != body)
		{
		myDebugWindow.log( fncName + "TRACE - Server response contained SOAP Body (" + body.nodeName + ")" );
		}
	else
		{
		msg = "Server response did not have a SOAP Body."
		this.transaction.faultString = msg;
		myDebugWindow.log( fncName + msg );
		this.transaction.onFault();
		return;
		}
	
	var	response = body.firstChild;
	if (null == response)
		{
		msg = "Server response did not have a SOAP Response."
		this.transaction.faultString = msg;
		myDebugWindow.log( fncName + msg );
		this.transaction.onFault();
		return;
		}

	myDebugWindow.log( fncName + "Found response element = " + response.nodeName );

	// Parsing response name to determine if normal response or SOAP Fault.
	var	responseName = response.nodeName;
	if ( responseName == "SOAP-ENV:Fault" )
		{
		myDebugWindow.log( fncName + "SOAP fault found." );

		// Parse the Fault elements
		var		child_nodes = response.childNodes();
		var		curNode;
		for ( var i = 0; i < child_nodes.length; i++ )
			{
			curNode = child_nodes[ i ];
			if ( curNode.nodeName.toUpperCase() == "FAULTCODE" )
				this.transaction.faultCode = curNode.nodeValue;
			else if ( curNode.nodeName.toUpperCase() == "FAULTSTRINGT" )
				this.transaction.faultString = curNode.nodeValue;
			else if ( curNode.nodeName.toUpperCase() == "DETAIL" )
				this.transaction.faultDetail = curNode;
			}
		this.transaction.onFault();
		return;
		}

	// responseName should be the opName + "Response"
	if ( responseName.toUpperCase() != this.transaction.opName.concat( "Response" ).toUpperCase() )
		{
		// Error - responseName needs to equal the opName + Response.
		msg = "Response name (" + responseName + ") does not match opName (" + this.transaction.opName.concat( "Response" ) + ").";
		this.transaction.faultString = msg;
		myDebugWindow.log( fncName + msg );
		this.transaction.onFault();
		return;
		}

	// Copy all the elements within the response to the results forwarded to the
	// onResponse handler.
	var results = this.transaction.response = new XML();
	for ( var i = 0; i < response.childNodes.length; i++ )
		{
		results.appendChild( response.childNodes[ i ] );
		}
	this.transaction.onResponse();

}   // of method loadHandler


/**
	* The send() method initiates the SOAP transaction.
	*
	* @param parms is the data being sent to the SOAP server.  It is an SoapParms object.
	* @param session is the session. Maybe null.
	*
*/
SoapOperation.prototype.send = function( parms, session )
{
	myDebugWindow.log( "SoapOperation.send(): Entered." );

	// Construct the SOAP message
	var soapMsg = new XML( "<SOAP-ENV:Envelope xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"></SOAP-ENV:Envelope>" );
	var soapEnvelopeBody = soapMsg.createElement( "SOAP-ENV:Body" );
	soapMsg.firstChild.appendChild( soapEnvelopeBody );
	var soapBody = soapMsg.createElement( this.opName );

	myDebugWindow.log( "SoapOperation.send(): About to create arguments." );

	soapEnvelopeBody.appendChild( soapBody );
	// Add arguments to body
	myDebugWindow.log( "SoapOperation.send(): There are " + parms.numParms + "parameters." );
	for ( i = 0; i < parms.parmArray.length; i++ )
		{
		myDebugWindow.log( "SoapOperation.send(): Arg[" + i + "] is " + parms.parmArray[ i ].toString() );
		soapBody.appendChild( parms.parmArray[ i ] );
		}
	
	myDebugWindow.log( "SoapOperation.send(): Created SOAP parameters." );

	soapMsg.contentType = "text/xml; charset=utf-8";
	soapMsg.xmlDecl = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";

	var resultXML = new XML();
	resultXML.ignoreWhite = true;
	resultXML.onLoad = this.loadHandler;
	// Set reference back to this Transaction object.
	resultXML.transaction = this;

	myDebugWindow.log( "SoapOperation.send(): Initialized resultXML." );

	// String
	this.faultCode = null;
	// String
	this.faultString = null;
	// XML object
	this.faultDetail = null;
	// XML object
	this.response = null;

	myDebugWindow.log( "SoapOperation.send(): this.soapUrl = " + this.soapUrl );
	myDebugWindow.log( "SoapOperation.send(): soapMsg = \n" + soapMsg.toString() );
	myDebugWindow.log( "SoapOperation.send(): Calling soapMsg.sendAndLoad()" );

	soapMsg.sendAndLoad( this.soapUrl, resultXML, "POST" );

}   // of method send()


/**
	* Constructor for SoapOperation object.
	*
	* @param soapTarget is an object of type SoapTarget
	* @param action is the SOAP action.
	* @param opName is the name of SOAP operation being invoked.
	* @param secure is a boolean indicating whether the protocol should be http
	* or https.
	* @param onResponse is the specialized function handler for a particular
	* SOAP operation in the event of a successful response.
	* @param onFault is the specialized function handler for a particular
	* SOAP operation in the event of a failed response.
*/
function SoapOperation( soapTarget, action, service, opName, secure, onResponse, onFault )
{
	// Properties:
	this.soapTarget = soapTarget;
	this.action = ( null == action ) ? "" : action;
	this.service = service
	this.opName = opName;
	this.secure = ( null == secure ) ? false : secure;
	this.onResponse = onResponse;
	this.onFault = onFault;
	this.myDebugWindow = null;

	// Construct the SOAP URL
//	var urlAttr = '?SOAPAction="' + this.action + '"';
	var urlAttr = '?SOAPAction=""';
	this.soapUrl = ( this.secure ? "https://" : "http://" ) + this.soapTarget.hostUrl +
		"/" + this.soapTarget.serviceContext + "/" + this.service + urlAttr;

	/*
		* Set up properties for SoapOperation
		*
		* The main purpose for these properties is to mirror the value in the
		* Transaction object to properties being set in OnFault and OnResponse
		* handler objects provided for additional processing of the SOAP request.
		* The mirrored properties are faultCode, faultString, and faultDetail for
		* the onFault handler, and the response property for the onResponse
		* handler.
	*/
	this.addProperty( "faultCode", this.getFaultCode, this.setFaultCode );
	this.faultCode = null;

	this.addProperty( "faultString", this.getFaultString, this.setFaultString );
	this.faultString = null;

	this.addProperty( "faultDetail", this.getFaultDetail, this.setFaultDetail );
	this.faultDetail = null;

	this.addProperty( "response", this.getResponse, this.setResponse );
	this.response = null;

}   // of class SoapOperation
