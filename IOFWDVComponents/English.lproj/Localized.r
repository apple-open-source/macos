#include "MacTypes.r"
//#include "IOFWDVComponents.h"

#define forCarbon			1
#define UseExtendedThingResource	1
#include "Components.r"

#define	kIsocCodecBaseID	-5736

// 'STR '
#define	kIsocCodecNameID	kIsocCodecBaseID

#define	kControlCodecBaseID	-5735

// 'STR '
#define	kControlCodecNameID	kControlCodecBaseID

resource 'STR ' (kIsocCodecNameID, "isoc name")
{
	"FireWire DV Isochronous Handler"
};

resource 'STR ' (kControlCodecNameID, "control name")
{
	"FireWire DV Device Control"
};

/* vendor ID to name matching resource, STR# to vnid */
resource 'STR#' ( -20775, "Vendor Names")
{
	{
		"Canon",
		"JVC",
		"Panasonic",
		"Sharp",
		"Sony",
		"Sony",
	}
};

data 'vnid' (-20775, "Vendor IDs") {
	$"00000006"		/* count of IDs is 6 */
	$"00000085"		/* Canon vendor ID */
	$"00008088"		/* JVC vendor ID */
	$"00008045"		/* Panasonic vendor ID */
	$"00000000"		/* Sharp vendor ID */
	$"00080046"		/* Sony vendor ID */
	$"00080146"		/* Sony vendor ID */
};

