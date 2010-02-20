#!/usr/bin/python
#
# fill a tree with known contents.
#
import optparse
import sys

# for S_I* (e.g. S_IRWXU)
from stat import *

# For walk, path.join, open, unlink, close, and O_*
from os import *

# Root of the tree to check
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
			action="store", help="root of tree to hold and unlink")
	parser.add_option("-c", "--content-tag", dest="content", type="string",
			default="dummy", action="store",
			help="tag for contents of the files (e.g. \"original\", \"dummy\")")
	(options, args) = parser.parse_args()
	root = path.abspath(options.root)
	content = options.content

	for (dirpath, dirnames, filenames) in rwalk():
		for dir in dirnames:
			chmod(join(dirpath, dir), S_IRWXU)
		for fname in filenames:
			f = join(dirpath, fname)
			chmod(f, S_IRUSR|S_IWUSR)
			fd = open(f, O_WRONLY|O_TRUNC)
			write(fd, "%s contents of %s\n" % (content, f))
			close(fd)
