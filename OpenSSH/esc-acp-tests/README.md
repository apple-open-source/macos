Test by first building `esc-acp` for your host, e.g.

```
$ xcodebuild -configuration Debug -arch arm64e -target esc-acp \
SDKROOT=$(xcrun --sdk macosx.internal --show-sdk-path)
```

Then use `prove .` in this directory to run all tests. If something goes
wrong, `ESC_ACP_DEBUG=-Dddd prove -v .` can be helpful.

```
% prove .
./basic.t ... ok   
./group.t ... ok   
./princs.t .. ok   
All tests successful.
Files=3, Tests=14,  1 wallclock secs ( 0.03 usr  0.02 sys +  0.65 cusr  0.40 csys =  1.10 CPU)
Result: PASS
```

Finally, the `runtests.sh` script will run `prove`, recording each invocation
of `esc-acp`. Afterward, it will re-run those invocations under the `leaks`
command and report any memory leaks.
