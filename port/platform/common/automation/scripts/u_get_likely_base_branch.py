'''This script will find the most likely base branch of a commit.'''
from argparse import ArgumentParser
from invoke import context

def main(args):
    '''Script entrypoint'''
    parser = ArgumentParser(description=
        "This script will find the most likely base branch of a commit.")
    parser.add_argument('--rev', nargs='?', default="HEAD",
                        help='the change rev (SHA, branch or "HEAD")')
    parser.add_argument("branches", nargs='*',
                        help="the branches to check")
    args = parser.parse_args()
    ctx = context.Context()
    min_commit_count = 1000000
    likely_branch = ""
    # Find the base branch with the least commit diff
    for branch in args.branches:
        try:
            commit_count = \
                int(ctx.run(f"git rev-list --count {branch}..{args.rev}", hide=True).stdout)
            if commit_count < min_commit_count:
                min_commit_count = commit_count
                likely_branch = branch
        except Exception:
            pass
    # Output the resulting branch
    print(likely_branch)

if __name__ == '__main__':
    import sys
    main(sys.argv[1:])
