require "enotify";

# 1: empty option
notify :options "" "mailto:stephan@example.org";

# 2: invalid option name syntax
notify :options "frop" "mailto:stephan@example.org";

# 3: invalid option name syntax
notify :options "_frop=" "mailto:stephan@example.org";

# 4: invalid option name syntax
notify :options "=frop" "mailto:stephan@example.org";

# 5: invalid value
notify :options "frop=frml
frop" "mailto:stephan@example.org";

