# Example Sieve Script
#   Author: Matthew Elvey (Slightly modified to remove syntax and context errors)
#   URL: http://www.elvey.com/it/sieve/SieveScript.txt

# Initial version completed and put in place 4/1/02 by Matthew Elvey  (firstname@lastname.com ; I've checked and it's not a valid address.); Copyright (C).and.current as of 5/19/2002 
#Change log:
#+ spam[:high]; major reordering; +DFC,BugTraq, PB up +Economist, FolderPath corrections 
#+ redid .0 matches. +Korean + whitelist +@f(useful once I start bouncing mail!)
#+open mag, simplifications, to fm=spamNOTwhite, Bulk changes, IETF rules, +lst
#Reword spam bounce.+scalable@ re-correction+++Work+activate Spam Optimization, etc...
#oops high = 2x threshold, so 2x1 is 2!  Too low. To @fm:bounce.  Added tons of comments.
require ["fileinto", "reject", "vacation", "envelope", "regex"];

if header :contains "subject" ["un eject", "lastname.com/spamoff.htm agreed to"] {  #I give out "uneject" to people to let them bypass the spam or size filters.
  keep;
} elsif header :contains "subject" ["ADV:", "bounceme", "2002 Gov Grants",   #bounceme is useful for testing.
             "ADV:ADLT", "ADV-ADULT", "ADULT ADVERTISEMENT"] {  #Subject text required by various US State laws
  reject text: 
  Hello.  The server content filter/spam detector I use has bounced your message. It appears to be spam. 

  I do not accept spam/UCE (Unsolicited Commercial Email). 

Please ask me how to bypass this filter if your email is not UCE.  In that case, I am sorry about this 
highly unusual error.  The filter is >99% accurate.

  (This is an automated message; I will not be aware that your message did not get through if I do not hear from you again.)

  -Firstname

  (P.S. You may also override the filter if you accept the terms at http://www.lastname.com/spamoff.htm, 
         by including "lastname.com/spamoff.htm agreed to." in the subject.)
.
   ;
}
# LINE 30.
  elsif size :over 10M {    # (note that the four leading dots get "stuffed" to three)

  reject text:
   Message NOT delivered!
   This system normally accepts email that is less than 10MB in size, because that is how I configured it.
   You may want to put your file on a server and send me the URL.
   Or, you may request override permission and/or unreject instructions via another (smaller) email.
   Sorry for the inconvenience.

   Thanks,

.... Firstname
   (This is an automated message; I will not be aware that your message did not get through if I do not hear from you again.)

   Unsolicited advertising sent to this E-Mail address is expressly prohibited 
   under USC Title 47, Section 227.  Violators are subject to charge of up to 
   $1,500 per incident or treble actual costs, whichever is greater.
.
  ; 
#LINE 47.
} elsif header :contains "From" "Firstname@lastname.com" {	#if I send myself email, leave it in the Inbox.
  keep;			#next, is the processing for the various mailing lists I'm on.  
} elsif header :contains ["Sender", "X-Sender", "Mailing-List", "Delivered-To", "List-Post", "Subject", "To", "Cc", "From", "Reply-to", "Received"] "burningman" {
  fileinto "INBOX.DaBurn";
} elsif header :contains ["Subject", "From", "Received"] ["E*TRADE", "Datek", "TD Waterhouse", "NetBank"] {
  fileinto "INBOX.finances.status";
} elsif header :contains "subject" "\[pacbell" {
  fileinto "INBOX.pacbell.dslreports";
} elsif header :contains "From" ["owner-te-wg ", "te-wg ", "iana.org"] {
  fileinto "INBOX.lst.IETF";
} elsif header :contains ["Mailing-List", "Subject", "From", "Received"] ["Red Hat", "Double Funk Crunch", "@economist.com", "Open Magazine", "@nytimes.com", "mottimorell", "Harrow Technology Report"] {
  fileinto "INBOX.lst.interesting";
} elsif header :contains ["Mailing-List", "Subject", "From", "Received", "X-LinkName"] ["DJDragonfly", "Ebates", "Webmonkey", "DHJ8091@aol.com", "Expedia Fare Tracker", "SoulShine", "Martel and Nabiel", "\[ecc\]"] {
  fileinto "INBOX.lst.lame";
} elsif header :contains ["Subject", "From", "To"] ["guru.com", "monster.com", "hotjobs", "dice.com", "linkify.com"] {  #job boards and current clients.
  fileinto "INBOX.lst.jobs";
} elsif header :contains "subject" "\[yaba" {
  fileinto "INBOX.rec.yaba";
} elsif header :contains ["to", "cc"] "scalable@" {
  fileinto "INBOX.lst.scalable";
} elsif header :contains ["Sender", "To", "Return-Path", "Received"] "NTBUGTRAQ@listserv.ntbugtraq.com" {
  fileinto "INBOX.lst.bugtraq";
} elsif header :contains "subject" "Wired" {
  fileinto "INBOX.lst.wired";
#LINE 72.
} elsif anyof (header :contains "From" ["postmaster", "daemon", "abuse"], header :contains "Subject" ["warning:", "returned mail", "failure notice", "undelivered mail"] ) {
keep;		#this one is important - don't want to miss any bounce messages!
#LINE 77.
} elsif anyof (header :contains "From" ["and here I put a whitelist of pretty much all the email addresses in my address book - it's several pages..."]) {
  fileinto "INBOX.white"; 
# better than keep;
# LINE 106.


} elsif anyof (address :all :is ["To", "CC", "BCC"] "Firstname.lastname@fastmail.fm",    #a couple people send to this, but I have have all their addrs in whitelist so OK.
           header :matches "X-Spam-score"  ["9.?" , "10.?", "9", "10", "11.?", "12.?" ,"13.?", "14.?", "11", "12","13", "14", "15.?", "16.?", "17.?" ,"18.?", "19.?", "15", "16", "17" ,"18", "19", "2?.?", "2?", "3?.?" , "3?", "40"]) { 		 #"5.?", "6.?", "5", "6" "7.?" , "8.?" , "7", "8"
  reject text: 
  Hello.  The server content filter/spam detector I use has bounced your message. It appears to be spam. 

  I do not accept spam/UCE (Unsolicited Commercial Email). 

Please ask me how to bypass this filter if your email is not UCE.  In that case, I am sorry about this 
highly unusual error.  The filter is >99% accurate.

  (This is an automated message; I will not be aware that your message did not get through if I do not hear from you again.)

  -Firstname

  (P.S. You may also override the filter if you accept the terms at http://www.lastname.com/spamoff.htm, 
         by including "lastname.com/spamoff.htm agreed to." in the subject.)
.
   ;
#LINE 127.
 
} elsif 
header :matches "X-Spam" ["spam", "high"] { if					#optimization idea line 1/2
           header :matches "X-Spam-score" ["5.?", "6.?", "5", "6"] { 
  fileinto "INBOX.Spam.5-7"; 
} elsif header :matches "X-Spam-score" ["7.?" , "8.?" , "7", "8"] { 
  fileinto "INBOX.Spam.7-9"; 
#} elsif header :matches "X-Spam-score" ["9.?" , "10.?" , "9", "10"] { 	#These lines obsoleted by reject text rule above, but others will find 'em useful!
#  fileinto "INBOX.Spam.9-11"; 
#} elsif header :matches "X-Spam-score" ["11.?" , "12.?" ,"13.?" , "14.?", "11" , "12" ,"13" , "14"] { 
#  fileinto "INBOX.Spam.11-15"; 
#} elsif header :matches "X-Spam-score" ["15.?" , "16.?" ,"17.?" ,"18.?" , "19.?", "15" , "16" ,"17" ,"18" , "19"] { 
#  fileinto "INBOX.Spam.15-20"; 
#} elsif header :matches "X-Spam-score" ["2?.?", "2?" ] {
#  fileinto "Inbox.Spam.20-30";
#} elsif header :matches "X-Spam-score" ["3?.?" , "3?", "40"] {
#fileinto "Inbox.Spam.30-40";
 }											#optimization idea  line 2/2 

#LINE 149.
	
} elsif header:contains ["Content-Type","Subject"] ["ks_c_5601-1987","euc_kr","euc-kr"]{
  fileinto "Inbox.Spam.kr";								#block Korean; it's prolly spam and I certainly can't read it.
} elsif header :contains "Received" "yale.edu" {
  fileinto "INBOX.Yale";								#if it made it past all the filters above, it's probably of interest.
      } elsif anyof (header :contains "Subject" ["HR 1910", "viagra", "MLM", "               ","	" ], # common in spam.  (prolly redundant to SpamAssassin.)
      not exists ["From", "Date"], 						#RFC822 violations common in spam.
      header :contains ["Sender", "X-Sender", "Mailing-List", "X-Apparently-From", "X-Version", "X-Sender-IP", "Received", "Return-Path", "Delivered-To", "List-Post", "Date", "Subject", "To", "Cc", "From", "Reply-to", "X-AntiAbuse", "Content-Type", "Received", "X-LinkName"] ["btamail.net.cn", "@arabia.com" ] ) {               #spam havens.
  fileinto "INBOX.GreyMail";
} elsif header :contains ["Precedence", "Priority", "X-Priority", "Mailing-List", "Subject", "From", "Received", "X-LinkName"] ["Bulk", "Newsletter"] {
  fileinto "INBOX.Bulk Precedence";
} elsif header :contains ["to", "cc", "Received"] ["IT@lastname.com", "mail.freeservers.com"] {
  fileinto "INBOX.lastname.IT";
} elsif header :contains ["To", "CC"] "Firstname@lastname.com" {
  fileinto "INBOX.lastname.non-BCC";
}
#LINE 167.
#END OF SCRIPT.  Implied 'keep' is part of the Sieve spec.





 

