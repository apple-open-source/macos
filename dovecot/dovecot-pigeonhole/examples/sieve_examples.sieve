# Example Sieve Script
#   Author: unknown
#   URL: http://wiki.fastmail.fm/index.php?title=MoreSieveExamples

require ["fileinto", "reject"];

###BYPASSES###

if anyof (
              header :contains ["From"] "friend1",
              header :contains ["From"] "friend12",
              header :contains ["From"] "friend3",
              header :contains ["From"] "friendsdomanin",
              header :contains ["Subject"] "elephant"  ##a safeword
         )
             {
                   fileinto "INBOX";
                   stop;
             }

###BIG MESSAGE PROTECTION
if size :over 5000K {
         reject "Message over 5MB size limit.  Please contact me before sending this.";
}

##SPAM FILTERING##
if header :contains ["X-Spam"] "high" {
      discard;
      stop;
}
if header :contains ["X-Spam-Flag"] "HIGH" {
      discard;
      stop;
}
if header :contains ["X-Spam"] "spam" {
      fileinto "INBOX.spam";  #emails forwarded from my unviersity account get SA tagged like this
      stop;
}
if header :contains ["X-Spam-Flag"] "YES" {
      fileinto "INBOX.spam";
      stop;
}

####LOCAL SPAM RULES#######
if header :contains ["From"]  "bannerport" { discard; stop; }  ##keyword filters for when SA doesn't quite catch them
if header :contains ["To"]  "MATT NOONE" { discard; stop; }
###AUTO management rules###

####Student Digest stuff#### ###   Examples of boolean OR rules
if anyof (
            header :contains ["X-BeenThere"] "student-digest@list.xxx.edu",
            header :contains ["X-BeenThere"] "firstyear-digest@list.xxx.edu",
            header :contains ["X-BeenThere"] "secondyear-digest@list.xxx.edu",
            header :contains ["X-BeenThere"] "thirdyear-digest@list.xxx.edu",
            header :contains ["X-BeenThere"] "fourthyear-digest@list.xxx.edu"
         )
         {
            fileinto "INBOX.lists.digests";
            stop;
         }
if allof (   ###A Boolean AND rule
            header :contains ["From"] "buddy1",
            header :contains ["To"]   "myotheraddress"
         )
         {
            fileinto "INBOX.scc.annoy";
            stop;
         }

#other local rules
if header :contains ["Subject"]  "helmreich" { fileinto "INBOX.lists.helmreich"; stop; }
if header :contains ["Subject"]  "helmcomm" { fileinto "INBOX.lists.helmreich"; stop; }
if header :contains ["Subject"]  "packeteer" { fileinto "INBOX.lists"; stop; }
