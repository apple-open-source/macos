var supported = /MSIE (5\.5)|[6789]/.test(navigator.userAgent) && navigator.platform == "Win32";
var blankSrc = "url(/blojsom/images/blank.gif)";

function makeTranslucentBackground(theElementID, theImageSrc)
{
	if (supported)
	{
		var theElement = document.getElementById(theElementID);
		
		if (theElement)
		{
			theElement.style.background = blankSrc;
			theElement.runtimeStyle.filter = "progid:DXImageTransform.Microsoft.AlphaImageLoader(src='" + theImageSrc + "',sizingMethod='scale')";
		}
	}
}

function makeTranslucentImageSrc(theElementID, theImageSrc)
{
	if (supported)
	{
		var theElement = document.getElementById(theElementID);
		var origSrc = theElement.src
		
		theElement.src = blankSrc;
		theElement.runtimeStyle.filter = "progid:DXImageTransform.Microsoft.AlphaImageLoader(src='" + theImageSrc + "',sizingMethod='scale')";
	}
}
