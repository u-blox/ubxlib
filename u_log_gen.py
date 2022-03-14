# Simple git tool that will filter out duplicated commits caused by cherry-picks
# This is mainly intended for release notes generation
import argparse
import time
from git import Repo, Commit

parser = argparse.ArgumentParser(description='Script for generating change log from a tag without duplicated commits. This is mainly intended for release notes generation')
parser.add_argument('rev', help='Starting tag, branch or SHA')
parser.add_argument('--oneline', action='store_true', help='Only print out the first line of the commit message for each listed commit')
parser.add_argument('--branchmerge', action='store_true', help='Merges between branches are hidden by default. Set this flag to show them.')
args = parser.parse_args()

repo = Repo(".")
assert not repo.bare

def filter_commit(commit: Commit, msg_list):
    # If the commit is alread in the msg_list we return True
    if commit.message in msg_list:
        return True
    else:
        msg_list.append(commit.message)
        return False

commits = repo.iter_commits(f"{args.rev}..HEAD")
msg_list = []
commits = [c for c in commits if not filter_commit(c, msg_list)]
for commit in commits:
    if not args.branchmerge:
        if commit.message.upper().startswith("MERGE BRANCH"):
            continue
    if args.oneline:
        print(commit.message.split('\n')[0])
    else:
        print(f"commit {commit.hexsha}")
        print(f"Author: {commit.author}")
        committed_date = time.strftime("%a, %d %b %Y %H:%M", time.gmtime(commit.committed_date))
        print(f"Date:   {committed_date}")
        print()
        msg = '   ' + commit.message.replace('\n','\n   ')
        print(msg)
        print()

