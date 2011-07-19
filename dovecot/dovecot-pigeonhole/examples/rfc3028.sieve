#
# Example Sieve Filter
# Declare any optional features or extension used by the script
#
require ["fileinto", "reject"];

#
# Reject any large messages (note that the four leading dots get
# "stuffed" to three)
#
if size :over 1M
    {
    reject text:
Please do not send me large attachments.
Put your file on a server and send me the URL.
Thank you.
.... Fred
.
;
    stop;
    }
#

# Handle messages from known mailing lists
# Move messages from IETF filter discussion list to filter folder
#
if header :is "Sender" "owner-ietf-mta-filters@imc.org"
    {
    fileinto "filter";  # move to "filter" folder
    }
#
# Keep all messages to or from people in my company
#
elsif address :domain :is ["From", "To"] "example.com"
    {
    keep;               # keep in "In" folder
    }

#
# Try and catch unsolicited email.  If a message is not to me,
# or it contains a subject known to be spam, file it away.
#
elsif anyof (not address :all :contains
         ["To", "Cc", "Bcc"] "me@example.com",
     header :matches "subject"
         ["*make*money*fast*", "*university*dipl*mas*"])
    {
    # If message header does not contain my address,
    # it's from a list.
    fileinto "spam";   # move to "spam" folder
    }
 else
    {
    # Move all other (non-company) mail to "personal"
    # folder.
    fileinto "personal";
    }

