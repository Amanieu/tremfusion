#!/usr/bin/env python
# based this file from http://effbot.org/librarybook/zipfile.htm
import zipfile
import glob, os
import commands
from optparse import OptionParser
from os.path import *
parser = OptionParser()
parser.add_option("-d", "--data",
                  action="store_true", dest="makedata", default=False,
                  help="make the data pk3")
parser.add_option("-v", "--verbose",
                  action="store_true", dest="verbose", default=False,
                  help="print status messages to stdout")
parser.add_option("-b", "--debug",
                  action="store_true", dest="usedebug", default=False,
                  help="use the debug build qvms")
parser.add_option("--dir", "--directory",
                  dest="directory", default=".",
                  help="the directory to place the pk3's in")
parser.add_option("-r", "--repository",
                  dest="repository", default="..",
                  help="path to the repository root folder")
parser.add_option("-s", "--symlink",
                  dest="symlink_dir", default=None,
                  help="the directory to place symlinks to the pk3's in")
parser.add_option("-g", "--game", 
                  action="store_true", dest="inc_game_qvm", default=False,
                  help="includes game.qvm in the tremfusion-game.pk3")

(options, args) = parser.parse_args()
scm_rev_num = int( commands.getoutput("hg identify -n | tr -d '+'") )

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

dir_path = abspath( options.directory )

if not exists( options.repository ):
    parser.error("directory %s does not exist" % options.repository )
    
if not isdir( options.repository ):
    parser.error("%s is not a directory" % options.repository )

if not exists( dir_path ):
    parser.error("directory %s does not exist" % options.directory )
    
if not isdir( dir_path ):
    parser.error("%s is not a directory" % options.directory )
    
if options.symlink_dir:
  
  symlink_dir = abspath( options.symlink_dir )
  if not exists( symlink_dir ):
    parser.error("directory %s does not exist" % options.symlink_dir )
  if not isdir( symlink_dir ):
    parser.error("%s is not a directory" % options.symlink_dir )
else:
  symlink_dir = None

if options.makedata:
    pk3filename = join ( dir_path, "tremfusion-data-r%04d.pk3" % scm_rev_num )
    print "Creating pk3 file at: %s" % pk3filename
    pk3 = zipfile.ZipFile(pk3filename, "w", zipfile.ZIP_DEFLATED)

    add_dir_tree(os.path.join(options.repository, "emoticons/*"), "emoticons/")
    add_dir_tree(os.path.join(options.repository, "fonts/*"), "fonts/")
    add_dir_tree(os.path.join(options.repository, "gfx/*"), "gfx/")
    add_dir_tree(os.path.join(options.repository, "models/*"), "models/")
    add_dir_tree(os.path.join(options.repository, "sound/*"), "sound/")
    add_dir_tree(os.path.join(options.repository, "ui/*"), "ui/")

    pk3.close()
    if symlink_dir:
      symlink_path = join ( symlink_dir, "tremfusion-data-r%04d.pk3" % scm_rev_num )
      print "Linking %s to %s" % ( pk3filename , symlink_path )
      if exists( symlink_path ):
        print "Removing %s" % symlink_path
        os.unlink(symlink_path)
      os.symlink(pk3filename, symlink_path)


pk3filename = join ( dir_path, "tremfusion-game-r%04d.pk3" % scm_rev_num )
print "Creating pk3 file at: %s" % pk3filename
pk3 = zipfile.ZipFile(pk3filename, "w", zipfile.ZIP_DEFLATED)

add_dir_tree(os.path.join(options.repository, "armour/*"), "armour/")
add_dir_tree(os.path.join(options.repository, "configs/*"), "configs/")
add_dir_tree(os.path.join(options.repository, "scripts/*"), "scripts/")

if options.usedebug:
  qvmglobstring = os.path.join(options.repository, "build/debug-*/base/vm/*.qvm")
else:
  qvmglobstring = os.path.join(options.repository, "build/release-*/base/vm/*.qvm")

for fspath in glob.glob(qvmglobstring):
  if os.path.isdir(fspath): continue
  pk3path = "vm/" + os.path.basename(fspath)
  # Leave out game.qvm because its not needed by the clients and wouldn't work for a server without our tremded.
  if not options.inc_game_qvm and pk3path == "vm/game.qvm":
    continue
  add_file(fspath, pk3path)

pk3.close()
if symlink_dir:
  symlink_path = join ( symlink_dir, "tremfusion-game-r%04d.pk3" % scm_rev_num )
  if exists( symlink_path ):
    print "Removing %s" % symlink_path
    os.unlink(symlink_path)
  print "Linking %s to %s" % ( pk3filename , symlink_path )
  os.symlink(pk3filename, symlink_path)

