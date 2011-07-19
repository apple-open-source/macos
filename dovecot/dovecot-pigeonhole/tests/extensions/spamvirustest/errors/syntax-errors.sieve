require "spamtest";
require "virustest";

# Value not a string
if spamtest 3 {
}

# Value not a string
if virustest 3 {
}

# Missing value argument
if spamtest :matches :comparator "i;ascii-casemap" {
}

# Inappropriate :percent argument
if spamtest :percent "3" {
}

