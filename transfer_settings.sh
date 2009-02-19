#!/bin/sh

# Find the paths (different on Macs)
if [ -d "$HOME/Library/Application Support/Tremulous" ]; then
	SRCDIR="$HOME/Library/Application Support/Tremulous"
	DESTDIR="$HOME/Library/Application Support/Tremfusion"
elif [ -d "$HOME/.tremulous" ]; then
	SRCDIR="$HOME/.tremulous"
	DESTDIR="$HOME/.tremfusion"
else
	echo "Tremulous home directory could not be found."
	exit 1
fi

echo "Tremulous home directory found at $SRCDIR"
echo "Copying settings to $DESTDIR"

mkdir -p $DESTDIR
cd $SRCDIR

# Copy directory structure
echo "Copying directory structure..."
find * -type d \
	-exec mkdir $DESTDIR/{} \;

# Hard link all the read-only stuff
echo "Hard-linking large files..."
find * \( -type f -o -type l \) \
	\( -name \*.pk3 \
	-o -name \*.jpg \
	-o -name \*.tga \
	-o -name \*.svdm_\* \
	-o -name \*.dm_\* \
	-o -name \*.avi \) \
	-exec ln {} $DESTDIR/{} \;
	
# Copy the rest
echo "Copying small files..."
find * \( -type f -o -type l \) \
	! -name \*.pk3 \
	! -name \*.jpg \
	! -name \*.tga \
	! -name \*.svdm_\* \
	! -name \*.dm_\* \
	! -name \*.tmp \
	! -name \*.avi \
	-exec cp -dp {} $DESTDIR/{} \;

# Fix some setting
echo "Fixing autogen.cfg..."
sed -e 's/seta\ cl_defaultUI\ \"[^\"]*\"//' \
	-e 's/seta\ fs_extrapaks\ \"[^\"]*\"//' \
	autogen.cfg > $DESTDIR/autogen.cfg

echo "Done."
