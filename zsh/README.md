Z Shell
=======

To upgrade to a new version of zsh:

1. Checkout `dev/UPSTREAM`.
1. Replace its contents with the new version.
1. Rename `zsh/.gitignore` to `zsh/.gitignore.dist`.
1. Commit and push the new content.
1. Create an eng/ branch.
1. `git merge dev/UPSTREAM` into your branch and resolve any conflicts.
1. Review, test, and nominate that eng/ branch.
