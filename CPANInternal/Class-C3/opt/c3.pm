
## OPTIONAL MODULE
# this module is supplied simply the use of this module 
# more aesthetically pleasing (at least to me), I think 
# it is much nicer to see:
# 
# use c3;
# 
# then to see a bunch of:
#
# use Class::C3;
# 
# all over the place.

package # ignore me PAUSE
    c3;

BEGIN { 
    use Class::C3;
    *{'c3::'} =  *{'Class::C3::'};
}

1;