#!/usr/bin/env python
# based this file from http://effbot.org/librarybook/zipfile.htm
import zipfile
import glob, os, sys
import commands
from optparse import OptionParser
from os.path import *
parser = OptionParser()
parser.add_option("-d", "--data",
                  action="store_true", dest="makedata", default=False,
                  help="make the data pk3")
parser.add_option("-q", "--quiet",
                  action="store_false", dest="verbose", default=True,
                  help="don't print status messages to stdout")

(options, args) = parser.parse_args()
scm_rev_num = int( commands.getoutput('hg identify -n') )

def add_file(fspath, pk3path):
  if options.verbose:
    print "Putting %-45s at %s" % (fspath, pk3path)
  pk3.write(fspath, pk3path)

def add_dir_tree(globstring, pk3path):
  for fspath in glob.glob(globstring):
    if os.path.isdir(fspath):
      if options.verbose:
        print "recursing: %-42s at %s" % (fspath+"/*", join(pk3path, basename(fspath)) )
      add_dir_tree(fspath+"/*",  join(pk3path, basename(fspath)))
    else:
      add_file(fspath,  join(pk3path, basename(fspath)) )

if options.makedata:
    pk3filename = "themerge-data-r%03d.pk3" % scm_rev_num
    print "Creating pk3 file at: %s" % pk3filename
    pk3 = zipfile.ZipFile(pk3filename, "w", zipfile.ZIP_DEFLATED)

    add_dir_tree("fonts/*", "fonts/")
    add_dir_tree("gfx/*", "gfx/")
    add_dir_tree("models/*", "models/")
    add_dir_tree("sonud/*", "sound/")
    add_dir_tree("ui/*", "ui/")

    pk3.close()


pk3filename = "themerge-game-r%03d.pk3" % scm_rev_num
print "Creating pk3 file at: %s" % pk3filename
pk3 = zipfile.ZipFile(pk3filename, "w", zipfile.ZIP_DEFLATED)

add_dir_tree("armour/*", "armour/")
add_dir_tree("configs/*", "configs/")
add_dir_tree("scripts/*", "scripts/")

for fspath in glob.glob("build/release*/base/vm/*.qvm"):
  if os.path.isdir(fspath): continue
  pk3path = "vm/" + os.path.basename(fspath)
  if pk3path == "vm/game.qvm": # Leave out game.qvm because its not needed by the clients and wouldn't work for a server without our tremded.
    continue
  add_file(fspath, pk3path)

pk3.close()
