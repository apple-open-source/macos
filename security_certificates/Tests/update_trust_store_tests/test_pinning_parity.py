#!/usr/bin/env python3
"""
Pinning Parity Test Script
Validates that paired domain suffixes in CertificatePinning.plist have consistent
labelRegex rules. Catches regressions where a regex cleanup on one suffix silently
drops coverage for the other.

With no arguments, runs all comparisons defined in DEFAULT_COMPARISONS (below).
Use --suffix-a/--suffix-b to compare an arbitrary pair instead.

Usage:
    # Run default comparisons
    python3 test_pinning_parity.py

    # Compare two arbitrary suffixes (no allowlists — full parity required)
    python3 test_pinning_parity.py --suffix-a example.com --suffix-b example.co.uk
"""

import sys
import plistlib
import argparse
from pathlib import Path


# ---------------------------------------------------------------------------
# Default comparison sets
# ---------------------------------------------------------------------------
# Each entry is a dict with:
#   suffix_a    — first domain suffix to compare
#   suffix_b    — second domain suffix to compare
#   allowed     — dict of per-policy allowlists for expected differences.
#                 Keyed by policy name (e.g. "MMCS"). Each value is a dict
#                 with optional keys:
#                   "a_only": set of terms expected only in suffix_a
#                   "b_only": set of terms expected only in suffix_b
#                 Omit a policy entirely if full parity is required for it.
#   description — human-readable label for test output
DEFAULT_COMPARISONS = [
    {
        "suffix_a":    "icloud.com",
        "suffix_b":    "icloud.com.cn",
        "allowed": {
            "MMCS": {"a_only": {"^.*contacts$", "^.*caldav$"}},
        },
        "description": "icloud.com vs icloud.com.cn",
    },
]


def load_pinning_plist(plist_path):
    """Load and return the parsed CertificatePinning.plist."""
    with open(plist_path, "rb") as f:
        return plistlib.load(f)


def extract_policies(plist_data):
    """Extract all policy dicts from the plist top-level array.

    The plist is an array whose first element is a version integer
    and the remaining elements are policy dicts.
    """
    return [entry for entry in plist_data if isinstance(entry, dict)]


def split_regex_alternatives(label_regex):
    """Split a labelRegex string on '|' into a set of individual alternatives.

    Returns an empty set for empty/missing labelRegex values.
    """
    if not label_regex:
        return set()
    return set(label_regex.split("|"))


def collect_domain_rules(policies, suffix):
    """Return a list of (policyName, alternatives_set) for every domain entry
    matching the given suffix, across all policies."""
    results = []
    for policy in policies:
        policy_name = policy.get("policyName", "<unnamed>")
        domains = policy.get("domains", [])
        for domain in domains:
            if domain.get("suffix") == suffix:
                alts = split_regex_alternatives(domain.get("labelRegex", ""))
                results.append((policy_name, alts))
    return results


def compare_suffix_pair(policies, suffix_a, suffix_b, allowed=None):
    """Compare pinning rules between two domain suffixes.

    For each policy that contains a domain entry for both suffixes, verifies
    that every regex alternative present in one also exists in the other,
    unless it appears in a per-policy allowlist.

    allowed: dict keyed by policy name. Each value is a dict with optional
    keys "a_only" (set) and "b_only" (set) for terms expected to differ.

    Returns (passed, failed, skipped) counts plus detailed failure info.
    """
    if allowed is None:
        allowed = {}

    a_rules = collect_domain_rules(policies, suffix_a)
    b_rules = collect_domain_rules(policies, suffix_b)

    # Index by policy name for matching
    a_by_name = {}
    for name, alts in a_rules:
        a_by_name.setdefault(name, []).append(alts)

    b_by_name = {}
    for name, alts in b_rules:
        b_by_name.setdefault(name, []).append(alts)

    all_policy_names = set(a_by_name.keys()) | set(b_by_name.keys())

    passed = 0
    failed = 0
    skipped = 0
    failures = []

    for policy_name in sorted(all_policy_names):
        a_list = a_by_name.get(policy_name, [])
        b_list = b_by_name.get(policy_name, [])

        if not a_list or not b_list:
            # Only one suffix present in this policy -- nothing to compare
            skipped += 1
            continue

        policy_allowed = allowed.get(policy_name, {})
        allowed_a_only = policy_allowed.get("a_only", set())
        allowed_b_only = policy_allowed.get("b_only", set())

        # Policies can have multiple domain entries per suffix (unlikely but
        # handle it). Compare pairwise in order of appearance.
        pairs = max(len(a_list), len(b_list))
        for i in range(pairs):
            a_alts = a_list[i] if i < len(a_list) else set()
            b_alts = b_list[i] if i < len(b_list) else set()

            # Terms in suffix_a but not suffix_b (minus allowed exceptions)
            a_only = (a_alts - b_alts) - allowed_a_only
            # Terms in suffix_b but not suffix_a (minus allowed exceptions)
            b_only = (b_alts - a_alts) - allowed_b_only

            if a_only or b_only:
                failed += 1
                failure = {
                    "policy": policy_name,
                    "pair_index": i,
                }
                if a_only:
                    failure["a_only"] = sorted(a_only)
                if b_only:
                    failure["b_only"] = sorted(b_only)
                failures.append(failure)
            else:
                passed += 1

    return passed, failed, skipped, failures


def run_comparison(plist_path, suffix_a, suffix_b, allowed=None, label=None):
    """Run a single suffix comparison and print results.

    Returns True if all checks passed.
    """
    if label is None:
        label = f"{suffix_a} vs {suffix_b}"

    print(f"\nComparing: {label}")
    print("=" * 60)

    plist_data = load_pinning_plist(plist_path)
    policies = extract_policies(plist_data)

    passed, failed, skipped, failures = compare_suffix_pair(
        policies, suffix_a, suffix_b, allowed
    )

    for f in failures:
        policy = f["policy"]
        print(f"  FAIL  policy={policy}")
        for term in f.get("a_only", []):
            print(f"        {suffix_a} has '{term}' but {suffix_b} does not")
        for term in f.get("b_only", []):
            print(f"        {suffix_b} has '{term}' but {suffix_a} does not")

    print(f"\n  Passed: {passed}  Failed: {failed}  Skipped (unpaired): {skipped}")
    if allowed:
        for policy_name, policy_allowed in sorted(allowed.items()):
            a_only = policy_allowed.get("a_only", set())
            b_only = policy_allowed.get("b_only", set())
            if a_only:
                print(f"  Allowed {suffix_a}-only in {policy_name}: {sorted(a_only)}")
            if b_only:
                print(f"  Allowed {suffix_b}-only in {policy_name}: {sorted(b_only)}")

    return failed == 0


def main():
    parser = argparse.ArgumentParser(
        description="Validate pinning rule parity between paired domain suffixes."
    )
    parser.add_argument(
        "--plist",
        default=str(Path(__file__).parent.parent.parent / "Pinning" / "CertificatePinning.plist"),
        help="Path to CertificatePinning.plist",
    )
    parser.add_argument(
        "--suffix-a",
        help="First domain suffix (e.g. icloud.com)",
    )
    parser.add_argument(
        "--suffix-b",
        help="Second domain suffix (e.g. icloud.com.cn)",
    )
    args = parser.parse_args()

    plist_path = args.plist
    if not Path(plist_path).exists():
        print(f"Error: {plist_path} not found")
        return False

    print("Pinning Parity Test Suite")
    print("=" * 60)
    print(f"Plist: {plist_path}")

    results = []

    if args.suffix_a and args.suffix_b:
        # Run a single user-specified comparison (no allowlists via CLI)
        ok = run_comparison(plist_path, args.suffix_a, args.suffix_b)
        results.append(ok)
    else:
        # Run all default comparisons
        for entry in DEFAULT_COMPARISONS:
            ok = run_comparison(
                plist_path,
                entry["suffix_a"],
                entry["suffix_b"],
                entry.get("allowed"),
                label=entry["description"],
            )
            results.append(ok)

    # Summary
    total = len(results)
    passed = sum(results)
    print(f"\n{'=' * 60}")
    if passed == total:
        print(f"All {total} comparison(s) passed")
        return True
    else:
        print(f"FAILED: {total - passed}/{total} comparison(s) had mismatches")
        return False


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
