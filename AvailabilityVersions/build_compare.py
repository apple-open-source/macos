#!/usr/bin/env python3
"""
Build comparison tool for availability project.

This tool builds the project for multiple platforms using two different git revisions
and compares the outputs to ensure refactoring doesn't change behavior.
"""

import argparse
import difflib
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import List, Tuple, Dict, Optional


class BuildComparer:
    """Compares build outputs between two git revisions."""
    
    PLATFORMS = ['macos', 'ios', 'watchos', 'tvos', 'visionos', 'bridgeos']
    
    # Platform to SDK mapping for environment variables
    PLATFORM_SDKS = {
        'macos': 'macosx',
        'ios': 'iphoneos', 
        'watchos': 'watchos',
        'tvos': 'appletvos',
        'visionos': 'xros',
        'bridgeos': 'bridgeos'
    }
    
    def __init__(self, verbose: bool = False, max_diff_lines: int = 50, cleanup_on_error: bool = False, cleanup_on_success: bool = True, refactor_mode: bool = False):
        self.verbose = verbose
        self.max_diff_lines = max_diff_lines
        self.cleanup_on_error = cleanup_on_error
        self.cleanup_on_success = cleanup_on_success
        self.refactor_mode = refactor_mode
        self.temp_dir = None
        
    def log(self, message: str):
        """Log a message if verbose mode is enabled."""
        if self.verbose:
            print(f"[VERBOSE] {message}")
            
    def run_command(self, cmd: List[str], cwd: str = None, capture_output: bool = True) -> subprocess.CompletedProcess:
        """Run a command and optionally log it."""
        if self.verbose:
            print(f"[CMD] {' '.join(cmd)}")
            if cwd:
                print(f"[CWD] {cwd}")
                
        result = subprocess.run(
            cmd, 
            cwd=cwd, 
            capture_output=capture_output,
            text=True
        )
        
        if result.returncode != 0:
            print(f"Command failed: {' '.join(cmd)}")
            if result.stderr:
                print(f"Error: {result.stderr}")
            sys.exit(1)
            
        return result
        
    def get_upstream_commit(self) -> str:
        """Get the most recent commit from origin."""
        self.log("Fetching from origin...")
        self.run_command(['git', 'fetch', 'origin'])
        
        result = self.run_command(['git', 'rev-parse', 'origin/HEAD'])
        commit = result.stdout.strip()
        self.log(f"Upstream commit: {commit}")
        return commit
        
    def setup_worktrees(self, base_commit: str) -> Tuple[str, str]:
        """Set up git worktrees for comparison."""
        self.temp_dir = tempfile.mkdtemp(prefix='build_compare_', dir='/tmp')
        self.log(f"Created temp directory: {self.temp_dir}")
        
        current_dir = os.path.join(self.temp_dir, 'current')
        base_dir = os.path.join(self.temp_dir, 'base')
        
        # Create worktree for current state
        self.log("Creating worktree for current state...")
        self.run_command(['git', 'worktree', 'add', current_dir, 'HEAD'])
        
        # Create worktree for base commit
        self.log(f"Creating worktree for base commit {base_commit}...")
        self.run_command(['git', 'worktree', 'add', base_dir, base_commit])
        
        return current_dir, base_dir
        
    def build_platform(self, source_dir: str, platform: str) -> str:
        """Build the project for a specific platform."""
        build_dir = os.path.join(self.temp_dir, f'build_{platform}_{os.path.basename(source_dir)}')
        os.makedirs(build_dir, exist_ok=True)
        
        self.log(f"Building {platform} in {source_dir} -> {build_dir}")
        
        # Set up environment for platform-specific build
        env = os.environ.copy()
        if platform in self.PLATFORM_SDKS:
            env['SDKROOT'] = self.PLATFORM_SDKS[platform]
            
        # Use the Makefile to build with our custom DSTROOT
        make_args = ['make', 'install', f'DSTROOT={build_dir}']
        
        if self.verbose:
            print(f"[CMD] {' '.join(make_args)}")
            print(f"[CWD] {source_dir}")
            if 'SDKROOT' in env:
                print(f"[ENV] SDKROOT={env['SDKROOT']}")
        
        result = subprocess.run(
            make_args,
            cwd=source_dir,
            env=env,
            capture_output=not self.verbose,
            text=True
        )
        
        if result.returncode != 0:
            print(f"Build failed for {platform}")
            if result.stderr:
                print(f"Error: {result.stderr}")
            if result.stdout:
                print(f"Output: {result.stdout}")
            sys.exit(1)
            
        return build_dir
        
    def compare_directories(self, dir1: str, dir2: str, platform: str) -> bool:
        """Compare two build output directories."""
        self.log(f"Comparing {platform} outputs...")
        
        # Get all files in both directories
        files1 = self._get_all_files(dir1)
        files2 = self._get_all_files(dir2)
        
        # Check for missing/extra files
        only_in_1 = files1 - files2
        only_in_2 = files2 - files1
        common_files = files1 & files2
        
        differences_found = False
        
        if only_in_1:
            indicator = "❌" if self.refactor_mode else "ℹ️"
            print(f"{indicator} Files only in current build for {platform}:")
            for f in sorted(only_in_1):
                print(f"  + {f}")
            differences_found = True
            
        if only_in_2:
            indicator = "❌" if self.refactor_mode else "ℹ️"
            print(f"{indicator} Files only in base build for {platform}:")
            for f in sorted(only_in_2):
                print(f"  - {f}")
            differences_found = True
            
        # Compare common files
        for rel_path in sorted(common_files):
            file1 = os.path.join(dir1, rel_path)
            file2 = os.path.join(dir2, rel_path)
            
            if not self._compare_files(file1, file2, f"{platform}:{rel_path}", file1, file2):
                differences_found = True
                
        return not differences_found
        
    def _get_all_files(self, directory: str) -> set:
        """Get all files in a directory recursively."""
        files = set()
        for root, _, filenames in os.walk(directory):
            for filename in filenames:
                full_path = os.path.join(root, filename)
                rel_path = os.path.relpath(full_path, directory)
                # Skip the availability.pl script since it's the build tool wrapper
                if not rel_path.endswith('/availability.pl') and rel_path != 'availability.pl':
                    files.add(rel_path)
        return files
        
    def _compare_files(self, file1: str, file2: str, label: str, full_path1: str, full_path2: str) -> bool:
        """Compare two files and show differences if any."""
        try:
            with open(file1, 'rb') as f1, open(file2, 'rb') as f2:
                content1 = f1.read()
                content2 = f2.read()
                
            if content1 == content2:
                return True
                
            # Files differ - show full paths for inspection
            indicator = "❌" if self.refactor_mode else "ℹ️"
            print(f"\n{indicator} Differences in {label}:")
            print(f"    Base file:    {full_path2}")
            print(f"    Current file: {full_path1}")
                
            # Try to show text diff if possible
            try:
                text1 = content1.decode('utf-8').splitlines()
                text2 = content2.decode('utf-8').splitlines()
                
                diff = list(difflib.unified_diff(
                    text2, text1,
                    fromfile=f"base/{label}",
                    tofile=f"current/{label}",
                    lineterm=''
                ))
                
                if len(diff) <= self.max_diff_lines:
                    for line in diff:
                        print(line)
                else:
                    print(f"Diff has {len(diff)} lines (too large to display)")
                    print("Use --max-diff-lines to increase limit")
                    
            except UnicodeDecodeError:
                print("Binary file differs")
                
            return False
            
        except Exception as e:
            print(f"Error comparing {label}: {e}")
            return False
            
    def cleanup(self, force: bool = False):
        """Clean up temporary directories and worktrees."""
        if not force and not (self.cleanup_on_error or self.cleanup_on_success):
            return
            
        if self.temp_dir and os.path.exists(self.temp_dir):
            self.log("Cleaning up...")
            
            # Remove worktrees first, before temp directory cleanup
            for subdir in ['current', 'base']:
                worktree_path = os.path.join(self.temp_dir, subdir)
                if os.path.exists(worktree_path):
                    try:
                        # Use --force to handle modified/untracked files
                        self.run_command(['git', 'worktree', 'remove', '--force', worktree_path])
                    except:
                        # If worktree remove fails, try to clean up manually
                        try:
                            shutil.rmtree(worktree_path, ignore_errors=True)
                        except:
                            pass  # Ignore errors during cleanup
                        
            # Remove temp directory after worktrees are cleaned up
            try:
                shutil.rmtree(self.temp_dir, ignore_errors=True)
            except:
                pass  # Ignore errors during cleanup
        else:
            if self.temp_dir:
                print(f"Temp files preserved at: {self.temp_dir}")
            
    def compare_builds(self, base_commit: Optional[str] = None, platforms: Optional[List[str]] = None) -> bool:
        """Main comparison function."""
        if platforms is None:
            platforms = self.PLATFORMS
            
        if base_commit is None:
            base_commit = self.get_upstream_commit()
            
        try:
            current_dir, base_dir = self.setup_worktrees(base_commit)
            
            all_match = True
            
            for platform in platforms:
                print(f"\n=== Building and comparing {platform} ===")
                
                # Build both versions
                current_build = self.build_platform(current_dir, platform)
                base_build = self.build_platform(base_dir, platform)
                
                # Compare outputs
                if not self.compare_directories(base_build, current_build, platform):
                    all_match = False
                else:
                    print(f"{platform}: ✅ No differences found")
                    
            return all_match
            
        finally:
            # Clean up based on success/failure and user preferences
            if all_match and self.cleanup_on_success:
                self.cleanup(force=True)
            elif not all_match and self.cleanup_on_error:
                self.cleanup(force=True)
            elif not all_match and not self.cleanup_on_error:
                if self.temp_dir:
                    print(f"Temp files preserved for inspection at: {self.temp_dir}")
            else:
                self.cleanup(force=True)


def main():
    parser = argparse.ArgumentParser(
        description="Compare build outputs between git revisions",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                          # Compare current state with origin/HEAD
  %(prog)s --base-commit abc123     # Compare with specific commit
  %(prog)s --platforms macos ios    # Only test specific platforms
  %(prog)s --verbose                # Show all commands executed
        """
    )
    
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Show verbose output including all commands executed'
    )
    
    parser.add_argument(
        '--base-commit',
        help='Base commit to compare against (default: origin/HEAD)'
    )
    
    parser.add_argument(
        '--platforms',
        nargs='+',
        choices=BuildComparer.PLATFORMS,
        help=f'Platforms to test (default: all platforms)'
    )
    
    parser.add_argument(
        '--max-diff-lines',
        type=int,
        default=50,
        help='Maximum number of diff lines to display (default: 50)'
    )
    
    parser.add_argument(
        '--cleanup-on-error',
        action='store_true',
        help='Clean up temp files even when there are errors (default: keep for inspection)'
    )
    
    parser.add_argument(
        '--no-cleanup-on-success',
        action='store_true',
        help='Keep temp files even when builds match successfully'
    )
    
    parser.add_argument(
        '--refactor',
        action='store_true',
        help='Refactor mode: show error indicators (❌) for differences (default: show info indicators)'
    )
    
    args = parser.parse_args()
    
    comparer = BuildComparer(
        verbose=args.verbose,
        max_diff_lines=args.max_diff_lines,
        cleanup_on_error=args.cleanup_on_error,
        cleanup_on_success=not args.no_cleanup_on_success,
        refactor_mode=args.refactor
    )
    
    try:
        success = comparer.compare_builds(
            base_commit=args.base_commit,
            platforms=args.platforms
        )
        
        if success:
            print("\n✅ All builds match - no differences found!")
            sys.exit(0)
        else:
            if comparer.refactor_mode:
                print("\n❌ Differences found between builds")
            else:
                print("\n📋 Differences found between builds")
            sys.exit(1)
            
    except KeyboardInterrupt:
        print("\nInterrupted by user")
        comparer.cleanup(force=True)
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        if comparer.temp_dir:
            print(f"Temp files preserved for debugging at: {comparer.temp_dir}")
        sys.exit(1)


if __name__ == '__main__':
    main()
