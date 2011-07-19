/*
 * Envelope test errors
 *
 * Total errors: 2 (+1 = 3)
 */

require "envelope";

# Not an error 
if envelope :is "to" "frop@example.org" {
}

# Unknown envelope part (1)
if envelope :is "frop" "frop@example.org" {
}

# Not an error
if envelope :is ["to","from"] "frop@example.org" {
}

# Unknown envelope part (2)
if envelope :is ["to","frop"] "frop@example.org" {
}
