#-----------------------------------------------------------------------------
#
#                           Gotmail - gotmail.awk
#
#             1999 by Thomas Nesges <ThomasNesges@TNT-Computer.de>
#
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# This script is part of GotMail.  It emits html to a specified File
# The AWK-Library htmllib has to be properly installed.
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
function init_environ()
{
	TextColor = ENVIRON["GOTM_TXCOL"]
	BackColor = ENVIRON["GOTM_BGCOL"] 
	MsgColor  = ENVIRON["GOTM_MSGCOL"]
	ErrColor  = ENVIRON["GOTM_ERRCOL"]
	TimColor  = ENVIRON["GOTM_TIMCOL"]
	OutFile   = ENVIRON["GOTM_HTMLFILE"]
	PrintMsg  = toupper(ENVIRON["GOTM_MSG"])
	PrintErr  = toupper(ENVIRON["GOTM_ERR"])
	PrintTim  = toupper(ENVIRON["GOTM_TIM"])
	PrintHed  = toupper(ENVIRON["GOTM_HED"])
	
}
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
{
  init_environ()
  if($2!="reading")
  {
	if($3=="messages")
	{
		Mails = Mails TableRow("start", MsgColor)	
		Mails = Mails TableItem($5) TableItem($7)
		Mails = Mails TableItem(Align($2,0)) 
		Mails = Mails TableRow("stop")
	}
	else if($3=="fetchmail")
	{
		Times = Times TableRow("start", TimColor)
		Times = Times TableItem($0)
		Times = Times TableRow("stop")
	}
	else
	{
		Errors = Errors TableRow("start", ErrColor)	
		Errors = Errors TableItem($0)
		Errors = Errors TableRow("stop")	
	}
  }
}
#-----------------------------------------------------------------------------
END	{
	Stats = StartPage(Title("Gotmail Stats") Body(BackColor, TextColor))
	if(PrintHed == "YES")
	{
		Stats = Stats Align(Headline("Gotmail Stats",1),0) 
		Stats = Stats Divider Newline
	}
	if(PrintMsg == "YES")
	{
		Stats = Stats TableStart(1)
		Stats = Stats TableRow("start", MsgColor)
		Stats = Stats TableItem(Bold("Account"))
		Stats = Stats TableItem(Bold("Server"))
		Stats = Stats TableItem(Bold("Mails fetched"))
		Stats = Stats TableRow("stop")
		Stats = Stats Mails TableEnd Newline Divider Newline
	}

	if(PrintErr == "YES")
	{
		Stats = Stats TableStart(1) 
		Stats = Stats TableRow("start", ErrColor)
		Stats = Stats TableItem(Bold("Error Messages"))
		Stats = Stats TableRow("stop")
		Stats = Stats Errors TableEnd Newline Divider 
	}

	if(PrintTim == "YES")
	{
		Stats = Stats TableStart(1)
		Stats = Stats TableRow("start", TimColor)
		Stats = Stats TableItem(Bold("Start/Stop Times"))
		Stats = Stats TableRow("stop")
		Stats = Stats Times TableEnd Newline Divider
	}

	Stats = Stats Center("start") "GotMail - 1999 by Thomas Nesges " 
	Stats = Stats "<ThomasNesges@TNT-Computer.de>" Center("stop") EndPage

	print Stats > OutFile
	}
#-----------------------------------------------------------------------------
