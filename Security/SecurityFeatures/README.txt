
On our targets we have different features enabled, some of our header
files are shared between the targets and installed on both targets.

This means that since they are shared there is no good way to
provide conditionals based on TARGET_OS_ macros since some of the
targets are built on both platforms, and you don't know if
you are considering building for OSX or iOS at that point
(the simulator for example).

Really, the headers needs to be unifdef'ed instead of provided
prototypes that are never defined. But that is more invasive.

Right now the switch between the headers are done in the two
Security.framework targets, which is somewhat of a cheat.

Once the two Security frameworks targets are merged into one, this
switch needs to happen on a Configuration bases.
