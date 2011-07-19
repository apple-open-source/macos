# Example Sieve Script
#   Author: Jerry
#   URL: http://www.emaildiscussions.com/showthread.php?postid=145322#post145322

require ["fileinto", "reject", "vacation", "regex", "relational",
"comparator-i;ascii-numeric"];


#### BLACKLIST - BOUNCE ANYTHING THAT MATCHES
#    From individual addresses
         if header :contains "from"
         [
           "username@example.com",
           "username@example.net"
         ]
         { reject "Message bounced by server content filter"; stop; }

#    From domains
         elsif header :contains "from"
         [
           "example.com",
           "example.net"
         ]
         { reject "Message bounced by server content filter"; stop; }



#### BLACKLIST - DELETE ANYTHING THAT MATCHES
#    From individual addresses
         elsif header :contains "from"
         [
           "username@example.com",
           "username@example.net"
         ]
         { discard; stop; }

#    From domains
         elsif header :contains "from"
         [
           "example.com",
           "example.net"
         ]
         { discard; stop; }

#    I just added the following section after the joe-job
#    that we all suffered at the hands of "inbox.com".
#    The "myusername" is MY username at FastMail.
#    DISCARDing this mail instead of directing it to a
#    SPAM folder kept me from going over quota repeatedly.

#    To individual addresses
         elsif header :contains "to"
         [
           "myusername@inbox.com",
           "myusername@example.net"
         ]
         { discard; stop; }

         elsif  allof
             (
                 not anyof
                 (
#### WHITELIST - KEEP ANYTHING THAT MATCHES
#    From individual addresses
                     header :contains "from"
                     [
                       "username@example.com",
                       "username@example.net"
                     ],

#    From trusted domains
                     header :contains "from"
                     [
                       "example.com",
                       "example.net"
                     ],

#    Specific "to" address (mailing lists etc)
                     header :contains ["to", "cc"]
                     [
                       "username@example.com",
                       "username@example.net"
                     ],

#    Specific "subject" keywords
                     header :contains "subject"
                     [
                       "code_word_for_friend_#1",
                       "code_word_for_friend_#2"
                     ]

                 ),
                 anyof
                 (

#    Filter by keywords in subject or from headers
                     header :contains ["subject", "from"]
                     [
                       "adilt", "adult", "advertise", "affordable",
                       "as seen on tv", "antenna", "alarm",
                       "background check", "bankrupt", "bargain",
                       "best price", "bikini", "boost reliability",
                       "brand new", "breast", "business directory",
                       "business opportunity", "based business", "best
                       deal", "bachelor's", "benefits", "cable",
                       "career", "casino", "celeb", "cheapest", "child
                       support", "cd-r", "catalog", "classified ad",
                       "click here", "coed", "classmate", "commerce",
                       "congratulations", "credit", "cruise", "cds",
                       "complimentary", "columbia house", "crushlink",
                       "debt", "detective", "diploma", "directv",
                       "directtv", "dish", "dream vacation", "deluxe",
                       "drug", "dvds", "dvd movie", "doubleclick",
                       "digital tv", "erotic", "exciting new",
                       "equalamail", "fantastic business", "fat
                       burning", "financial independence", "finalist",
                       "for life", "financing", "fitness", "fixed
                       rate", "four reports", "free!", "free
                       business", "from home", "funds", "fbi know",
                       "fortune", "gambl", "getaway", "girls", "great
                       price", "guaranteed", "get big", "get large",
                       "giveaway", "hard core", "hardcore", "home
                       document imaging", "home employment directory",
                       "homeowner", "home owner", "homeworker", "home
                       security", "home video", "immediate release",
                       "information you requested", "income",
                       "inkjet", "insurance", "interest rate",
                       "invest", "internet connection", "join price",
                       "judicial judgment", "just released", "know
                       your rights", "legal", "license", "loan", "long
                       distance", "look great", "low interest",
                       "low-interest", "low rate", "lust", "lbs",
                       "make money", "market", "master card",
                       "mastercard", "meg web", "merchant account",
                       "millionaire", "mini-vacation", "mortgage",
                       "master's", "magazine", "nasty", "new car",
                       "nigeria", "nude", "nympho", "naked",
                       "obligation", "online business", "opportunity",
                       "pager", "paying too much", "pda", "penis",
                       "pennies", "pills", "porn", "pounds",
                       "pre-approved", "prescri", "prscri", "prize",
                       "prostate", "printer ink", "quote", "refinanc",
                       "remove fat", "removing fat", "reward",
                       "sales", "satellite", "saw your site",
                       "scrambler", "sex", "smoking", "snoring", "some
                       people succeed", "special invitation", "special
                       offer", "stock", "saving", "singles", "teen",
                       "ticket", "tired of", "truth about anyone",
                       "the best", "ucking", "unbelievable",
                       "uncensored", "uncollected", "unlimited", "USA
                       domains", "urgent", "valium", "viagra",
                       "venture capital", "virgin", "visa", "vitamin",
                       "waist", "wealth", "webcam", "weight", "win a",
                       "winner", "win one", "work smarter", "work at
                       home", "xxx", "younger", "your web site", "your
                       money", "your date is wait",
                       "!!!", "$", "%", "10K"
                     ],

#    Filter when the subject is all uppercase (no lowercase)
                     header :regex :comparator
                     "i;octet" "subject" "^[^[:lower:]]+$",

#    Filter using regular expressions on the subject
                     header :regex    "subject"
                     [
                       "start.+business", "live.+auction",
                       "discover.+card", "pay.+college", "apr$",
                       "apr[^[:alnum:]]", "adv[^[:alnum:]]",
                       "free.+(coupon|info|install|money)",
                       "free.+(phone|sample|test|trial)",
                       "(buy|sell).+(house|home)"
                     ],

#    Filter with tracker codes in the subject
                     header :regex    "subject"
                     "[[:space:].\-_]{4}#?\[?[[:alnum:]-]+\]?$",

#    Filter spam with no to/from address set
                     not exists    ["To", "From"],

#    Filter spam not addressed to me
#        Put here all of your own addresses (and alias) that you expect
#        mail addressed to.  I found a lot of my spam didn't have my
#        name in the TO or CC fields at all -- it must have been in the
#        BCC (which doesn't show in the headers).  I can still get BCC
#        mail from legitimate sources because everyone in my address
#        book is on the WHITELIST above.

                     not header :contains ["to", "cc"]
                     [
                       "myusername@example.com",
                       "myusername@example.net"
                     ]

                 )
             )
         { fileinto "INBOX.1_spam"; }



#### Virus Filter
         elsif  header :contains ["subject", "from"]
         [
           "infected file rejected",
           "infected file rejected"
         ]
         { fileinto "INBOX.1_virus"; }


#### Telephone Alerts
#        Any message that gets this far should not be spam,
#        and a copy gets sent to my cell-phone as a TEXT message.

         elsif  header :contains ["to", "cc"]
         [
           "myusername@example.com",
           "myaliasname@example.com"
         ]
         { redirect "2135551234@mobile.example.net"; keep; }



# END OF SCRIPT
