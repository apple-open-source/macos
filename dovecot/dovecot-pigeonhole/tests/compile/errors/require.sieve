/*
 * Require errors
 *
 * Total errors: 11 (+1 = 12)
 */

# Not an error
require "fileinto";

# Missing argument
require;

# Too many arguments
require "fileinto" "vacation";

# Invalid argument
require 45;

# Invalid extensions (3 errors)
require ["_frop", "_friep", "_frml"];

# Core commands required
require ["redirect", "keep", "discard"];

# Invalid arguments
require "dovecot.test" true;

# Invalid extension
require "_frop";

# Spurious command block
require "fileinto" { 
  keep;
}

# Nested require
if true {
  require "relional";
}

# Require after other command than require
require "copy";
