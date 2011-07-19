require "variables";

# Invalid variable name
set "${frop}" "frop";
set "...." "frop";
set "name." "frop";
set ".name" "frop";

# Not an error
set "\n\a\m\e" "frop";

# Trying to assign match variable;
set "0" "frop";

# Not an error
set :UPPER "name" "frop";

# Invalid tag
set :inner "name" "frop";
