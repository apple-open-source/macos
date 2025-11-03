# `Sudo` Documentation and Tests

This directory contains documentation, tests and test products for `sudo`.

## Feature Files

Each feature file, `*.feature` describe behavior of a specific feature or its part.
The described behavior isn't always user-facing.
Some tests depend on external specification, e.g. UI specification.
This should be indicated and the related resources should be linked if available.

The feature files are written in [`Gherkin`](https://cucumber.io/docs/gherkin/reference/) language and follow [conventions](https://quip-apple.com/0F4lAbzxPqIZ) set in our team to remain interoperable and maintainable.

### Quick Intro to Feature Files Development

Feature files originate from behavior-driven development (BDD).
BDD encourages collaboration across roles to build shared understanding of a problem to be solved.
Developing feature files produces system documentation that is easily maintainable and both human- and machine-readable.
Treat other readers as you would want to be treated.

Each feature file has to follow this structure:

```gherkin
Feature: Feature name
    Optional multiline description or related information

    Scenario: Short description of specific behavior
        Given state before the behavior starts
        When the behavior that is being described
        Then the expected changes caused by the specified behavior
```

When writing the feature files, please follow the [conventions we set](https://quip-apple.com/0F4lAbzxPqIZ).
The conventions document provides guidance on writing feature files in compact form.

To contact us regarding content of this folder, please reach out to [security-services-qa@group.apple.com](mailto:security-services-qa@group.apple.com).

## Resources
- [AE intro to Gherkin](https://quip-apple.com/1U3LATDB6PAg)
- [Feature Files in AE](https://quip-apple.com/0F4lAbzxPqIZ)

