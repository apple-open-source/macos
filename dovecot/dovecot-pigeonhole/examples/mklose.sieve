# Example Sieve Script
#   Author: Michael Klose
#   URL: http://wiki.fastmail.fm/index.php?title=MichaelKloseSieveScript

require ["fileinto", "reject", "vacation", "regex", "relational", "comparator-i;ascii-numeric"];

# Experimental

# End experimental



# ----------------------------------------------
#    Discard messages (high Spam values)
# ----------------------------------------------

if anyof
    (
     allof
      (
       #Spam score > 17?
       #We have to check the sign and the value separately: ascii-numeric, defined at http://www.ietf.org/rfc/rfc2244.txt, doesn't see minus signs or decimal points ("-" or ".").
       header :value "ge" :comparator "i;ascii-numeric" "X-Spam-Score" "17",
       not header :contains "X-Spam-Score" "-",

       not header :contains ["to","cc"]
        [
         "@my-domain.de",
         "myemail@myotherdomain.us",
         "myotheremail@myotherdomain.us",
         "myotheremail2@myotherdomain.us"
         # Do not discard stuff going to me - gets filed into Junk later
        ],
       not header :contains "from"
        [
         "lockergnome.com",
         "Excite@info.excite.com" # gets filed into Junk later
        ]


      ),
     allof
      (
       header :contains "X-LinkName" "hotmail", # OR anything from Hotmail with low spam
       allof
        (
         header :value "ge" :comparator "i;ascii-numeric" "X-Spam-Score" "7",
         not header :contains "X-Spam-Score" "-"
        )
      ),

     # Black List

     header :contains "from"
      [
       "ahbbcom@cncorn.com",
       "Darg. B."
      ],

     # Chinese Encoding at BEGINNING of Subject

     allof
      (
       anyof
        (
         header :regex "Subject"  "^=\\?(gb|GB)2312\\?",  # Chinese ecoding at subject
         header :regex "Subject"  "^=\\?big5\\?", # Other kind of Chinese mail

         # Chinese content type

         header :contains "Content-Type"
          [
           "GB2312",
           "big5"
          ]
        ),
       not anyof
        (
      #Spam score > -4? <sic> - ascii-numeric ignores the ".9"!.  -Or is this correct?
       #We have to check the sign and the value separately: ascii-numeric, defined at http://www.ietf.org/rfc/rfc2244.txt, doesn't see minus signs or decimal points ("-" or ".").

         header :contains "X-Spam-Score" "-",
         header :value "lt" :comparator "i;ascii-numeric" "X-Spam-Score" "4"
        )
      )
    )

{


  # discard;

  if header :contains "X-LinkName" "hotmail"
   { discard; }
  else
   { fileinto "INBOX.Junk.Reject"; }
   # I used to reject this stuff, but I wanted to know what I was rejecting, and this stuck.
  stop;
}



# Addresses that need to be forwarded to a different domain here before spam checking
# ******************************Michael - I don't understand what you're doing here!  -elvey
# REPLY: this here is actually used to forward stuff addressed to my sister (using my domain)
# to her - without using one of the own-domain aliases.

if header :contains ["to", "cc"]
 [
  "bla@blabla.de",
  "bla2@blabla.us",
  "bla3@blabla.us"
 ]
 {
  redirect "otheremailaddress@something.com";
  redirect "anotheremailadress@something.com";
  stop;
 }


# File into a folder before Spam filtering

if header :contains ["to","cc"]
 [
  "important@mydomain.us",
  "important2@mydomain.us"
 ]
 {
  fileinto "Inbox.Important";
  stop;
 }



# -------------------------------------------
#              Filing rules
# -------------------------------------------


# Pre-SPAM


if size :over 750K
 {
  fileinto "INBOX.largemail";
  stop;
 }


if header :contains "from"
   [

# White list 1 (with SMS notification)

    "Fred Bloggs",
    "f.bloggs@hotmail.com",
    "myboss@somecompany.com",
    "Trisha",
    "endofauction@ebay.de" # I want to know about end of auctions
    ]
 {
  fileinto "Inbox";

  # Send an SMS
  redirect "smsgateway@somegateway.de";
  keep;

  stop;
 }

  # Advertising I want to receive, which normally ends up in the SPAM filter

  if anyof
   (
    header :contains "from"

     [

# Advertising whitelist

      "Mark Libbert",
      "newsletter@snapfish.dom"
     ],
    header :contains "Return-Path" "mailings@gmx.dom"
   )
   { fileinto "INBOX.Ads"; }
  elsif  header :contains "from"
   [
    "newsletter@neuseelandhaus.dom",
    "Lockergnome",
    "CNET News.com"
   ]
   { fileinto "INBOX.Newsletter";



# Spam protection


} elsif anyof
   (

    #Spam assasin
    allof
     (
      header :value "ge" :comparator "i;ascii-numeric" "X-Spam-Score" "6",
      not header :contains "X-Spam-Score" "-",
      not anyof # White list
       (
        header :contains "From"         # Whitelist From addresses
         [
          "CNN Quick News",
          "FastMail.FM Support",
          "lockergnome.com"
         ]
       )
     ),

    # User defined

    # Filter out Femalename1234z12@ spam (base64 encoded)
    allof
     (
      header :regex "From" "alpha:{2,}digit:{2,}alpha:+digit:{2,}@",
      header :contains "Content-Type" "multipart/mixed"
     ),
    # Filter our Spam with invalid headers. You can see this because FM adds
    # @fastmail.fm to them. For safty, check that mklose@ @michael-klose mkmail@gmx do
    # not appear

    # Mklose: addition: The only negative side effect I have seen of the condition below
    # is that it catches the FM newsletters. So far I find them in the spam occasionly
    # but since they are so few, I have never bothered changing this to not catch them.

    allof
     (
      header :contains "To" "@fastmail.fm", # I do not have a fastmail address   # This doesn't catch BCC's; you should be checking the envelop instead.  -elvey
      not header :contains ["To", "CC", "Reply-To"] ["klose","mkmail@gmx.dom", "chaospower"]
     )
   )
  {
   fileinto "INBOX.Junk";
   stop;
  }


# Post Spam-protection

  elsif  header :contains ["to", "cc"] "gpc@gnu.dom" {
  fileinto "INBOX.GPC";
} elsif  header :contains ["to", "cc"] "alfs\-discuss@linuxfromscratch.dom" {
  fileinto "INBOX.LFS-Support.ALFS";
} elsif  header :contains "subject" "(usagi\-users" {
  fileinto "INBOX.Usagi";
} elsif anyof (header :contains "Subject" "\[eplus-de\]", header :contains "Reply-To" "eplus-de") {
  fileinto "INBOX.E-Plus";
} elsif  header :contains ["to", "cc"] "lfs\-support@linuxfromscratch.dom" {
  fileinto "INBOX.LFS-Support";
} elsif  header :contains ["to", "cc"] "netdev@oss.sgi.dom" {
  fileinto "INBOX.NetDev";
} elsif  header :contains ["to", "cc"] "lfs\-dev@linuxfromscratch.dom" {
  fileinto "INBOX.LFS-DEV";
} elsif  header :contains "from" "GMX Best Price" {
  fileinto "INBOX.Werbung";
} elsif  header :contains "subject" "RHN Errata Alert" {
  fileinto "INBOX.Notifications";
} elsif  header :contains "from"
  [
   "EmailDiscussions.com Mailer",
   "help1@dungorm.dom"
  ] {
  fileinto "INBOX.Notifications";
} elsif  header :contains "subject" "\[Gaim\-commits\]" {
  fileinto "INBOX.Notifications";
} elsif  header :contains "subject" "\[Bug" {
  fileinto "INBOX.Notifications.Bugzilla";
} elsif header :contains "X-LinkName" "hotmail" {
  fileinto "INBOX.Old Hotmail.new";
}


# -----------------------------------------------------------------------
#               SMS notifications and forwarding
# -----------------------------------------------------------------------

if allof
    (
     header :contains "to" ["@mydomain1.de","email@mydomain2.us","email2@somedomain"],
     not header :contains "from"
      [

# This avoids sending SMS notifications if I am the sender

       "@mydomain1.de",
       "myotheremail@somedomain.de",
       "myotheremail@someotherdomain.de"
      ]
    )
 {
  redirect "smsgateway@somegateway.com";
  keep;
 }

