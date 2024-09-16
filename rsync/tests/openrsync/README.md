Edit conf.sh to your liking.

Run like this:

./generate-kyua && kyua test

You can also run the individual test cases like this:
./test5_symlink-kills-dir.test

Requirements:
- pkg misc/cstream is required for some modes of testing.
- perl5 for some one-liners

Makefile has some useful functions you might want to check out.
