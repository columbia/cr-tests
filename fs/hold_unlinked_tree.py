#!/usr/bin/python
#
# Open everything in a tree, unlink the tree, and optional create new
# things where the tree once was.
#
import optparse
import sys

# for S_I* (e.g. S_IRWXU)
from stat import *

# For walk, path.join, open, unlink, close, and O_*
from os import *

# List of open fds
open_fds = []

# Lists of dirs and files to "re-create"
mkdirs = []
mkfiles = []

# Root of the tree to hold
root = None

# Like os.walk() but assumes root and followlinks = False
# (which prevents likely-unwanted removal of files for the
#  purposes of this test script)
def rwalk(**kw):
	kw['followlinks'] = False
	return walk(root, **kw)

# Alias path.join -> join
def join(*args):
	return path.join(*args)

if __name__ == '__main__':
	parser = optparse.OptionParser()
	parser.add_option("-r", "--root", dest="root", type="string",
			default="./hold_root",
			action="store", help="root of tree to hold and unlink")
	parser.add_option("-n", "--new-tree", dest="mknew",
			action="store_true", help="remake tree")
	(options, args) = parser.parse_args()
	root = path.abspath(options.root)
	do_mk = options.mknew
	mkdirs.append(root)

	# Hold open fds to everything in the tree
	for (dirpath, dirnames, filenames) in rwalk():
		for fname in filenames:
			fpath = join(dirpath, fname)
			fd = open(join(dirpath, fname), O_RDONLY)
			open_fds.append(fd);
			unlink(fpath);
			# Record the files to re-create
			mkfiles.append(fpath)
		for dname in dirnames:
			fd = open(join(dirpath, dname), O_RDONLY|O_DIRECTORY)
			open_fds.append(fd);
			# Can't unlink the dirs yet
			# Record the directories to re-create, top-down
			mkdirs.append(join(dirpath, dname))

	# Unlink dirs, bottom up
	for (dirpath, dirnames, filenames) in rwalk(topdown=False):
		for dname in dirnames:
			rmdir(join(dirpath, dname))
	rmdir(root)

	if do_mk:
		# Make a new tree over the old one
		for dir in mkdirs:
			mkdir(dir, S_IRWXU)
		for f in mkfiles:
			fd = open(f, O_CREAT|O_EXCL|O_WRONLY, S_IRUSR|S_IWUSR)
			write(fd, "new contents of %s\n" % (f))
			close(fd)
	mkdirs = None
	mkfiles = None

	sys.stdout.flush()

	# Sleep until signalled, then exit. NOTE: assumes
	# O_CLOEXEC is not set on fds.
	execv("./do_ckpt", [ "./do_ckpt" ])
