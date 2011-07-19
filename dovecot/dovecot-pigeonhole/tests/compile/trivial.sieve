# Commands must be case-insensitive
keep;
Keep;
KEEP;
discard;
DisCaRD;

# Tags must be case-insensitive
if size :UNDER 34 {
}

if header :Is "from" "tukker@rename-it.n" {
}

# Numbers must be case-insensitive
if anyof( size :UNDER 34m, size :oVeR 50M ) {
}
