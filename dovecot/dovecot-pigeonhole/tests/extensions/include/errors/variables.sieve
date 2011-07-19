require "include";
require "variables";

# Duplicate global declaration (not an error)
global "frml";
global "frml";

keep;

# Global after command not being require or global (not an error)
global "friep";

# DEPRECATED: import/export after command not being require or import/export
export "friep";
import "friep";

# Marking local variable as global
set "frutsels" "frop";
global "frutsels";
set "frutsels" "frop";



