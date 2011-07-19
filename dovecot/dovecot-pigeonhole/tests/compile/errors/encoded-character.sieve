/*
 * Encoded-character errors
 *
 * Total errors: 2 (+1 = 3)
 */

require "encoded-character";
require "fileinto";

# Invalid unicode character (1)
fileinto "INBOX.${unicode:200000}";

# Not an error
fileinto "INBOX.${unicode:200000";

# Invalid unicode character (2)
fileinto "INBOX.${Unicode:DF01}";

# Not an error
fileinto "INBOX.${Unicode:DF01";



