"""Runs the given command with an augmented path

This script is invoked like:
  python path_wrap.py --path=/path/to/resource $command
"""

import os
import subprocess
import sys

def main(args):
  assert len(args) > 1

  binpath = os.path.abspath(os.path.dirname(args[1]))

  d = os.environ.copy()
  d['PATH'] = binpath + ';' + d['PATH']
  return subprocess.call(args[1:], env=d)


if __name__ == "__main__":
  sys.exit(main(sys.argv))
