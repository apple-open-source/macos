/*
 * Stop command errors
 *
 * Total errors: 7 (+1 = 8)
 */

# Spurious string argument
stop "frop";

# Spurious number argument
stop 13;

# Spurious string list argument
stop [ "frop", "frop" ];

# Spurious test
stop true;

# Spurious test list
stop ( true, false );

# Spurious command block
stop {
  keep;
}

# Spurious argument and test
stop "frop" true {
  stop;
}

# Not an error
stop;
