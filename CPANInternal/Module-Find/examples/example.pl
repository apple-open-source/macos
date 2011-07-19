use Module::Find;

# use all modules in the Plugins/ directory
@found = usesub Mysoft::Plugins;

# use modules in all subdirectories
@found = useall Mysoft::Plugins;

# find all DBI::... modules
@found = findsubmod DBI;

# find anything in the CGI/ directory
@found = findallmod CGI;

# set your own search dirs (uses @INC otherwise)
setmoduledirs(@INC, @plugindirs, $appdir);