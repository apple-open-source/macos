/*********************************************************************
 *
 * Macromedia Flash Dispatcher -- a scriptable detector for Flash Player
 *
 *
 * copyright (c) 2000 Macromedia, Inc.
 *
 *********************************************************************/


/*
 * URL of the Flash self-detecting movie ("sniffer").
 *
 * Reset this if you move the file out of the directory in which the
 * document containing the script that calls MM_FlashDispatch() resides.
 */

var MM_FlashSnifferURL = "detectFlash.swf";


/*
 * Latest available revisions of the Plug-in.
 */

var MM_latestPluginRevision = new Object();
MM_latestPluginRevision["6.0"] = new Object();
MM_latestPluginRevision["5.0"] = new Object();
MM_latestPluginRevision["4.0"] = new Object();
MM_latestPluginRevision["3.0"] = new Object();
MM_latestPluginRevision["2.0"] = new Object();

/*
 * This table must be updated as new versions and revisions of the
 * plug-in are released, in support of the 'requireLatestRevision'
 * option in MM_FlashDispatch().
 */
//FS 103101 - the 6.0 revision numbers need to be updated once we know the revision numbers will be.
MM_latestPluginRevision["6.0"]["Windows"] = 00;
MM_latestPluginRevision["6.0"]["Macintosh"] = 00;

MM_latestPluginRevision["5.0"]["Windows"] = 30;
MM_latestPluginRevision["5.0"]["Macintosh"] = 30;

MM_latestPluginRevision["4.0"]["Windows"] = 28;
MM_latestPluginRevision["4.0"]["Macintosh"] = 27;
MM_latestPluginRevision["4.0"]["Unix"] = 12;

MM_latestPluginRevision["3.0"]["Windows"] = 10;
MM_latestPluginRevision["3.0"]["Macintosh"] = 10;

MM_latestPluginRevision["2.0"]["Windows"] = 11;
MM_latestPluginRevision["2.0"]["Macintosh"] = 11;


/*
 * MM_FlashInfo() -- construct an object representing Flash Player status
 *
 * Constructor:
 *
 *	new MM_FlashInfo()
 *
 * Properties:
 *
 *	installed		true if player is installed
 *				(undefined if undetectable)
 *
 *	implementation		the form the player takes in this
 *				browser: "ActiveX control" or "Plug-in"
 *
 *	autoInstallable		true if the player can be automatically
 *				installed/updated on this browser/platform
 *
 *	version			player version if installed
 *
 *	revision		revision if implementation is "Plug-in"
 *
 * Methods:
 *
 *	canPlay(contentVersion)	true if installed player is capable of
 *				playing content authored with the
 *				specified version of Flash software
 *
 * Description:
 *
 *	MM_FlashInfo() instantiates an object that contains as much
 *	information about Flash Player--whether it is installed, what
 *	version is installed, and so one--as is possible to collect.
 *
 *	Where Flash Player is implemented as a plug-in and the user's
 *	browser supports plug-in detection, all properties are defined;
 *	this includes Netscape on all platforms and Microsoft Internet
 *	Explorer 5 on the Macintosh.  Where Flash Player is implemented
 *	as an ActiveX control (MSIE on Windows), all properties except
 *	'revision' are defined.
 *
 *	Prior to version 5, Microsoft Internet Explorer on the Macintosh
 *	did not support plug-in detection.  In this case, no properties
 *	are defined, unless the cookie 'MM_FlashDetectedSelf' has been
 *	set, in which case all properties except 'version' and 'revision'
 *	are set.
 *
 *	This object is primarily meant for use by MM_FlashDispatch(), but
 *	may be of use in reporting the player version, etc. to the user.
 */

var MM_FlashControlInstalled;	// is the Flash ActiveX control installed?
var MM_FlashControlVersion;	// ActiveX control version if installed

function MM_FlashInfo()
{
    if (navigator.plugins && navigator.plugins.length > 0)
    {
	this.implementation = "Plug-in";
	this.autoInstallable = false;	// until Netscape SmartUpdate supported

	// Check whether the plug-in is installed:

	if (navigator.plugins["Shockwave Flash"])
	{
	    this.installed = true;

	    // Get the plug-in version and revision:

	    var words =
		navigator.plugins["Shockwave Flash"].description.split(" ");

	    for (var i = 0; i < words.length; ++i)
	    {
		if (isNaN(parseInt(words[i])))
		continue;

		this.version = words[i];
		

		this.revision = parseInt(words[i + 1].substring(1));
	    }
	}
	else
	{
	    this.installed = false;
	}
    }
    else if (MM_FlashControlInstalled != null)
    {
	this.implementation = "ActiveX control";
	this.installed = MM_FlashControlInstalled;
	this.version = MM_FlashControlVersion;
	this.autoInstallable = true;
    }
    else if (MM_FlashDetectedSelf())
    {
	this.installed = true;
	this.implementation = "Plug-in";
	this.autoInstallable = false;
    }

    this.canPlay = MM_FlashCanPlay;
}


/*
 * MM_FlashDispatch() -- get Flash Player status and redirect appropriately
 *
 * Synopsis:
 *
 *	MM_FlashDispatch(contentURL, contentVersion, requireLatestRevision,
 *			 upgradeURL, install, installURL, altURL,
 *			 overridePluginsPage)
 *
 *	Arguments:
 *
 *	    contentURL			URL of document containing Flash content
 *
 *	    contentVersion		version of Flash software used to
 *					author content
 *
 *	    requireLatestRevision	Boolean indicating whether to require
 *					latest revision of player (plug-in only)
 *
 *	    upgradeURL			document to load if player must be
 *					upgraded to play content and automated
 *					updating is not supported on the user's
 *					browser & platform
 *
 *	    install			Boolean indicating whether to install
 *					if player is not installed
 *
 *	    installURL			document to load if 'install' is true
 *					and automated installation is not
 *					supported on user's browser & platform
 *
 *	    altURL			document to load if 'install' is false
 *
 *	    overridePluginsPage		Boolean indicating whether to set the
 *					PLUGINSPAGE attribute for the embedded
 *					Flash Player sniffer to `installURL'
 *		disableAutoInstall		Boolean indicating that the auto-installation
 *					should not occur and that the user will go to the installURL
 * 					or to the upgradeURL as specified
					
 *
 *	Returns:
 *
 *	    Normally, never returns; changes window.location.
 *	    Returns with no value when called improperly.
 *
 * Description:
 *
 *	MM_FlashDispatch() detects whether the user's Web browser has the
 *	Flash plug-in or ActiveX control installed, and what version is
 *	installed if so. It then takes appropriate action based on whether
 *	Flash Player is installed and is compatible with 'contentVersion':
 *	load a document containing Flash content, load alternate content,
 *	or oversee the updating or installation of the player.
 *
 *	There are three possible outcomes of the detection process: 
 *
 *	    1. A version of Flash Player has been detected that is
 *	       suitable for playing the requested content version.
 *	       MM_FlashDispatch() will load 'contentURL'.
 *
 *	    2. An unsuitable version of Flash Player has been detected.
 *	       MM_FlashDispatch() will load 'contentURL' if automated
 *	       updating is supported on the user's browser & platform;
 *	       otherwise, it will load 'upgradeURL'.
 *
 *	    3. Flash Player is not installed.  If 'install' is set to
 *	       true, MM_FlashDispatch() will load 'contentURL' if the
 *	       user's browser supports automated installation; otherwise,
 *	       it will load 'installURL'.  If 'install' is false,
 *	       MM_FlashDispatch() will load 'altURL'.
 *
 *	When script-based detection of Flash Player is not possible,
 *	MM_FlashDispatch() attempts to load a Flash movie to carry out
 *	the detection. If Flash Player is not installed, there is presently
 *	no choice but to let the browser redirect the user via the
 *	PLUGINSPAGE attribute of the EMBED tag. In this case, 'install'
 *	is ignored, but setting 'overridePluginsPage' to true will
 *	set PLUGINSPAGE to 'installURL', overriding its default value
 *	(the URL for the Macromedia Flash download center). If this flag
 *	is set, 'installURL' must be absolute, not relative.
 */

var MM_FlashPluginsPage = "http://www.macromedia.com/shockwave/download/index.cgi?P1_Prod_Version=ShockwaveFlash";

function MM_FlashDispatch(contentURL, contentVersion, requireLatestRevision,
			  upgradeURL, install, installURL, altURL,
			  overridePluginsPage,disableAutoInstall)
{
    if (disableAutoInstall == null)
    {
	alert("ERROR: MM_FlashDispatch() called with too few arguments.");
	return;
    }


    if (overridePluginsPage && installURL.substring(0, 7) != "http://")
    {
	alert("ERROR: MM_FlashDispatch() called with relative URL" +
	    " for PLUGINSPAGE (" + installURL + ")");

	return;
    }


    var player = new MM_FlashInfo();

    if (player.installed == null)
    {
	var sniffer =
	    "<EMBED HIDDEN=\"true\" TYPE=\"application/x-shockwave-flash\"" +
		  " WIDTH=\"18\" HEIGHT=\"18\"" +
		  " BGCOLOR=\"" + document.bgcolor + "\"" +
		  " SRC=\"" + MM_FlashSnifferURL +
			"?contentURL=" + contentURL + "?" +
			"&contentVersion=" + contentVersion +
			"&requireLatestRevision=" + requireLatestRevision +
			"&latestRevision=" +
			    MM_FlashLatestPluginRevision(contentVersion) +
			"&upgradeURL=" + upgradeURL +
			"\"" +
		  " LOOP=\"false\" MENU=\"false\"" +
		  " PLUGINSPAGE=\"" +
		    (overridePluginsPage ? installURL : MM_FlashPluginsPage) +
		    "\"" +
	    ">" +
	    "</EMBED>";

	document.open();
	document.write("<HTML><HEAD><TITLE>");
	document.write("Checking for the Flash Player");
	document.write("</TITLE></HEAD>");
	document.write("<BODY BGCOLOR=\"" + document.bgcolor + "\">");
	document.write(sniffer);
	document.write("</BODY>");
	document.write("</HTML>");
	document.close();
    }
    else if (player.installed)
    {
	if (player.canPlay(contentVersion, requireLatestRevision))
	{
	    location = contentURL;
	}
	else
	{
	if (disableAutoInstall)
	{
	location = upgradeURL;
	}else
	{
	    location = player.autoInstallable ? contentURL : upgradeURL;
	}
	}
    }
    else if (install)
    {
	if (disableAutoInstall){
	location = installURL;
	}
	else{
	location = player.autoInstallable ? contentURL : installURL;
	}
    }
    else
    {
	location = altURL;
    }
}


/*
 * MM_FlashRememberIfDetectedSelf() -- record that Flash Player detected itself
 *
 * Synopsis:
 *
 *	MM_FlashRememberIfDetectedSelf()
 *	MM_FlashRememberIfDetectedSelf(count)
 *	MM_FlashRememberIfDetectedSelf(count, units)
 *
 *	Arguments:
 *
 *	    count		length of time in units before re-checking
 *				whether content can be played (default: 60)
 *
 *	    units		unit(s) of time to count: "minute(s)," "hour(s)"
 * 				or "day(s)" (default: "days")
 *
 *
 * Description:
 *
 *	This function conditionally sets a cookie signifying that
 *	the current document was referred via the Dispatcher using
 *	Flash Player self-detection.  It is intended to spare the user
 *	whose browser does not support script-based detection from the
 *	process of Flash Player self-detection on each visit.
 *	
 *	The cookie persists for 60 days, or for the amount of time
 *	specified by the 'count' and 'units' parameters.
 *
 *	If cookies are not being accepted, this function is a no-op;
 *	the Dispatcher will simply attempt Flash Player self-detection
 *	on subsequent visits.
 *
 *	This function must be called from a script embedded in the
 *	document referenced by the 'contentURL' argument to
 *	MM_FlashDispatch().
 *
 */

function MM_FlashRememberIfDetectedSelf(count, units)
{
    // the sniffer appends an empty search string to the URL
    // to indicate that it is the referrer

    if (document.location.search.indexOf("?") != -1)
    {
	if (!count) count = 60;
	if (!units) units = "days";

	var msecs = new Object();

	msecs.minute = msecs.minutes = 60000;
	msecs.hour = msecs.hours = 60 * msecs.minute;
	msecs.day = msecs.days = 24 * msecs.hour;

	var expires = new Date();

	expires.setTime(expires.getTime() + count * msecs[units]);

	document.cookie =
	    'MM_FlashDetectedSelf=true ; expires=' + expires.toGMTString();
    }
}


/*
 * MM_FlashDemur() -- record user's decision not to install Flash Player
 *
 * Synopsis:
 *
 *	MM_FlashDemur()
 *	MM_FlashDemur(count)
 *	MM_FlashDemur(count, units)
 *
 *	Arguments:
 *
 *	    count	length of time in units to remember decision
 *			(default: 60)
 *
 *	    units	unit(s) of time to count: "minute(s)," "hour(s)"
 *			or "day(s)" (default: "days")
 *
 *	Returns:
 *
 *	    true if successful; false otherwise.
 *
 * Description:
 *
 *	MM_FlashDemur() sets a cookie signifying that the user requested
 *	that the decision not to install Flash be remembered.
 *
 *	The cookie persists for 60 days, or for the amount of time
 *	specified by the 'count' and 'units' parameters.
 *
 *	This function may be used as the handler for the 'onClick' event
 *	associated with the user's selecting a link to alternate content.
 *	If cookies are not being accepted, it will return false; this
 *	may be used to control whether the link is followed.
 */

function MM_FlashDemur(count, units)
{
    if (!count) count = 60;
    if (!units) units = "days";

    var msecs = new Object();

    msecs.minute = msecs.minutes = 60000;
    msecs.hour = msecs.hours = 60 * msecs.minute;
    msecs.day = msecs.days = 24 * msecs.hour;

    var expires = new Date();

    expires.setTime(expires.getTime() + count * msecs[units]);

    document.cookie =
	'MM_FlashUserDemurred=true ; expires=' + expires.toGMTString();


    if (!MM_FlashUserDemurred())
    {
	alert("Your browser must accept cookies in order to " +
	      "save this information.  Try changing your preferences.");

	return false;
    }
    else
	return true;
}


/*
 * MM_FlashUserDemurred() -- recall user's decision not to install Flash Player
 *
 * Synopsis:
 *
 *	MM_FlashUserDemurred()
 *
 *	Returns:
 *
 *	    true if a cookie signifying that the user declined to install
 *	    Flash Player is set; false otherwise.
 *
 * Description:
 *
 *	This function is useful in determining whether to set the 'install'
 *	flag when calling MM_FlashDispatch().  If true, it means that the
 *	user's previous decision not to install Flash Player should be
 *	honored, i.e., 'install' should be set to false.
 */

function MM_FlashUserDemurred()
{
    return (document.cookie.indexOf("MM_FlashUserDemurred") != -1);
}


/*********************************************************************
 * THE FOLLOWING FUNCTIONS ARE NOT PUBLIC.  DO NOT CALL THEM DIRECTLY.
 *********************************************************************/

/*
 * MM_FlashLatestPluginRevision() -- look up latest Flash Player plug-in
 *				     revision for this platform
 *
 * Synopsis:
 *
 *	MM_FlashLatestPluginRevision(playerVersion)
 *
 *	Arguments:
 *
 *	    playerVersion	plug-in version to look up revision of
 *
 *	Returns:
 *
 *	    The latest available revision of the specified version of
 *	    the Flash Player plug-in on this platform, as an integer;
 *	    undefined for versions before 2.0.
 *
 * Description:
 *
 *	This look-up function is only intended to be called internally.
 */

function MM_FlashLatestPluginRevision(playerVersion)
{
    var latestRevision;
    var platform;

    if (navigator.appVersion.indexOf("Win") != -1)
	platform = "Windows";
    else if (navigator.appVersion.indexOf("Macintosh") != -1)
	platform = "Macintosh";
    else if (navigator.appVersion.indexOf("X11") != -1)
	platform = "Unix";

    latestRevision = MM_latestPluginRevision[playerVersion][platform];

    return latestRevision;
}


/*
 * MM_FlashCanPlay() -- check whether installed Flash Player can play content
 *
 * Synopsis:
 *
 *	MM_FlashCanPlay(contentVersion, requireLatestRevision)
 *
 *	Arguments:
 *
 *	    contentVersion		version of Flash software used to
 *					author content
 *
 *	    requireLatestRevision	Boolean indicating whether latest
 *					revision of plug-in should be required
 *
 *	Returns:
 *
 *	    true if the installed player can play the indicated content;
 *	    false otherwise.
 *
 * Description:
 *
 *	This function is not intended to be called directly, only
 *	as an instance method of MM_FlashInfo.
 */

function MM_FlashCanPlay(contentVersion, requireLatestRevision)
{
    var canPlay;

    if (this.version)
    {
	canPlay = (parseInt(contentVersion) <= this.version);

	if (requireLatestRevision)
	{
	    if (this.revision &&
		this.revision < MM_FlashLatestPluginRevision(this.version))
	    {
		canPlay = false;
	    }
	}
    }
    else
    {
	canPlay = MM_FlashDetectedSelf();
    }

    return canPlay;
}


/*
 * MM_FlashDetectedSelf() -- recall whether Flash Player has detected itself
 *
 * Synopsis:
 *
 *	MM_FlashDetectedSelf()
 *
 *	Returns:
 *
 *	    true if a cookie signifying that Flash Player has detected itself
 *	    is set; false otherwise.
 *
 * Description:
 *
 *	This function is only meant to be called internally.
 */

function MM_FlashDetectedSelf()
{
    return (document.cookie.indexOf("MM_FlashDetectedSelf") != -1);
}
