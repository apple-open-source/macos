package # Hide from PAUSE
    MyClass::Child;

use base 'MyClass::Parent';

sub child_method { return 'child_method'; }

1;
