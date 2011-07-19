require "enotify";

# 1: Invalid character in to part
notify "mailto:stephan@example.org;?header=frop";

# 2: Invalid character in hname
notify "mailto:stephan@example.org?header<=frop";

# 3: Invalid character in hvalue
notify "mailto:stephan@example.org?header=fr>op";

# 4: Invalid header name 
notify "mailto:stephan@example.org?header:=frop";

# 5: Invalid recipient
notify "mailto:stephan%23example.org";

# 6: Invalid to header recipient
notify "mailto:stephan@example.org?to=nico%23frop.example.org";

