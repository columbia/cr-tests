#!/usr/bin/python
#
# Open everything in a tree and check for known contents.
#
# NOTE: Does not detect if there are more files and directories
#       beyond the "original" tree if the new ones follow the
#	scheme detected here.
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
			if not access(join(dirpath, dir), R_OK|W_OK|X_OK):
				exit(EX_NOPERM)
		for fname in filenames:
			f = join(dirpath, fname)
			fd = open(f, O_RDONLY, S_IRUSR|S_IWUSR) # does access()
			expected_contents = "%s contents of %s\n" % (content, f)
			contents = read(fd, len(expected_contents))
			if read(fd, 2) != '':
				sys.stderr.write('Extra contents of file \"%s\": Got \"%s\" and more but expected \"%s\"\n' % (f, contents, expected_contents))
				sys.exit(EX_DATAERR) # not at EOF
			close(fd)
			if expected_contents == contents:
				continue
			sys.stderr.write('Contents mismatch \"%s\": Got \"%s\" but expected \"%s\"\n' % (f, contents, expected_contents))
			sys.exit(EX_DATAERR)
	sys.exit(EX_OK)
