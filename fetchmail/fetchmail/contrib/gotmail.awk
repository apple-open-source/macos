#-----------------------------------------------------------------------------
#
#                           Gotmail - gotmail.awk
#
#             1999 by Thomas Nesges <ThomasNesges@TNT-Computer.de>
#
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# This script is part of GotMail. It gives back normal text to the console
#-----------------------------------------------------------------------------

{
  if($2!="reading")
  {
	if(($3=="message") || ($3=="messages"))
	{	
		Mails = Mails sprintf(" %- 40s ",substr($5,1,40))
		Mails = Mails sprintf(" %- 5s ",substr($2,1,5))
		Mails = Mails sprintf(" %- 30s\n",substr($7,1,30))
	}
	else if($3=="fetchmail")
	{
		Started = Started " " $0 "\n"
	}
	else
	{
		Errors = Errors $0 "\n"
	}
  }
}

END	{
		Separator = "-------------------------------------------------------------------------------"
		if(ENVIRON["GOTM_HED"]=="yes")
		{
			print "\n\t\t---------------------------------------"
			print "\t\t| ** GotMail - Stats for fetchmail ** |"
			print "\t\t---------------------------------------"
		}
		if(ENVIRON["GOTM_MSG"]=="yes")
		{
			print Separator 
			print "|    Fetched Mails:"
			print Separator 
			print Mails
		}
		if(ENVIRON["GOTM_ERR"]=="yes")
		{
			print Separator 
			print "|    Error Messages:"
			print Separator 
			print Errors
		}
		if(ENVIRON["GOTM_TIM"]=="yes")
		{
			print Separator
			print "|    Fetchmail started/stoped:"
			print Separator
			print Started
		}
	}
