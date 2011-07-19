# Example Sieve Script
#  Author: Matthew Johnson
#  URL: http://wiki.fastmail.fm/index.php?title=MatthewJohnson

##########################################################################
#######  SIEVE SCRIPT by Matthew Johnson - MRJ Solutions, Inc. ###########
#######  Email me at mailto:mattjohnson2005@gmail.com ##
#######  Code Version: 12JUN2004                               ###########
##########################################################################
require ["envelope", "fileinto", "reject", "vacation", "regex", "relational",
         "comparator-i;ascii-numeric"];
#
# todo:
# change to a nested format with
#   allof()s and nots.
# add "in address book" check. ex:"header :is :comparator "i;octet" "X-Spam-Known-Sender" "yes""
# finish reformating lines to <= 75 col (for web edit box)
#   and delete rulers.
# Mine Michael Klose script for ideas.
# Check out the update to the Sieve pages on the Fastmail Wiki.
#

#---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+
require ["envelope", "fileinto", "reject", "vacation", "regex",
         "relational", "comparator-i;ascii-numeric"];



# BLACKLIST - Mails to discard, drop on the floor.
#   -high spam values except those delivered to me
#   -Chinese content except for low spam values
#   -virus rejected notifications
#   -known spam addresses
#   -newsletters that refuse my removal requests
#   -twit-list
#   -double twit-list
#   -other


#---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+
if  anyof
    (
      allof       # combo test one - high spam values except for mail to/from me
      (
        # spam score is greater or equal to 14
        header :value "ge" :comparator "i;ascii-numeric"
                           ["X-Spam-score"] ["14"],
        not header :contains "X-Spam-Score" "-",  
        not header :contains "X-Spam-Score" "0.0",
        not header :contains ["to","from","cc","bcc","received"]
           [
             # do not discard email to me, will file or discard
             # as spam later if needed
             "matt@zeta.net",
             "matthew@bigsc.com",
             "matthew_johnson@bigsmallcompany.com",
             "mmm@spend.com",
             "finger@spend.com",
             "myyaacct@yahoo.com"
           ]
       ), # end allof
      allof       #combo test two - chinese content except for low spam values
      (
        anyof
        (
           header :regex "Subject"  "^=\\?(gb|GB)2312\\?",  # Chinese ecoding at subject
           header :regex "Subject"  "^=\\?big5\\?", # Other kind of  Chinese mail

           # Chinese content type
           header :contains "Content-Type"
            [
             "GB2312",
             "big5"
            ]
        ), #end anyof
        not anyof
        (
           #We have to check the sign and the value separately: ascii-numeric, defined at
           #header :contains "X-Spam-Score" "-",
           header :value "lt" :comparator "i;ascii-numeric" "X-Spam-Score" "3"
         )  #end not anyof
     ), # end allof - test two

     # single tests

     # discard fastmail virus notifications
     header :is ["subject"] ["Infected file rejected"],

     # black list, invalid addresses receiving a large amount of spam
     # or spam bounces,rejected zeta.net accounts.
     header :contains ["X-Delivered-to"]

                        ["eagleeye@zeta.net","ealgeeye@zeta.net",
                        "alica.thiele@zeta.net", "2005@theta.com",
                        "jimlovingu2@zeta.net",
                        "alpha@zeta.net",
                        "JoshuaS@zeta.net",
                        "donnaf@zeta.net",
                        "pspinks@zeta.net",
                        "jsherman@zeta.net",
                        "holly@zeta.net",
                        "clabarca@zeta.net",
                        "meghanr@zeta.net",
                        "rtaylor@zeta.net",
                        "lboone@zeta.net",
                        "brower@zeta.net",
                        "jenj@zeta.net",
                        "cbackus@zeta.net",
                        "spengles@zeta.net",
                        "adams@zeta.net",
                        "dsmith@zeta.net",
                        "jwilderman@zeta.net",
                        "TimF@zeta.net",
                        "zd@zeta.net",
                        "louise@zeta.net"]

     # single 'not' tests
     # ---out for testing---  not header :is :comparator "i;octet" "X-Spam-Known-Sender" "yes"
    ) # end anyof()
{
   discard;
   stop;
}


#
# WHITELIST - Keep these mails and put them in the inbox
#             (some kept getting put in Junk Mail)
#             Family, Friends, Current Vendors, Customers
#             Contents of fastmail address book.
#
#---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+
if  anyof (  header :contains ["from","to","cc","bcc"]
                     [ "notification@eBay.com",
                       "MAILER-DAEMON@zeta.net",
                       "USPS_Track_Confirm@usps.com",
                       "credit.services@target.com",
                       "Comcast_Paydirect@comcast.net",
                       "mary@zeta.net",
                       "betty@zeta.net",
                       "andmanymore@zeta.net"
                       ],
            header :is :comparator "i;octet" "X-Spam-Known-Sender" "yes"
          )
{
  fileinto "INBOX";
  stop;
}

# redirects
if header :contains ["to", "cc"] "mary1@zeta.net"
 {
  redirect "mary@zeta.net";
  stop;
 }


#
#   +Spam filtering by score on 3, 5 and 14(above).
#
#
if  header :value "ge" :comparator "i;ascii-numeric" ["X-Spam-score"] ["5"]  {
    fileinto "INBOX.Junk Mail.ge5";
    stop;
#---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+
} elsif  header :value "ge" :comparator "i;ascii-numeric" ["X-Spam-score"] ["3"]  {
    fileinto "INBOX.Junk Mail.ge3";
    stop;
}


# Potential Blacklist, start with soft discard, then migrate to full discard above
#
# Blacklist (2nd) During testing, throw into "Junk Mail.discard" until
#                 ready to discard.
#
if anyof
   (
    # rejects for accounts across all domains
    header :contains ["X-Delivered-to"]
                  [
                  "drjoe@","VX@",
                  "alfa@zeta.net",
                  "media@zeta.net",
                  "zeta@zeta.net",
                  "xyz@zeta.net"
                  ],

    # other criteria - weird message from this account
    header :contains ["from"] ["Charlie Root"],
    # mailers that are always sending spam returns to me
    header :contains ["from"] ["MAILER-DAEMON@aol.com"] ,
    header :contains ["from"] ["MAILER-DAEMON@otenet.gr"] ,

    # common account names that I don't use in any of my domains and that spammers like
    header :contains ["X-Delivered-to"]
                     [ "biz@","sales@","support@", "service@", "reg@",
                       "registration@", "regisration@", "root@", "webmaster@", "noreply@"
                     ],
    # zeta.net common account names to reject
    header :contains ["X-Delivered-to"] ["info@zeta.net"],
    # bigsc.com  rejects
    header :contains ["X-Delivered-to"] ["info@bigsc.com"],
    # theta.com rejects
    header :contains ["X-Delivered-to"] ["info@theta.com"],
    header :contains ["X-Delivered-to"] ["reg@theta.com"]
#---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+
        # saves for use maybe later
        #   header :contains ["X-Delivered-to"] ["webmaster@zeta.net"],
        #   header :contains ["X-Delivered-to"] ["webmaster@theta.com"],
        #   header :contains ["X-Delivered-to"] ["sales@bs.com"],
        #   header :contains ["X-Delivered-to"] ["sales@theta.com"],
        #   header :contains ["X-Delivered-to"] ["sales@bigsc.com"],
        #   header :contains ["X-Delivered-to"] "root@zeta.net",

   )   #end  anyof() 2nd blacklist
{

  fileinto "INBOX.Junk Mail.discard";
  stop;
}


#  +Greylist, move to "INBOX.Junk Mail.greylist"
#
#   'Soft' Blacklist  ?Greylist?
#

#annoying person(s) that send questionable attachments
#  look at occationally
if  header :contains "from" "alex@yahoo.com"
{
  fileinto "INBOX.Junk Mail.greylist";
} elsif  header :contains "subject" "MAILER-DAEMON@fastmail.fm"
                                     #  non-person, but might
                                     # want to look at it while
								     # figuring issues
{
  fileinto "INBOX.Junk Mail.greylist";
  stop;
}

#   +Spammy domains to filter
#
# domains that are known to be present in spam
#
if  header :contains ["from", "received"] [".ru",".jp", ".kr", ".pt",
					                     ".pl",".at",".cz",".cn",".lu" ]
{
  fileinto "INBOX.Junk Mail.discard";
  stop;
}


#
#  Annoying newsletters that won't unsubscribe me, reject
#

if anyof (
           #annoying newsletters
           header :contains ["from"] "VistaPrintNews",               # 2003
           header :contains ["from"] "newsletter@briantracyintl.com", # 2003
           header :contains ["from"] "info@yogalist.com",            # 2003
           header :contains ["from"] "The Angela Larson Real Estate Team",
           header :contains ["from"] "Brian Tracy"
         )
#---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+
{
   reject "I HAVE TRIED TO UNSUBSCRIBE; I DO NOT WANT YOUR NEWSLETTER; PLEASE UNSUBSCRIBE ME";
  stop;
}




#
# Suspected zeta.net user from/to Zeta Institute, NY - reject
#
#
#
if    header :contains ["X-Delivered-to","from"]
          [
          # aaaaNEW_ENTRIES_ABOVE  ###################################
          "neville@zeta.net",
          "animika@zeta.net",
          "linda@zeta.net",
          "jerry@zeta.net",
          "adamS@zeta.net",
          "lkdamon@zeta.net",
          "AdamS@zeta.net",
          "DConnor@zeta.net",
          "LOUISR@zeta.net",

          # Start of Alpha #############################################
          "Allanv@zeta.net",
          "AmberJ@zeta.net",
          "DANDERSON@zeta.net",
          "Jonas@zeta.net",
          "KarenE@zeta.net",
          "J.R.C.@zeta.net", # check to see if this is working
          "PMackey@zeta.net",

          "adrienne@zeta.net","alpha@zeta.net","amina@zeta.net",
          "anamika@zeta.net",
          "claborca@zeta.net","communications@zeta.net",
          "cz241@zeta.net",
          "dee@zeta.net",
          "ellenb@zeta.net","evis@zeta.net",
          "frivera@zeta.net",
          "gblack@zeta.net","gbrown@zeta.net","george@zeta.net","grace@zeta.net",
          "happygolucky@zeta.net","hsp@zeta.net",
          "ila@zeta.net",
          "jacqueline_fenatifa@zeta.net","jlengler@zeta.net",
          "joel@zeta.net","jolsen@zeta.net", "jsherman@zeta.net",
          "kronjeklandish@zeta.net","kwilcox@zeta.net","bettyb@zeta.net",
          "laurie@zeta.net","llmansell@zeta.net",
          "louise@zeta.net","lzollo@zeta.net",
          "mcraft@zeta.net","meganB@zeta.net","mwezi@zeta.net",
          "nanwile@zeta.net",
          "zetasound@zeta.net",
          "peter@zeta.net",
          "randi@zeta.net", "rcbackus@zeta.net", "registration@zeta.net",
          "registration@omgea.org",
          "rtaylor@zeta.net",
          "sdonnarumma@zeta.net","stephanR@zeta.net","suzanne@zeta.net","suzzane@zeta.net",
          "taryngaughan_dn@zeta.net"
          # zzzzEND_OF_LIST####
          ]   #end of Xdelivered-to list for possible zeta institute users

{
  reject text:
      ERROR: Your email has not been delivered.

      You have reached the mailer at zeta.net

      Perhaps you want to send to Zeta Institute in DillyDally, NY, USA?

      Use  USER@zeta.net for them

      or try registration@zeta.net
      Check the website at  http://www.zeta.net/zeta/contact/
      Call Registration at    1 800 944 1001.

      or use this information:

      Zeta Institute
      150 River Drive
      DillyDally, NY 12666
      Registration: 800-900-0000
      Ph: 845-200-0000
      Fax: 845-200-0001
      registration@zeta.net

      sincerely, POSTMASTER
.
;
  fileinto "Inbox.Junk Mail.ezeta";
  stop;
 }
#---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+
# +Move messages into folders
#
# Process other messages into separate folders
#
 # newsletters and mail lists
if  header :contains  ["subject"]
                      [ "newsletter", "[tc-ieee-", "[icntc",
                        "JUG News", "Xdesksoftware",
                        "announcement"   ]
{
  fileinto "INBOX.Newsletters";
} elsif header :contains ["from","subject"] ["Anthony Robbins"] {
  fileinto "INBOX.Newsletters";
} elsif  header :contains ["from","subject"] ["MN Entrepreneurs","ME!"]  {
  fileinto "INBOX.Newsletters";
} elsif  header :contains ["from","received"] "adc.apple.com" {
  fileinto "INBOX.Newsletters";
} elsif  header :contains "from" "wnewadmn@ieee.org" {
  fileinto "INBOX.Newsletters";
} elsif  header :contains "from" "@lb.bcentral.com" {  # techworthy@lb.bcentral.com
  fileinto "INBOX.Newsletters";
} elsif  header :contains "from" "announcement@netbriefings.com" {  #st paul company
  fileinto "INBOX.Newsletters";
} elsif  header :contains "from" "newsletter@eletters.extremetech.com" {  #semi-annoying rag
  fileinto "INBOX.Newsletters";
#---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+
# my newsletter throw-away addresses
} elsif  header :contains "to" ["microcenter@zeta.net","nmha@zeta.net"] {
  fileinto "INBOX.Newsletters";

#---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+
#
# Alerts mailbox
} elsif header :contains ["subject", "from"]
                         [
                          "Alert",                         # F-Prot virus alert service, matches:
                                                           # "FRISK Virus Alert"
                                                           #     or use s:FRISK Virus Alert:
                                                           #     or use f:support@f-prot.com
                          "Payment",                       # Alerts from other payments
                          "credit.services@target.com",    # Target Card Payments
                          "notify@quickbase.com"           # Tic Talkers Database changes
                         ]
{
  fileinto "INBOX.Alerts";
  stop;
}

# +Announcements from Dave Rolm, forward
#
# Perl Announcements from Dave Rolm
if  header :contains "from" "dave@other.org"
{
  fileinto "Inbox";
  keep;
}
#---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+
#######################################################################
#### END OF SIEVE SCRIPT by Matthew Johnson - MRJ Solutions, Inc. #####
################ email me at mailto:mattjohnson2005@gmail.com   #

