require "notify";

# 1: empty option
notify :options "";

# 2: invalid address syntax
notify :options "frop#frop.example.org";

# Valid
notify :options "frop@frop.example.org";

