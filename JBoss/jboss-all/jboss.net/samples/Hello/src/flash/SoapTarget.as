/*
	* Multideck Corporation
	* Copyright @2002  All rights reserved.
	* @author Frederick N. Brier
*/

/**
	* Constructor for SoapTarget object.  It designates the server and service
	* that will be used for specified Soap transactions.  The SoapTarget is
	* passed as a parameter to the constructor of SoapOperation which is then
	* used to perform those transactions.
	*
	* @param hostUrl is a string of the format hostname:portnumber.  If the
	*    parameter is null, then the value of the _root.hostUrl will be used.
	* @param serviceContext is a string containing the rootContext +
	*    "/" + soap-servlet-url (usually "services").  If the parameter is null,
	*    then the value of the _root.rootContext will be used with "/services"
	*    Appended
	*
	* @see SoapOperation
*/
function SoapTarget( hostUrl, serviceContext )
{
	this.hostUrl = ( null == hostUrl ) ? _root.hostUrl : hostUrl;
	if ( ( null == serviceContext ) && ( null != rootContext ) )
		this.serviceContext = "/" + _root.rootContext + "/services"
	else
		this.serviceContext = serviceContext;
}