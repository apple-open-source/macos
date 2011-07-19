# Example Sieve Script
#   Author: SanjaySheth
#   URL: http://wiki.fastmail.fm/index.php?title=SanjaySieveSpamFilter

require "fileinto";

if anyof (

      # Blacklisted sender domains
      header :contains ["from", "Received", "X-Sender", "Sender",
                        "To","CC","Subject","X-Mail-from"]
             [ "123greetings", "allfreewebsite.com",
               "new-fields.com","atlasrewards","azogle.com",
               "bannerport.net","bettingextreme.com","bigemailoffers.com",
               "BlingMail.com",
               "beyondoffers.net", ".biz ", ".biz]",
               "cavalrymail.com","ciol.com","citywire.co.uk",
               "cosmicclick.com",
               "consumergamblingreport","creativemailoffers.com","creativeoffers.com",
               "daily-promotions.com",
               "dailypromo.","dailypromotions.",
               "dandyoffers","dlbdirect",
               "e54.org",  "email-specials.net","email-ware.com","emailoffersondemand",
               "emailbargain.com","emailofferz","emailrewardz","etoll.net","emailvalues.com",
               "evaluemarketing.com","exitrequest.com",
               "fantastic-bargain.com","fpsamplesmail.com","freelotto",
               "findtv.com", "freddysfabulousfinds.com",
               "genuinerewards.com",
               "hotdailydeal.com","hulamediamail","hy-e.net",
               "inboxbargains.com","idealemail.com",
               "jackpot.com","jpmailer.com",
               "lolita","lund.com.br",
               "mafgroup.com","mailasia.com","mailtonic.net","migada.com","ms83.com",
               "nationaloffers.com","nexdeals.com ",
               "offercatch.com","offermagnet.com","offerservice.net","offertime.com",
               "offersdaily.net","optnetwork.net",
               "ombramarketing.com","on-line-offers.com","outblaze.com",
               "permissionpass","primetimedirect.net","productsontheweb.net",
               "rapid-e.net","recessionspecials", "redmoss","remit2india",
               "sampleoffers.com","savingsmansion.com","sendoutmail.com","simpleoffers.com",
               "specialdailydeals4u.com","Select-Point.net",
               "speedyvalues.com","sportsoffers","sporttime.info","suntekglobal.com",
               "superstorespecials.com", "synapseconnect","sunsetterawnings.com",
               "thefreesamplenews","truemail.net",
               "ub-kool","ultimatesports.info","uniquemailoffers","utopiad.com",
               "unixlovers.net",
               "valuesdirect","virtualoffers.net",
               "wagerzine", "webdpoffrz",
               "yestshirt.com",
               "z-offer.com", "zipido.com"
             ],

      # Blacklisted ip subnets due to excessive spam from them
      header :contains "Received"
             [ "[4.63.221.224",
               "[24.244.141.112",
               "[61.171.253.177",
               "[63.123.149.", "[63.209.206.", "(63.233.30.73", "[63.251.200.",
               "[64.41.183.","[64.49.250.", "[64.57.188.", "[64.57.221.",
               "[64.62.204.",
               "[64.70.17.", "[64.70.44.", "[64.70.53.",
               "[64.39.27.6", "[64.39.27.7","[64.191.25.","[64.191.36.",
               "[64.191.9.",
               "[64.125.181.", "[64.191.123.", "[64.191.23.", "[64.239.182.",
               "[65.211.3.",
               "[66.46.150.", "[66.62.162.", "[66.118.170.", "[66.129.124.",
               "[66.205.217.", "[66.216.111.", "[66.239.204.",
               "[67.86.69.",
               "[80.34.206.", "[80.80.98.",
               "[81.72.233.13",
               "[128.242.120.",
               "[157.238.18",
               "[168.234.195.18]",
               "[193.253.198.57",
               "[194.25.83.1",
               "[200.24.129.", "[200.161.203.",
               "[202.164.182.76]","[202.57.69.116",
               "[203.19.220.","[203.22.104.","[203.22.105.",
               "[204.188.52.",
               "[205.153.154.203",
               "[206.26.195.", "[206.154.33.","[206.169.178",
               "[207.142.3.",
               "[208.46.5.","[208.187.",
               "[209.164.27.","[209.236.",
               "[210.90.75.129]",
               "[211.101.138.199","[211.185.7.125]","[211.239.231.",
               "[212.240.95.",
               "[213.47.250.139", "[213.225.61.",
               "[216.22.79.","[216.39.115.","[216.99.240.",
               "[216.126.32.", "[216.187.123.","[217.36.124.53",
               "[218.145.25","[218.52.71.103","[218.158.136.115",
               "[218.160.42.74", "[218.242.112.4]"
             ],

      # Blacklisted SpamAssassin flags
      header :contains ["SPAM", "X-Spam-hits"]
             ["ADDRESSES_ON_CD","ACT_NOW","ADULT_SITE", "ALL_CAP_PORN",
              "AMATEUR_PORN", "AS_SEEN_ON",
              "BAD_CREDIT", "BALANCE_FOR_LONG_20K", "BARELY_LEGAL", "BEEN_TURNED_DOWN",
              "BANG_GUARANTEE", "BANG_MONEY","BASE64_ENC_TEXT",
              "BAYES_99","BAYES_90",
              "BE_BOSS", "BEST_PORN", "BULK_EMAIL",
              "CASINO", "CONSOLIDATE_DEBT", "COPY_ACCURATELY", "COPY_DVD",
              "DIET", "DO_IT_TODAY","DOMAIN_4U2",
              "EMAIL_MARKETING","EMAIL_ROT13", "EXPECT_TO_EARN","EARN_MONEY",
              "FIND_ANYTHING", "FORGED_AOL_RCVD",
              "FORGED_HOTMAIL_RCVD", "FORGED_YAHOO_RCVD",
              "FORGED_RCVD_TRAIL", "FORGED_JUNO_RCVD",
              "FORGED_MUA_",
              "FREE_MONEY","FREE_PORN",
              "GENTLE_FEROCITY", "GET_PAID", "GUARANTEED_STUFF", "GUARANTEED_100_PERCENT",
              "HAIR_LOSS", "HIDDEN_ASSETS", "HGH,", "HOME_EMPLOYMENT","HOT_NASTY","HTTP_ESCAPED_HOST",
              "HTTP_USERNAME_USED","HTML_FONT_INVISIBLE",
              "IMPOTENCE","INVALID_MSGID","INVESTMENT",
              "LESBIAN","LIVE_PORN","LOSE_POUNDS",
              "MARKETING_PARTNERS", "MORTGAGE_OBFU", "MORTGAGE_RATES",
              "NIGERIAN_SCAM", "NIGERIAN_TRANSACTION_1", "NIGERIAN_BODY", "NUMERIC_HTTP_ADDR",
              "NO_MX_FOR_FROM","NO_DNS_FOR_FROM",
              "OBFUSCATING_COMMENT", "ONLINE_PHARMACY",
              "PENIS_ENLARGE",
              "PREST_NON_ACCREDITED", "PURE_PROFIT","PORN_4",
              "RCVD_IN_DSBL", "RCVD_IN_OSIRUSOFT_COM","RCVD_IN_BL_SPAMCOP_NET", "RCVD_IN_SBL",
              "RCVD_IN_MULTIHOP_DSBL", "RCVD_IN_RELAYS_ORDB_ORG", "RCVD_IN_UNCONFIRMED_DSBL",
              "RCVD_FAKE_HELO_DOTCOM", "RCVD_IN_RFCI", "RCVD_IN_NJABL","RCVD_IN_SORBS",
              "REFINANCE", "REVERSE_AGING",
              "SAVE_ON_INSURANCE","SPAM_REDIRECTOR", "STOCK_ALERT", "STOCK_PICK", "STRONG_BUY",
              "SEE_FOR_YOURSELF", "SUPPLIES_LIMITED",
              "THE_BEST_RATE","TONER",
              "UNSECURED_CREDIT",
              "VACATION_SCAM", "VIAGRA", "VJESTIKA",
              "WHILE_SUPPLIES", "WORK_AT_HOME",
              "X_OSIRU_DUL", "X_OSIRU_SPAMWARE_SITE", "X_OSIRU_SPAM_SRC"
             ],


      # Blacklisted subjects

      header :contains ["From","Subject"]
             [" penis ",
              "ADV:", "adult dvd", "adult movie", "adultdirect", "adultemail",
              "background check", "bankrupt", "boobs", "business opportunity","big@boss.com",
              "casino", "cash guarantee",
              "debt free", "diet bread", "ebay secrets", "erection",
              "financial freedom", "free credit",
              "gambl", "gov grants", "jackpot",
              "life insurance", "lottery", "lotto",
              "mortgage", "nude", "OTCBB",
              "penis", "porn", "promotion", "proven System",
              " rape ",
              " sex ", "skin resurfacing", "special offer",
              "ultimate software", "viagra", "V1AGRA", "vivatrim",
              "win money","work from home", "xxx"
             ],

      # often spam emails to multiple addresses with same name & different domain
      header :matches ["To","CC"]
             ["*fastmail*fastmail*fastmail*fastmail*fastmail*"],

      # Almost all emails from these domains is spam (at least for me)
      header :contains ["from", "received"]
                       [".ru ",".jp ", ".kr ", ".pt ",".pl ",".at ",".cz ",
                        ".ru>",".jp>", ".kr>", ".pt>", ".pl>",".at>",".cz>"],

      # Really high SpamAssassin scores (15.0+)
      header :matches ["X-Spam-score","X-Remote-Spam-score"] [
          "1?.?", "2?.?", "3?.?", "4?.?", "5?.?", "6?.?"     # 10.0 to 69.9
      ]
) {
      fileinto "INBOX.Spam.discard";
      stop;
}
