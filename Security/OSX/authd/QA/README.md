# Tests

This directory contains the scenarios that describe the behaviour of several 
user facing features of Auth on several platforms. The scenarios are 
described using feature files which describe the behaviour of the user and 
the system. 

## Feature Files

Each `*.feature` file contains steps on how to test a feature. 
The _feature files_ are written using _Gherkin_ syntax and follow 
[a set of conventions](https://confluence.sd.apple.com/display/SECURITYSERVICES/Feature+Files+in+AE).

## Requirements

To parse and filter the feature files we use [behave](https://github.com/behave/behave).

```bash
# Install behave for the current user
pip install --user behave

# Add a symlink to it so it can be used via `behave` 
sudo ln -s "$HOME/Library/Python/<version>/bin/behave" "/usr/local/bin/behave"
```

## Usage

There are typically two reasons why you would end up in this directory: 

- verify that existing user flows are working correctly
- update the existing user flows

### Verification

You'd like to verify that the user flows that exercice a specific feature 
are still working as expected. In this case you can use `behave` in order 
to list on stdout the scenarios that you'll like to verify:

```bash
# Parse all test scenarios
behave --dry-run --no-snippets ./features

# List on the command line all the scenarios in feature files matching the given regexp
behave --dry-run --no-snippets --include "(liveness|accessibility-pay)"

# List on the command line all the scenarios with the tag "@smoke"
behave --dry-run --no-snippets --tags=@"smoke"
```

### Contributing

You'd like to update the existing list of features and scenarios -e.g: you 
have implemented a new feature in Auth and would like to document 
the way the feature behaves from the user perspective. While you can rely 
on standard UNIX tools to do this, editors such as: VIM, 
Visual Studio Code, SublimeText and InelliJ offer syntax highlighting 
for 'Gherkin', which may help you write these files faster while you 
are becoming familiar with the syntax. Please follow 
[our conventions](https://confluence.sd.apple.com/display/SECURITYSERVICES/Feature+Files+in+AE).

## Resources
- [AE intro to Gherkin](https://quip-apple.com/1U3LATDB6PAg)
- [Behave](https://behave.readthedocs.io/en/latest/)
