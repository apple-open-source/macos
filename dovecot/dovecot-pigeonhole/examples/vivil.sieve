# Example Sieve Script
#   Author: Vivil
#   URL: http://wiki.fastmail.fm/index.php?title=Vivil
#   Removed unused notify require

# *************************************************************************
require ["envelope", "fileinto", "reject", "vacation", "regex", "relational", 
"comparator-i;ascii-numeric"];


if size :over 2048K {
  reject "Message not delivered; size over limit accepted by recipient";
  stop;  
}

#because of the use of elsif below, none of the "stop;"'s below are needed, but they're good 'defensive programming'. Only the one above is actually needed.

redirect "login@gmail.dom";

if header :contains ["from","cc"]
[
  "from-begin@beginbeginbeginbeginbeginbeginbeginbeginbegin.fr",
  "sex.com newsletter",
  "ad@gator.com",
  "newsletter@takecareof.com",
  "from-end@endendendendendendendendendendendendendendendend.fr"
]
{
  discard;
  stop;
}

elsif header :contains ["from"]
[
  "mygirlfriend-who-use-incredimail@foo.dom"
]
{
  fileinto "INBOX.PRIORITY";
  stop;
}

#use of "to" field detection next lines is ONLY USEFUL FOR DOMAIN NAME OWNERS if you forward your mail to your fastmail account, some virus/spam send mail to well known addresses as info@willemijns.dom i never use...

elsif header :contains ["to","cc"]
[
  "to-begin@beginbeginbeginbeginbeginbeginbeginbeginbegin.fr",
  "FTPsebastien@willemijns.dom",
  "info@willemijns.dom",
  "webmaster@willemijns.dom",
  "to-end@endendendendendendendendendendendendendendendend.fr"
]
{
  discard;
  stop;
}

elsif header :contains ["subject"]
[
  "subject-begin@beginbeginbeginbeginbeginbeginbeginbeginbegin.fr",
  "Undeliverable mail: Registration is accepted",
  "subject-end@endendendendendendendendendendendendendendendend.fr"
]
{
  discard;
  stop;
}
elsif header :value "ge" :comparator "i;ascii-numeric" ["X-Spam-score"] ["6"]  {
  fileinto "INBOX.Junk Mail";
  stop;
}
elsif header :contains "from" "reflector@launay.dom" {
  fileinto "INBOX.TEST";
  stop;
}
elsif header :contains "from" "do-not-reply@franconews.dom" {
  fileinto "INBOX.TEST";
  stop;
}
elsif header :contains "from" "devnull@news.telefonica.dom" {
  fileinto "INBOX.TEST";
  stop;
}
elsif header :contains ["to"] ["sebastien@willemijns.dom"] {
  fileinto "INBOX.PRIORITY";
  stop;
}
elsif header :contains ["to"] ["seb@willemijns.dom"] {
  fileinto "INBOX.PRIORITY";
  stop;
}
else {
  fileinto "INBOX";
}
# ********************************************************************
