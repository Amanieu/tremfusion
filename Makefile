#
# Tremulous Makefile
#
# GNU Make required
#

COMPILE_PLATFORM=$(shell uname|sed -e s/_.*//|tr '[:upper:]' '[:lower:]')

COMPILE_ARCH=$(shell uname -m | sed -e s/i.86/x86/)

ifeq ($(COMPILE_PLATFORM),sunos)
  # Solaris uname and GNU uname differ
  COMPILE_ARCH=$(shell uname -p | sed -e s/i.86/x86/)
endif
ifeq ($(COMPILE_PLATFORM),darwin)
  # Apple does some things a little differently...
  COMPILE_ARCH=$(shell uname -p | sed -e s/i.86/x86/)
endif

ifndef BUILD_CLIENT
  BUILD_CLIENT     = 1
endif
ifndef BUILD_CLIENT_SMP
  BUILD_CLIENT_SMP = 1
endif
ifndef BUILD_SERVER
  BUILD_SERVER     = 1
endif
ifndef BUILD_GAME_SO
  BUILD_GAME_SO    = 1
endif
ifndef BUILD_GAME_QVM
  BUILD_GAME_QVM   = 1
endif

# SDL 1.2 is only thread-safe on Macs
ifneq ($(PLATFORM),darwin)
  BUILD_CLIENT_SMP = 0
endif

#############################################################################
#
# If you require a different configuration from the defaults below, create a
# new file named "Makefile.local" in the same directory as this file and define
# your parameters there. This allows you to change configuration without
# causing problems with keeping up to date with the repository.
#
#############################################################################
-include Makefile.local

ifndef PLATFORM
PLATFORM=$(COMPILE_PLATFORM)
endif
export PLATFORM

ifeq ($(COMPILE_ARCH),powerpc)
  COMPILE_ARCH=ppc
endif

ifndef ARCH
ARCH=$(COMPILE_ARCH)
endif
export ARCH

ifneq ($(PLATFORM),$(COMPILE_PLATFORM))
  CROSS_COMPILING=1
else
  CROSS_COMPILING=0

  ifneq ($(ARCH),$(COMPILE_ARCH))
    CROSS_COMPILING=1
  endif
endif
export CROSS_COMPILING

ifndef COPYDIR
COPYDIR="/usr/local/games/tremulous"
endif

ifndef MOUNT_DIR
MOUNT_DIR=src
endif

ifndef BUILD_DIR
BUILD_DIR=build
endif

ifndef GENERATE_DEPENDENCIES
GENERATE_DEPENDENCIES=1
endif

ifndef USE_OPENAL
USE_OPENAL=1
endif

ifndef USE_OPENAL_DLOPEN
  ifeq ($(PLATFORM),mingw32)
    USE_OPENAL_DLOPEN=1
  else
    USE_OPENAL_DLOPEN=0
  endif
endif

ifndef USE_CURL
USE_CURL=1
endif

ifndef USE_CURL_DLOPEN
  ifeq ($(PLATFORM),mingw32)
    USE_CURL_DLOPEN=0
  else
    USE_CURL_DLOPEN=1
  endif
endif

ifndef USE_PYTHON
  USE_PYTHON=0
endif

ifndef USE_LUA
  USE_LUA=1
endif

ifndef USE_CODEC_VORBIS
USE_CODEC_VORBIS=0
endif

ifndef USE_MUMBLE
USE_MUMBLE=1
endif

ifndef USE_VOIP
USE_VOIP=1
endif

ifndef USE_INTERNAL_SPEEX
USE_INTERNAL_SPEEX=1
endif

ifndef USE_TTY_CLIENT
USE_TTY_CLIENT=0
endif

ifndef USE_LOCAL_HEADERS
USE_LOCAL_HEADERS=1
endif

ifndef BUILD_MASTER_SERVER
BUILD_MASTER_SERVER=0
endif

# Disable this on release builds
ifndef USE_SCM_VERSION
USE_SCM_VERSION=1
endif

ifndef ENABLE_SCRIPT_UI
ENABLE_SCRIPT_UI=1
endif

#############################################################################

BD=$(BUILD_DIR)/debug-$(PLATFORM)-$(ARCH)
BR=$(BUILD_DIR)/release-$(PLATFORM)-$(ARCH)
CDIR=$(MOUNT_DIR)/client
SDIR=$(MOUNT_DIR)/server
RDIR=$(MOUNT_DIR)/renderer
CMDIR=$(MOUNT_DIR)/qcommon
SDLDIR=$(MOUNT_DIR)/sdl
ASMDIR=$(MOUNT_DIR)/asm
SYSDIR=$(MOUNT_DIR)/sys
GDIR=$(MOUNT_DIR)/game
CGDIR=$(MOUNT_DIR)/cgame
NDIR=$(MOUNT_DIR)/null
UIDIR=$(MOUNT_DIR)/ui
JPDIR=$(MOUNT_DIR)/jpeg-6
PYTHONDIR=$(MOUNT_DIR)/python
LUADIR=$(MOUNT_DIR)/lua
SCRIPTDIR=$(MOUNT_DIR)/script
SPEEXDIR=$(MOUNT_DIR)/libspeex
Q3ASMDIR=$(MOUNT_DIR)/tools/asm
LBURGDIR=$(MOUNT_DIR)/tools/lcc/lburg
Q3CPPDIR=$(MOUNT_DIR)/tools/lcc/cpp
Q3LCCETCDIR=$(MOUNT_DIR)/tools/lcc/etc
Q3LCCSRCDIR=$(MOUNT_DIR)/tools/lcc/src
SDLHDIR=$(MOUNT_DIR)/SDL12
LIBSDIR=$(MOUNT_DIR)/libs
MASTERDIR=$(MOUNT_DIR)/master
TEMPDIR=/tmp

# set PKG_CONFIG_PATH to influence this, e.g.
# PKG_CONFIG_PATH=/opt/cross/i386-mingw32msvc/lib/pkgconfig
ifeq ($(shell which pkg-config > /dev/null; echo $$?),0)
  CURL_CFLAGS=$(shell pkg-config --cflags libcurl)
  CURL_LIBS=$(shell pkg-config --libs libcurl)
  OPENAL_CFLAGS=$(shell pkg-config --cflags openal)
  OPENAL_LIBS=$(shell pkg-config --libs openal)
  # FIXME: introduce CLIENT_CFLAGS
  SDL_CFLAGS=$(shell pkg-config --cflags sdl|sed 's/-Dmain=SDL_main//')
  SDL_LIBS=$(shell pkg-config --libs sdl)
endif

# version info
VERSION_NUMBER=0.0.1b3

ifeq ($(USE_SCM_VERSION),1)
  ifeq ($(wildcard .svn),.svn)
    SVN_REV=$(shell LANG=C svnversion .)
    ifneq ($(SVN_REV),)
      VERSION=$(VERSION_NUMBER)_R$(SVN_REV)
      USE_SVN=1
    endif
  endif

  # For git-svn
  ifeq ($(wildcard .git/svn/.metadata),.git/svn/.metadata)
    GIT_REV=$(shell LANG=C git-svn info | awk '$$1 == "Revision:" {print $$2; exit 0}')
    ifneq ($(GIT_REV),)
      VERSION=$(VERSION_NUMBER)_R$(GIT_REV)
      USE_GIT=1
    endif
  endif

  ifeq ($(wildcard .hg),.hg)
    HG_REV=$(shell LANG=C hg id -n)
    ifneq ($(HG_REV),)
      VERSION=$(VERSION_NUMBER)_R$(HG_REV)
      USE_HG=1
    endif
  endif
else
  VERSION=$(VERSION_NUMBER)
endif


#############################################################################
# SETUP AND BUILD -- LINUX
#############################################################################

## Defaults
LIB=lib

INSTALL=install
MKDIR=mkdir

ifeq ($(PLATFORM),linux)

  ifeq ($(ARCH),alpha)
    ARCH=axp
  else
  ifeq ($(ARCH),x86_64)
    LIB=lib64
  else
  ifeq ($(ARCH),ppc64)
    LIB=lib64
  else
  ifeq ($(ARCH),s390x)
    LIB=lib64
  endif
  endif
  endif
  endif

  BASE_CFLAGS = -Wall -fno-strict-aliasing -Wimplicit -Wstrict-prototypes \
    -pipe

  ifneq ($(USE_TTY_CLIENT),1)
    BASE_CFLAGS += -DUSE_ICON $(shell sdl-config --cflags)
  else
    BASE_CFLAGS += -DUSE_TTY_CLIENT
  endif

  ifeq ($(USE_OPENAL),1)
    BASE_CFLAGS += -DUSE_OPENAL
    ifeq ($(USE_OPENAL_DLOPEN),1)
      BASE_CFLAGS += -DUSE_OPENAL_DLOPEN
    endif
  endif

  ifeq ($(USE_CURL),1)
    BASE_CFLAGS += -DUSE_CURL
    ifeq ($(USE_CURL_DLOPEN),1)
      BASE_CFLAGS += -DUSE_CURL_DLOPEN
    endif
  endif

  ifeq ($(USE_CODEC_VORBIS),1)
    BASE_CFLAGS += -DUSE_CODEC_VORBIS
   endif
   
  ifeq ($(USE_PYTHON),1)
    BASE_CFLAGS += -DUSE_PYTHON
  endif

   ifeq ($(USE_LUA),1)
     BASE_CFLAGS += -DUSE_LUA
   endif

  OPTIMIZE = -O3 -funroll-loops -fomit-frame-pointer

  ifeq ($(ARCH),x86_64)
    OPTIMIZE = -O3 -fomit-frame-pointer -funroll-loops \
      -falign-loops=2 -falign-jumps=2 -falign-functions=2 \
      -fstrength-reduce
    # experimental x86_64 jit compiler! you need GNU as
    HAVE_VM_COMPILED = true
  else
  ifeq ($(ARCH),x86)
    OPTIMIZE = -O3 -march=i586 -fomit-frame-pointer \
      -funroll-loops -falign-loops=2 -falign-jumps=2 \
      -falign-functions=2 -fstrength-reduce
    HAVE_VM_COMPILED=true
  else
  ifeq ($(ARCH),ppc)
    BASE_CFLAGS += -maltivec
    HAVE_VM_COMPILED=false
  endif
  endif
  endif

  ifneq ($(HAVE_VM_COMPILED),true)
    BASE_CFLAGS += -DNO_VM_COMPILED
  endif

  SHLIBEXT=so
  SHLIBCFLAGS=-fPIC
  SHLIBLDFLAGS=-shared $(LDFLAGS)

  THREAD_LDFLAGS=-lpthread
  LDFLAGS=-ldl -lm

  CLIENT_LDFLAGS=

  ifneq ($(USE_TTY_CLIENT),1)
    CLIENT_LDFLAGS += $(shell sdl-config --libs) -lGL
  endif

  ifeq ($(USE_OPENAL),1)
    ifneq ($(USE_OPENAL_DLOPEN),1)
      CLIENT_LDFLAGS += -lopenal
    endif
  endif

  ifeq ($(USE_CURL),1)
    ifneq ($(USE_CURL_DLOPEN),1)
      CLIENT_LDFLAGS += -lcurl
    endif
  endif

  ifeq ($(USE_PYTHON),1)
    PYTHONLDFLAGS = $(shell python -c "import distutils.sysconfig;print distutils.sysconfig.get_config_var('LINKFORSHARED')")
    CFLAGS += -I/usr/include/python2.5
    LDFLAGS += $(PYTHONLDFLAGS)
    LDFLAGS += -lnsl  -lieee -lpthread -lutil -lpython2.5
  endif
  

  ifeq ($(USE_CODEC_VORBIS),1)
    CLIENT_LDFLAGS += -lvorbisfile -lvorbis -logg
  endif

  ifeq ($(USE_MUMBLE),1)
    CLIENT_LDFLAGS += -lrt
  endif

  ifeq ($(USE_LOCAL_HEADERS),1)
    BASE_CFLAGS += -I$(SDLHDIR)/include
  endif

  ifeq ($(ARCH),x86)
    # linux32 make ...
    BASE_CFLAGS += -m32
    LDFLAGS+=-m32
  else
  ifeq ($(ARCH),ppc64)
    BASE_CFLAGS += -m64
    LDFLAGS += -m64
  endif
  endif

  DEBUG_CFLAGS = $(BASE_CFLAGS) -g -O0
  RELEASE_CFLAGS=$(BASE_CFLAGS) -DNDEBUG $(OPTIMIZE)

else # ifeq Linux

#############################################################################
# SETUP AND BUILD -- MAC OS X
#############################################################################

ifeq ($(PLATFORM),darwin)
  HAVE_VM_COMPILED=true
  CLIENT_LDFLAGS=
  OPTIMIZE=
  
  BASE_CFLAGS = -Wall -Wimplicit -Wstrict-prototypes

  ifeq ($(ARCH),ppc)
    BASE_CFLAGS += -faltivec
    OPTIMIZE += -O3
  endif
  ifeq ($(ARCH),x86)
    OPTIMIZE += -march=prescott -mfpmath=sse
    # x86 vm will crash without -mstackrealign since MMX instructions will be
    # used no matter what and they corrupt the frame pointer in VM calls
    BASE_CFLAGS += -mstackrealign
  endif

  BASE_CFLAGS += -fno-strict-aliasing -DMACOS_X -fno-common -pipe

  ifeq ($(USE_OPENAL),1)
    BASE_CFLAGS += -DUSE_OPENAL
    ifneq ($(USE_OPENAL_DLOPEN),1)
      CLIENT_LDFLAGS += -framework OpenAL
    else
      BASE_CFLAGS += -DUSE_OPENAL_DLOPEN
    endif
  endif

  ifeq ($(USE_CURL),1)
    BASE_CFLAGS += -DUSE_CURL
    ifneq ($(USE_CURL_DLOPEN),1)
      CLIENT_LDFLAGS += -lcurl
    else
      BASE_CFLAGS += -DUSE_CURL_DLOPEN
    endif
  endif

  ifeq ($(USE_CODEC_VORBIS),1)
    BASE_CFLAGS += -DUSE_CODEC_VORBIS
    CLIENT_LDFLAGS += -lvorbisfile -lvorbis -logg
  endif

  BASE_CFLAGS += -D_THREAD_SAFE=1

  ifeq ($(USE_LOCAL_HEADERS),1)
    BASE_CFLAGS += -I$(SDLHDIR)/include
  endif

  # We copy sdlmain before ranlib'ing it so that subversion doesn't think
  #  the file has been modified by each build.
  LIBSDLMAIN=$(B)/libSDLmain.a
  LIBSDLMAINSRC=$(LIBSDIR)/macosx/libSDLmain.a
  CLIENT_LDFLAGS += -framework Cocoa -framework IOKit -framework OpenGL \
    $(LIBSDIR)/macosx/libSDL-1.2.0.dylib

  OPTIMIZE += -falign-loops=16

  ifneq ($(HAVE_VM_COMPILED),true)
    BASE_CFLAGS += -DNO_VM_COMPILED
  endif

  DEBUG_CFLAGS = $(BASE_CFLAGS) -g -O0

  RELEASE_CFLAGS=$(BASE_CFLAGS) -DNDEBUG $(OPTIMIZE)

  SHLIBEXT=dylib
  SHLIBCFLAGS=-fPIC -fno-common
  SHLIBLDFLAGS=-dynamiclib $(LDFLAGS)

  NOTSHLIBCFLAGS=-mdynamic-no-pic

  TOOLS_CFLAGS += -DMACOS_X

else # ifeq darwin


#############################################################################
# SETUP AND BUILD -- MINGW32
#############################################################################

ifeq ($(PLATFORM),mingw32)

  ifndef WINDRES
    WINDRES=windres
  endif

  ARCH=x86

  BASE_CFLAGS = -Wall -fno-strict-aliasing -Wimplicit -Wstrict-prototypes \
    -DUSE_ICON

  # Require Windows XP or later
  #
  # IPv6 support requires a header wspiapi.h to work on earlier versions of
  # windows. There is no MinGW equivalent of this header so we're forced to
  # require XP. In theory this restriction can be removed if this header is
  # obtained separately from the platform SDK. The MSVC build does not have
  # this limitation.
  BASE_CFLAGS += -DWINVER=0x501

  ifeq ($(USE_OPENAL),1)
    BASE_CFLAGS += -DUSE_OPENAL
    BASE_CFLAGS += $(OPENAL_CFLAGS)
    ifeq ($(USE_OPENAL_DLOPEN),1)
      BASE_CFLAGS += -DUSE_OPENAL_DLOPEN
    else
      CLIENT_LDFLAGS += $(OPENAL_LDFLAGS)
    endif
  endif

  ifeq ($(USE_CODEC_VORBIS),1)
    BASE_CFLAGS += -DUSE_CODEC_VORBIS
  endif

  OPTIMIZE = -O3 -march=i586 -fno-omit-frame-pointer \
    -falign-loops=2 -funroll-loops -falign-jumps=2 -falign-functions=2 \
    -fstrength-reduce

  HAVE_VM_COMPILED = true

  SHLIBEXT=dll
  SHLIBCFLAGS=
  SHLIBLDFLAGS=-shared $(LDFLAGS)

  BINEXT=.exe

  LDFLAGS= -lws2_32 -lwinmm
  CLIENT_LDFLAGS = -mwindows -lgdi32 -lole32 -lopengl32

  ifeq ($(USE_CURL),1)
    BASE_CFLAGS += -DUSE_CURL
    BASE_CFLAGS += $(CURL_CFLAGS)
    ifneq ($(USE_CURL_DLOPEN),1)
      ifeq ($(USE_LOCAL_HEADERS),1)
        BASE_CFLAGS += -DCURL_STATICLIB
        CLIENT_LDFLAGS += $(LIBSDIR)/win32/libcurl.a
      else
        CLIENT_LDFLAGS += $(CURL_LIBS)
      endif
    endif
  endif

  ifeq ($(USE_CODEC_VORBIS),1)
    CLIENT_LDFLAGS += -lvorbisfile -lvorbis -logg
  endif

  ifeq ($(ARCH),x86)
    # build 32bit
    BASE_CFLAGS += -m32
    LDFLAGS+=-m32
  endif

  DEBUG_CFLAGS=$(BASE_CFLAGS) -g -O0
  RELEASE_CFLAGS=$(BASE_CFLAGS) -DNDEBUG $(OPTIMIZE)

  # libmingw32 must be linked before libSDLmain
  CLIENT_LDFLAGS += -lmingw32
  ifeq ($(USE_LOCAL_HEADERS),1)
    BASE_CFLAGS += -I$(SDLHDIR)/include
    CLIENT_LDFLAGS += $(LIBSDIR)/win32/libSDLmain.a \
                      $(LIBSDIR)/win32/libSDL.dll.a
  else
    BASE_CFLAGS += $(SDL_CFLAGS)
    CLIENT_LDFLAGS += $(SDL_LIBS)
  endif

else # ifeq mingw32

#############################################################################
# SETUP AND BUILD -- FREEBSD
#############################################################################

ifeq ($(PLATFORM),freebsd)

  ifneq (,$(findstring alpha,$(shell uname -m)))
    ARCH=axp
  else #default to x86
    ARCH=x86
  endif #alpha test


  BASE_CFLAGS = -Wall -fno-strict-aliasing -Wimplicit -Wstrict-prototypes \
    -DUSE_ICON $(shell sdl-config --cflags)

  ifeq ($(USE_OPENAL),1)
    BASE_CFLAGS += -DUSE_OPENAL
    ifeq ($(USE_OPENAL_DLOPEN),1)
      BASE_CFLAGS += -DUSE_OPENAL_DLOPEN
    endif
  endif

  ifeq ($(USE_CODEC_VORBIS),1)
    BASE_CFLAGS += -DUSE_CODEC_VORBIS
  endif

  ifeq ($(ARCH),axp)
    BASE_CFLAGS += -DNO_VM_COMPILED
    RELEASE_CFLAGS=$(BASE_CFLAGS) -DNDEBUG -O3 -funroll-loops \
      -fomit-frame-pointer -fexpensive-optimizations
  else
  ifeq ($(ARCH),x86)
    RELEASE_CFLAGS=$(BASE_CFLAGS) -DNDEBUG -O3 -mtune=pentiumpro \
      -march=pentium -fomit-frame-pointer -pipe \
      -falign-loops=2 -falign-jumps=2 -falign-functions=2 \
      -funroll-loops -fstrength-reduce
    HAVE_VM_COMPILED=true
  else
    BASE_CFLAGS += -DNO_VM_COMPILED
  endif
  endif

  DEBUG_CFLAGS=$(BASE_CFLAGS) -g

  SHLIBEXT=so
  SHLIBCFLAGS=-fPIC
  SHLIBLDFLAGS=-shared $(LDFLAGS)

  THREAD_LDFLAGS=-lpthread
  # don't need -ldl (FreeBSD)
  LDFLAGS=-lm

  CLIENT_LDFLAGS =

  CLIENT_LDFLAGS += $(shell sdl-config --libs) -lGL

  ifeq ($(USE_OPENAL),1)
    ifneq ($(USE_OPENAL_DLOPEN),1)
      CLIENT_LDFLAGS += $(THREAD_LDFLAGS) -lopenal
    endif
  endif

  ifeq ($(USE_CODEC_VORBIS),1)
    CLIENT_LDFLAGS += -lvorbisfile -lvorbis -logg
  endif

else # ifeq freebsd

#############################################################################
# SETUP AND BUILD -- OPENBSD
#############################################################################

ifeq ($(PLATFORM),openbsd)

  #default to i386, no tests done on anything else
  ARCH=i386


  BASE_CFLAGS = -Wall -fno-strict-aliasing -Wimplicit -Wstrict-prototypes \
    -DUSE_ICON $(shell sdl-config --cflags)

  ifeq ($(USE_OPENAL),1)
    BASE_CFLAGS += -DUSE_OPENAL
    ifeq ($(USE_OPENAL_DLOPEN),1)
      BASE_CFLAGS += -DUSE_OPENAL_DLOPEN
    endif
  endif

  ifeq ($(USE_CODEC_VORBIS),1)
    BASE_CFLAGS += -DUSE_CODEC_VORBIS
  endif

  BASE_CFLAGS += -DNO_VM_COMPILED -I/usr/X11R6/include -I/usr/local/include
  RELEASE_CFLAGS=$(BASE_CFLAGS) -DNDEBUG -O3 \
    -march=pentium -fomit-frame-pointer -pipe \
    -falign-loops=2 -falign-jumps=2 -falign-functions=2 \
    -funroll-loops -fstrength-reduce
  HAVE_VM_COMPILED=false

  DEBUG_CFLAGS=$(BASE_CFLAGS) -g

  SHLIBEXT=so
  SHLIBCFLAGS=-fPIC
  SHLIBLDFLAGS=-shared $(LDFLAGS)

  THREAD_LDFLAGS=-lpthread
  LDFLAGS=-lm

  CLIENT_LDFLAGS =

  CLIENT_LDFLAGS += $(shell sdl-config --libs) -lGL

  ifeq ($(USE_OPENAL),1)
    ifneq ($(USE_OPENAL_DLOPEN),1)
      CLIENT_LDFLAGS += $(THREAD_LDFLAGS) -lossaudio -lopenal
    endif
  endif

  ifeq ($(USE_CODEC_VORBIS),1)
    CLIENT_LDFLAGS += -lvorbisfile -lvorbis -logg
  endif

else # ifeq openbsd

#############################################################################
# SETUP AND BUILD -- NETBSD
#############################################################################

ifeq ($(PLATFORM),netbsd)

  ifeq ($(shell uname -m),i386)
    ARCH=x86
  endif

  LDFLAGS=-lm
  SHLIBEXT=so
  SHLIBCFLAGS=-fPIC
  SHLIBLDFLAGS=-shared $(LDFLAGS)
  THREAD_LDFLAGS=-lpthread

  BASE_CFLAGS = -Wall -fno-strict-aliasing -Wimplicit -Wstrict-prototypes

  ifneq ($(ARCH),x86)
    BASE_CFLAGS += -DNO_VM_COMPILED
  endif

  DEBUG_CFLAGS=$(BASE_CFLAGS) -g

  BUILD_CLIENT = 0
  BUILD_GAME_QVM = 0

else # ifeq netbsd

#############################################################################
# SETUP AND BUILD -- IRIX
#############################################################################

ifeq ($(PLATFORM),irix64)

  ARCH=mips  #default to MIPS

  CC = c99
  MKDIR = mkdir -p

  BASE_CFLAGS=-Dstricmp=strcasecmp -Xcpluscomm -woff 1185 \
    -I. $(shell sdl-config --cflags) -I$(ROOT)/usr/include -DNO_VM_COMPILED
  RELEASE_CFLAGS=$(BASE_CFLAGS) -O3
  DEBUG_CFLAGS=$(BASE_CFLAGS) -g

  SHLIBEXT=so
  SHLIBCFLAGS=
  SHLIBLDFLAGS=-shared

  LDFLAGS=-ldl -lm -lgen
  # FIXME: The X libraries probably aren't necessary?
  CLIENT_LDFLAGS=-L/usr/X11/$(LIB) $(shell sdl-config --libs) -lGL \
    -lX11 -lXext -lm

else # ifeq IRIX

#############################################################################
# SETUP AND BUILD -- SunOS
#############################################################################

ifeq ($(PLATFORM),sunos)

  CC=gcc
  INSTALL=ginstall
  MKDIR=gmkdir
  COPYDIR="/usr/local/share/games/tremulous"

  ifneq (,$(findstring i86pc,$(shell uname -m)))
    ARCH=x86
  else #default to sparc
    ARCH=sparc
  endif

  ifneq ($(ARCH),x86)
    ifneq ($(ARCH),sparc)
      $(error arch $(ARCH) is currently not supported)
    endif
  endif


  BASE_CFLAGS = -Wall -fno-strict-aliasing -Wimplicit -Wstrict-prototypes \
    -pipe -DUSE_ICON $(shell sdl-config --cflags)

  OPTIMIZE = -O3 -funroll-loops

  ifeq ($(ARCH),sparc)
    OPTIMIZE = -O3 \
      -fstrength-reduce -falign-functions=2 \
      -mtune=ultrasparc3 -mv8plus -mno-faster-structs \
      -funroll-loops #-mv8plus
  else
  ifeq ($(ARCH),x86)
    OPTIMIZE = -O3 -march=i586 -fomit-frame-pointer \
      -funroll-loops -falign-loops=2 -falign-jumps=2 \
      -falign-functions=2 -fstrength-reduce
    HAVE_VM_COMPILED=true
    BASE_CFLAGS += -m32
    LDFLAGS += -m32
    BASE_CFLAGS += -I/usr/X11/include/NVIDIA
    CLIENT_LDFLAGS += -L/usr/X11/lib/NVIDIA -R/usr/X11/lib/NVIDIA
  endif
  endif

  ifneq ($(HAVE_VM_COMPILED),true)
    BASE_CFLAGS += -DNO_VM_COMPILED
  endif

  DEBUG_CFLAGS = $(BASE_CFLAGS) -ggdb -O0

  RELEASE_CFLAGS=$(BASE_CFLAGS) -DNDEBUG $(OPTIMIZE)

  SHLIBEXT=so
  SHLIBCFLAGS=-fPIC
  SHLIBLDFLAGS=-shared $(LDFLAGS)

  THREAD_LDFLAGS=-lpthread
  LDFLAGS=-lsocket -lnsl -ldl -lm

  BOTCFLAGS=-O0

  CLIENT_LDFLAGS +=$(shell sdl-config --libs) -lGL

else # ifeq sunos

#############################################################################
# SETUP AND BUILD -- GENERIC
#############################################################################
  BASE_CFLAGS=-DNO_VM_COMPILED
  DEBUG_CFLAGS=$(BASE_CFLAGS) -g
  RELEASE_CFLAGS=$(BASE_CFLAGS) -DNDEBUG -O3

  SHLIBEXT=so
  SHLIBCFLAGS=-fPIC
  SHLIBLDFLAGS=-shared

endif #Linux
endif #darwin
endif #mingw32
endif #FreeBSD
endif #OpenBSD
endif #NetBSD
endif #IRIX
endif #SunOS

TARGETS =

ifneq ($(BUILD_SERVER),0)
  TARGETS += $(B)/tremded.$(ARCH)$(BINEXT)
endif

ifneq ($(BUILD_CLIENT),0)
  TARGETS += $(B)/tremulous.$(ARCH)$(BINEXT)
  ifneq ($(BUILD_CLIENT_SMP),0)
    TARGETS += $(B)/tremulous-smp.$(ARCH)$(BINEXT)
  endif
endif

ifneq ($(BUILD_GAME_SO),0)
  TARGETS += \
    $(B)/base/cgame$(ARCH).$(SHLIBEXT) \
    $(B)/base/game$(ARCH).$(SHLIBEXT) \
    $(B)/base/ui$(ARCH).$(SHLIBEXT)
endif

ifneq ($(BUILD_GAME_QVM),0)
  ifneq ($(CROSS_COMPILING),1)
    TARGETS += \
      $(B)/base/vm/cgame.qvm \
      $(B)/base/vm/game.qvm \
      $(B)/base/vm/ui.qvm
  endif
endif

ifeq ($(USE_MUMBLE),1)
  BASE_CFLAGS += -DUSE_MUMBLE
endif

ifeq ($(USE_VOIP),1)
  BASE_CFLAGS += -DUSE_VOIP
  ifeq ($(USE_INTERNAL_SPEEX),1)
    BASE_CFLAGS += -DFLOATING_POINT -DUSE_ALLOCA -I$(SPEEXDIR)/include
  else
    CLIENT_LDFLAGS += -lspeex
  endif
endif

ifdef DEFAULT_BASEDIR
  BASE_CFLAGS += -DDEFAULT_BASEDIR=\\\"$(DEFAULT_BASEDIR)\\\"
endif

ifeq ($(USE_LOCAL_HEADERS),1)
  BASE_CFLAGS += -DUSE_LOCAL_HEADERS
endif

ifeq ($(GENERATE_DEPENDENCIES),1)
  DEPEND_CFLAGS = -MMD
else
  DEPEND_CFLAGS =
endif

BASE_CFLAGS += -DPRODUCT_VERSION=\\\"$(VERSION)\\\"

ifeq ($(USE_LUA),1)
  SHLIBCFLAGS += -DUSE_LUA=1 -I$(LUADIR)
endif

ifeq ($(ENABLE_SCRIPT_UI),1)
  CFLAGS += -DENABLE_SCRIPT_UI
endif

ifeq ($(V),1)
echo_cmd=@:
Q=
else
echo_cmd=@echo
Q=@
endif

define DO_CC
$(echo_cmd) "CC $<"
$(Q)$(CC) $(NOTSHLIBCFLAGS) $(CFLAGS) -o $@ -c $<
endef

define DO_SMP_CC
$(echo_cmd) "SMP_CC $<"
$(Q)$(CC) $(NOTSHLIBCFLAGS) $(CFLAGS) -DSMP -o $@ -c $<
endef

define DO_BOT_CC
$(echo_cmd) "BOT_CC $<"
$(Q)$(CC) $(NOTSHLIBCFLAGS) $(CFLAGS) $(BOTCFLAGS) -DBOTLIB -o $@ -c $<
endef

ifeq ($(GENERATE_DEPENDENCIES),1)
  DO_QVM_DEP=cat $(@:%.o=%.d) | sed -e 's/\.o/\.asm/g' >> $(@:%.o=%.d)
endif

define DO_SHLIB_CC
$(echo_cmd) "SHLIB_CC $<"
$(Q)$(CC) $(CFLAGS) $(SHLIBCFLAGS) -o $@ -c $<
$(Q)$(DO_QVM_DEP)
endef

define DO_GAME_CC
$(echo_cmd) "GAME_CC $<"
$(Q)$(CC) -DGAME $(CFLAGS) $(SHLIBCFLAGS) -o $@ -c $<
$(Q)$(DO_QVM_DEP)
endef

define DO_CGAME_CC
$(echo_cmd) "CGAME_CC $<"
$(Q)$(CC) -DCGAME $(CFLAGS) $(SHLIBCFLAGS) -o $@ -c $<
$(Q)$(DO_QVM_DEP)
endef

define DO_UI_CC
$(echo_cmd) "UI_CC $<"
$(Q)$(CC) -DUI $(CFLAGS) $(SHLIBCFLAGS) -o $@ -c $<
$(Q)$(DO_QVM_DEP)
endef

define DO_AS
$(echo_cmd) "AS $<"
$(Q)$(CC) $(CFLAGS) -x assembler-with-cpp -o $@ -c $<
endef

define DO_DED_CC
$(echo_cmd) "DED_CC $<"
$(Q)$(CC) $(NOTSHLIBCFLAGS) -DDEDICATED $(CFLAGS) -o $@ -c $<
endef

define DO_WINDRES
$(echo_cmd) "WINDRES $<"
$(Q)$(WINDRES) -i $< -o $@
endef


#############################################################################
# MAIN TARGETS
#############################################################################

default: release
all: debug release

debug:
	@$(MAKE) targets B=$(BD) CFLAGS="$(CFLAGS) $(DEPEND_CFLAGS) \
		$(DEBUG_CFLAGS)" V=$(V)
ifeq ($(BUILD_MASTER_SERVER),1)
	$(MAKE) -C $(MASTERDIR) debug VERSION=$(VERSION_NUMBER)
endif

release:
	@$(MAKE) targets B=$(BR) CFLAGS="$(CFLAGS) $(DEPEND_CFLAGS) \
		$(RELEASE_CFLAGS)" V=$(V)
ifeq ($(BUILD_MASTER_SERVER),1)
	$(MAKE) -C $(MASTERDIR) release VERSION=$(VERSION_NUMBER)
endif

# Create the build directories, check libraries and print out
# an informational message, then start building
targets: makedirs
	@echo ""
	@echo "Building Tremulous in $(B):"
	@echo "  PLATFORM: $(PLATFORM)"
	@echo "  ARCH: $(ARCH)"
	@echo "  VERSION: $(VERSION)"
	@echo "  COMPILE_PLATFORM: $(COMPILE_PLATFORM)"
	@echo "  COMPILE_ARCH: $(COMPILE_ARCH)"
	@echo "  CC: $(CC)"
	@echo ""
	@echo "  CFLAGS:"
	@for i in $(CFLAGS); \
	do \
		echo "    $$i"; \
	done
	@echo ""
	@echo "  LDFLAGS:"
	@for i in $(LDFLAGS); \
	do \
		echo "    $$i"; \
	done
	@echo ""
	@echo "  Output:"
	@for i in $(TARGETS); \
	do \
		echo "    $$i"; \
	done
	@echo ""
ifneq ($(TARGETS),)
	@$(MAKE) $(TARGETS) V=$(V)
endif

makedirs:
	@if [ ! -d $(BUILD_DIR) ];then $(MKDIR) $(BUILD_DIR);fi
	@if [ ! -d $(B) ];then $(MKDIR) $(B);fi
	@if [ ! -d $(B)/client ];then $(MKDIR) $(B)/client;fi
	@if [ ! -d $(B)/clientsmp ];then $(MKDIR) $(B)/clientsmp;fi
	@if [ ! -d $(B)/ded ];then $(MKDIR) $(B)/ded;fi
	@if [ ! -d $(B)/base ];then $(MKDIR) $(B)/base;fi
	@if [ ! -d $(B)/base/cgame ];then $(MKDIR) $(B)/base/cgame;fi
	@if [ ! -d $(B)/base/game ];then $(MKDIR) $(B)/base/game;fi
	@if [ ! -d $(B)/base/ui ];then $(MKDIR) $(B)/base/ui;fi
	@if [ ! -d $(B)/base/qcommon ];then $(MKDIR) $(B)/base/qcommon;fi
	@if [ ! -d $(B)/base/python ];then $(MKDIR) $(B)/base/python;fi
	@if [ ! -d $(B)/base/lua ];then $(MKDIR) $(B)/base/lua;fi
	@if [ ! -d $(B)/base/script_ui ];then $(MKDIR) $(B)/base/script_ui;fi
	@if [ ! -d $(B)/base/script_game ];then $(MKDIR) $(B)/base/script_game;fi
	@if [ ! -d $(B)/base/script_cgame ];then $(MKDIR) $(B)/base/script_cgame;fi
	@if [ ! -d $(B)/base/vm ];then $(MKDIR) $(B)/base/vm;fi
	@if [ ! -d $(B)/tools ];then $(MKDIR) $(B)/tools;fi
	@if [ ! -d $(B)/tools/asm ];then $(MKDIR) $(B)/tools/asm;fi
	@if [ ! -d $(B)/tools/etc ];then $(MKDIR) $(B)/tools/etc;fi
	@if [ ! -d $(B)/tools/rcc ];then $(MKDIR) $(B)/tools/rcc;fi
	@if [ ! -d $(B)/tools/cpp ];then $(MKDIR) $(B)/tools/cpp;fi
	@if [ ! -d $(B)/tools/lburg ];then $(MKDIR) $(B)/tools/lburg;fi

#############################################################################
# QVM BUILD TOOLS
#############################################################################

TOOLS_OPTIMIZE = -g -O2 -Wall -fno-strict-aliasing
TOOLS_CFLAGS = $(TOOLS_OPTIMIZE) \
               -DTEMPDIR=\"$(TEMPDIR)\" -DSYSTEM=\"\" \
               -I$(Q3LCCSRCDIR) \
               -I$(LBURGDIR)
TOOLS_LDFLAGS =

ifeq ($(GENERATE_DEPENDENCIES),1)
	TOOLS_CFLAGS += -MMD
endif

define DO_TOOLS_CC
$(echo_cmd) "TOOLS_CC $<"
$(Q)$(CC) $(TOOLS_CFLAGS) -o $@ -c $<
endef

define DO_TOOLS_CC_DAGCHECK
$(echo_cmd) "TOOLS_CC_DAGCHECK $<"
$(Q)$(CC) $(TOOLS_CFLAGS) -Wno-unused -o $@ -c $<
endef

LBURG       = $(B)/tools/lburg/lburg$(BINEXT)
DAGCHECK_C  = $(B)/tools/rcc/dagcheck.c
Q3RCC       = $(B)/tools/q3rcc$(BINEXT)
Q3CPP       = $(B)/tools/q3cpp$(BINEXT)
Q3LCC       = $(B)/tools/q3lcc$(BINEXT)
Q3ASM       = $(B)/tools/q3asm$(BINEXT)

LBURGOBJ= \
	$(B)/tools/lburg/lburg.o \
	$(B)/tools/lburg/gram.o

$(B)/tools/lburg/%.o: $(LBURGDIR)/%.c
	$(DO_TOOLS_CC)

$(LBURG): $(LBURGOBJ)
	$(echo_cmd) "LD $@"
	$(Q)$(CC) $(TOOLS_LDFLAGS) -o $@ $^

Q3RCCOBJ = \
  $(B)/tools/rcc/alloc.o \
  $(B)/tools/rcc/bind.o \
  $(B)/tools/rcc/bytecode.o \
  $(B)/tools/rcc/dag.o \
  $(B)/tools/rcc/dagcheck.o \
  $(B)/tools/rcc/decl.o \
  $(B)/tools/rcc/enode.o \
  $(B)/tools/rcc/error.o \
  $(B)/tools/rcc/event.o \
  $(B)/tools/rcc/expr.o \
  $(B)/tools/rcc/gen.o \
  $(B)/tools/rcc/init.o \
  $(B)/tools/rcc/inits.o \
  $(B)/tools/rcc/input.o \
  $(B)/tools/rcc/lex.o \
  $(B)/tools/rcc/list.o \
  $(B)/tools/rcc/main.o \
  $(B)/tools/rcc/null.o \
  $(B)/tools/rcc/output.o \
  $(B)/tools/rcc/prof.o \
  $(B)/tools/rcc/profio.o \
  $(B)/tools/rcc/simp.o \
  $(B)/tools/rcc/stmt.o \
  $(B)/tools/rcc/string.o \
  $(B)/tools/rcc/sym.o \
  $(B)/tools/rcc/symbolic.o \
  $(B)/tools/rcc/trace.o \
  $(B)/tools/rcc/tree.o \
  $(B)/tools/rcc/types.o

$(DAGCHECK_C): $(LBURG) $(Q3LCCSRCDIR)/dagcheck.md
	$(echo_cmd) "LBURG $(Q3LCCSRCDIR)/dagcheck.md"
	$(Q)$(LBURG) $(Q3LCCSRCDIR)/dagcheck.md $@

$(B)/tools/rcc/dagcheck.o: $(DAGCHECK_C)
	$(DO_TOOLS_CC_DAGCHECK)

$(B)/tools/rcc/%.o: $(Q3LCCSRCDIR)/%.c
	$(DO_TOOLS_CC)

$(Q3RCC): $(Q3RCCOBJ)
	$(echo_cmd) "LD $@"
	$(Q)$(CC) $(TOOLS_LDFLAGS) -o $@ $^

Q3CPPOBJ = \
	$(B)/tools/cpp/cpp.o \
	$(B)/tools/cpp/lex.o \
	$(B)/tools/cpp/nlist.o \
	$(B)/tools/cpp/tokens.o \
	$(B)/tools/cpp/macro.o \
	$(B)/tools/cpp/eval.o \
	$(B)/tools/cpp/include.o \
	$(B)/tools/cpp/hideset.o \
	$(B)/tools/cpp/getopt.o \
	$(B)/tools/cpp/unix.o

$(B)/tools/cpp/%.o: $(Q3CPPDIR)/%.c
	$(DO_TOOLS_CC)

$(Q3CPP): $(Q3CPPOBJ)
	$(echo_cmd) "LD $@"
	$(Q)$(CC) $(TOOLS_LDFLAGS) -o $@ $^

Q3LCCOBJ = \
	$(B)/tools/etc/lcc.o \
	$(B)/tools/etc/bytecode.o

$(B)/tools/etc/%.o: $(Q3LCCETCDIR)/%.c
	$(DO_TOOLS_CC)

$(Q3LCC): $(Q3LCCOBJ) $(Q3RCC) $(Q3CPP)
	$(echo_cmd) "LD $@"
	$(Q)$(CC) $(TOOLS_LDFLAGS) -o $@ $(Q3LCCOBJ)

define DO_Q3LCC
$(echo_cmd) "Q3LCC $<"
$(Q)$(Q3LCC) -o $@ $<
endef

define DO_CGAME_Q3LCC
$(echo_cmd) "CGAME_Q3LCC $<"
$(Q)$(Q3LCC) -DPRODUCT_VERSION=\"$(VERSION)\" -DCGAME -o $@ $<
endef

define DO_GAME_Q3LCC
$(echo_cmd) "GAME_Q3LCC $<"
$(Q)$(Q3LCC) -DPRODUCT_VERSION=\"$(VERSION)\" -DGAME -o $@ $<
endef

define DO_UI_Q3LCC
$(echo_cmd) "UI_Q3LCC $<"
$(Q)$(Q3LCC) -DPRODUCT_VERSION=\"$(VERSION)\" -DUI -o $@ $<
endef


Q3ASMOBJ = \
  $(B)/tools/asm/q3asm.o \
  $(B)/tools/asm/cmdlib.o

$(B)/tools/asm/%.o: $(Q3ASMDIR)/%.c
	$(DO_TOOLS_CC)

$(Q3ASM): $(Q3ASMOBJ)
	$(echo_cmd) "LD $@"
	$(Q)$(CC) $(TOOLS_LDFLAGS) -o $@ $^


#############################################################################
# CLIENT/SERVER
#############################################################################

Q3OBJ = \
  $(B)/client/cl_cgame.o \
  $(B)/client/cl_cin.o \
  $(B)/client/cl_console.o \
  $(B)/client/cl_input.o \
  $(B)/client/cl_keys.o \
  $(B)/client/cl_main.o \
  $(B)/client/cl_net_chan.o \
  $(B)/client/cl_parse.o \
  $(B)/client/cl_scrn.o \
  $(B)/client/cl_ui.o \
  $(B)/client/cl_avi.o \
  \
  $(B)/client/cm_load.o \
  $(B)/client/cm_patch.o \
  $(B)/client/cm_polylib.o \
  $(B)/client/cm_test.o \
  $(B)/client/cm_trace.o \
  \
  $(B)/client/cmd.o \
  $(B)/client/common.o \
  $(B)/client/cvar.o \
  $(B)/client/files.o \
  $(B)/client/md4.o \
  $(B)/client/md5.o \
  $(B)/client/msg.o \
  $(B)/client/net_chan.o \
  $(B)/client/net_ip.o \
  $(B)/client/huffman.o \
  $(B)/client/parse.o \
  \
  $(B)/client/snd_adpcm.o \
  $(B)/client/snd_dma.o \
  $(B)/client/snd_mem.o \
  $(B)/client/snd_mix.o \
  $(B)/client/snd_wavelet.o \
  \
  $(B)/client/snd_main.o \
  $(B)/client/snd_codec.o \
  $(B)/client/snd_codec_wav.o \
  $(B)/client/snd_codec_ogg.o \
  \
  $(B)/client/qal.o \
  $(B)/client/snd_openal.o \
  \
  $(B)/client/cl_curl.o \
  \
  $(B)/client/sv_ccmds.o \
  $(B)/client/sv_client.o \
  $(B)/client/sv_game.o \
  $(B)/client/sv_init.o \
  $(B)/client/sv_main.o \
  $(B)/client/sv_net_chan.o \
  $(B)/client/sv_snapshot.o \
  $(B)/client/sv_world.o \
  \
  $(B)/client/q_math.o \
  $(B)/client/q_shared.o \
  \
  $(B)/client/unzip.o \
  $(B)/client/puff.o \
  $(B)/client/vm.o \
  $(B)/client/vm_interpreted.o \
  \
  $(B)/client/con_log.o \
  $(B)/client/sys_main.o

ifneq ($(USE_TTY_CLIENT),1)
Q3OBJ += \
  $(B)/client/jcapimin.o \
  $(B)/client/jcapistd.o \
  $(B)/client/jchuff.o   \
  $(B)/client/jcinit.o \
  $(B)/client/jccoefct.o  \
  $(B)/client/jccolor.o \
  $(B)/client/jfdctflt.o \
  $(B)/client/jcdctmgr.o \
  $(B)/client/jcphuff.o \
  $(B)/client/jcmainct.o \
  $(B)/client/jcmarker.o \
  $(B)/client/jcmaster.o \
  $(B)/client/jcomapi.o \
  $(B)/client/jcparam.o \
  $(B)/client/jcprepct.o \
  $(B)/client/jcsample.o \
  $(B)/client/jdapimin.o \
  $(B)/client/jdapistd.o \
  $(B)/client/jdatasrc.o \
  $(B)/client/jdcoefct.o \
  $(B)/client/jdcolor.o \
  $(B)/client/jddctmgr.o \
  $(B)/client/jdhuff.o \
  $(B)/client/jdinput.o \
  $(B)/client/jdmainct.o \
  $(B)/client/jdmarker.o \
  $(B)/client/jdmaster.o \
  $(B)/client/jdpostct.o \
  $(B)/client/jdsample.o \
  $(B)/client/jdtrans.o \
  $(B)/client/jerror.o \
  $(B)/client/jidctflt.o \
  $(B)/client/jmemmgr.o \
  $(B)/client/jmemnobs.o \
  $(B)/client/jutils.o \
  \
  $(B)/client/tr_animation.o \
  $(B)/client/tr_backend.o \
  $(B)/client/tr_bloom.o \
  $(B)/client/tr_bsp.o \
  $(B)/client/tr_cmds.o \
  $(B)/client/tr_curve.o \
  $(B)/client/tr_flares.o \
  $(B)/client/tr_font.o \
  $(B)/client/tr_image.o \
  $(B)/client/tr_image_png.o \
  $(B)/client/tr_image_jpg.o \
  $(B)/client/tr_image_bmp.o \
  $(B)/client/tr_image_tga.o \
  $(B)/client/tr_image_pcx.o \
  $(B)/client/tr_init.o \
  $(B)/client/tr_light.o \
  $(B)/client/tr_main.o \
  $(B)/client/tr_marks.o \
  $(B)/client/tr_mesh.o \
  $(B)/client/tr_model.o \
  $(B)/client/tr_noise.o \
  $(B)/client/tr_scene.o \
  $(B)/client/tr_shade.o \
  $(B)/client/tr_shade_calc.o \
  $(B)/client/tr_shader.o \
  $(B)/client/tr_shadows.o \
  $(B)/client/tr_sky.o \
  $(B)/client/tr_surface.o \
  $(B)/client/tr_world.o \
  \
  $(B)/client/sdl_gamma.o \
  $(B)/client/sdl_input.o \
  $(B)/client/sdl_snd.o
else
Q3OBJ += \
  $(B)/client/null_input.o \
  $(B)/client/null_snddma.o \
  $(B)/client/null_renderer.o  
endif

ifeq ($(ARCH),x86)
  Q3OBJ += \
    $(B)/client/snd_mixa.o \
    $(B)/client/matha.o \
    $(B)/client/ftola.o \
    $(B)/client/snapvectora.o
endif

ifeq ($(USE_VOIP),1)
ifeq ($(USE_INTERNAL_SPEEX),1)
Q3OBJ += \
  $(B)/client/bits.o \
  $(B)/client/buffer.o \
  $(B)/client/cb_search.o \
  $(B)/client/exc_10_16_table.o \
  $(B)/client/exc_10_32_table.o \
  $(B)/client/exc_20_32_table.o \
  $(B)/client/exc_5_256_table.o \
  $(B)/client/exc_5_64_table.o \
  $(B)/client/exc_8_128_table.o \
  $(B)/client/fftwrap.o \
  $(B)/client/filterbank.o \
  $(B)/client/filters.o \
  $(B)/client/gain_table.o \
  $(B)/client/gain_table_lbr.o \
  $(B)/client/hexc_10_32_table.o \
  $(B)/client/hexc_table.o \
  $(B)/client/high_lsp_tables.o \
  $(B)/client/jitter.o \
  $(B)/client/kiss_fft.o \
  $(B)/client/kiss_fftr.o \
  $(B)/client/lpc.o \
  $(B)/client/lsp.o \
  $(B)/client/lsp_tables_nb.o \
  $(B)/client/ltp.o \
  $(B)/client/mdf.o \
  $(B)/client/modes.o \
  $(B)/client/modes_wb.o \
  $(B)/client/nb_celp.o \
  $(B)/client/preprocess.o \
  $(B)/client/quant_lsp.o \
  $(B)/client/resample.o \
  $(B)/client/sb_celp.o \
  $(B)/client/smallft.o \
  $(B)/client/speex.o \
  $(B)/client/speex_callbacks.o \
  $(B)/client/speex_header.o \
  $(B)/client/stereo.o \
  $(B)/client/vbr.o \
  $(B)/client/vq.o \
  $(B)/client/window.o
endif
endif


ifeq ($(HAVE_VM_COMPILED),true)
  ifeq ($(ARCH),x86)
    Q3OBJ += $(B)/client/vm_x86.o
  endif
  ifeq ($(ARCH),x86_64)
    Q3OBJ += $(B)/client/vm_x86_64.o $(B)/client/vm_x86_64_assembler.o
  endif
  ifeq ($(ARCH),ppc)
    Q3OBJ += $(B)/client/vm_ppc.o
  endif
endif

ifeq ($(PLATFORM),mingw32)
  Q3OBJ += \
    $(B)/client/win_resource.o \
    $(B)/client/sys_win32.o \
    $(B)/client/con_win32.o
else
  Q3OBJ += \
    $(B)/client/sys_unix.o \
    $(B)/client/con_tty.o
endif

ifeq ($(USE_MUMBLE),1)
  Q3OBJ += \
    $(B)/client/libmumblelink.o
endif

ifneq ($(USE_TTY_CLIENT),1)
  Q3POBJ = \
    $(B)/client/sdl_glimp.o

  Q3POBJ_SMP = \
    $(B)/clientsmp/sdl_glimp.o
endif

$(B)/tremulous.$(ARCH)$(BINEXT): $(Q3OBJ) $(Q3POBJ) $(LIBSDLMAIN)
	$(echo_cmd) "LD $@"
	$(Q)$(CC) -o $@ $(Q3OBJ) $(Q3POBJ) $(CLIENT_LDFLAGS) \
		$(LDFLAGS) $(LIBSDLMAIN)

$(B)/tremulous-smp.$(ARCH)$(BINEXT): $(Q3OBJ) $(Q3POBJ_SMP) $(LIBSDLMAIN)
	$(echo_cmd) "LD $@"
	$(Q)$(CC) -o $@ $(Q3OBJ) $(Q3POBJ_SMP) $(CLIENT_LDFLAGS) \
		$(THREAD_LDFLAGS) $(LDFLAGS) $(LIBSDLMAIN)

ifneq ($(strip $(LIBSDLMAIN)),)
ifneq ($(strip $(LIBSDLMAINSRC)),)
$(LIBSDLMAIN) : $(LIBSDLMAINSRC)
	cp $< $@
	ranlib $@
endif
endif



#############################################################################
# DEDICATED SERVER
#############################################################################

Q3DOBJ = \
  $(B)/ded/sv_client.o \
  $(B)/ded/sv_ccmds.o \
  $(B)/ded/sv_game.o \
  $(B)/ded/sv_init.o \
  $(B)/ded/sv_main.o \
  $(B)/ded/sv_net_chan.o \
  $(B)/ded/sv_snapshot.o \
  $(B)/ded/sv_world.o \
  \
  $(B)/ded/cm_load.o \
  $(B)/ded/cm_patch.o \
  $(B)/ded/cm_polylib.o \
  $(B)/ded/cm_test.o \
  $(B)/ded/cm_trace.o \
  $(B)/ded/cmd.o \
  $(B)/ded/common.o \
  $(B)/ded/cvar.o \
  $(B)/ded/files.o \
  $(B)/ded/md4.o \
  $(B)/ded/msg.o \
  $(B)/ded/net_chan.o \
  $(B)/ded/net_ip.o \
  $(B)/ded/huffman.o \
  $(B)/ded/parse.o \
  \
  $(B)/ded/q_math.o \
  $(B)/ded/q_shared.o \
  \
  $(B)/ded/unzip.o \
  $(B)/ded/vm.o \
  $(B)/ded/vm_interpreted.o \
  \
  $(B)/ded/null_client.o \
  $(B)/ded/null_input.o \
  $(B)/ded/null_snddma.o \
  \
  $(B)/ded/con_log.o \
  $(B)/ded/sys_main.o

ifeq ($(ARCH),x86)
  Q3DOBJ += \
      $(B)/ded/ftola.o \
      $(B)/ded/snapvectora.o \
      $(B)/ded/matha.o
endif

ifeq ($(HAVE_VM_COMPILED),true)
  ifeq ($(ARCH),x86)
    Q3DOBJ += $(B)/ded/vm_x86.o
  endif
  ifeq ($(ARCH),x86_64)
    Q3DOBJ += $(B)/ded/vm_x86_64.o $(B)/client/vm_x86_64_assembler.o
  endif
  ifeq ($(ARCH),ppc)
    Q3DOBJ += $(B)/ded/vm_ppc.o
  endif
endif

ifeq ($(PLATFORM),mingw32)
  Q3DOBJ += \
    $(B)/ded/win_resource.o \
    $(B)/ded/sys_win32.o \
    $(B)/ded/con_win32.o
else
  Q3DOBJ += \
    $(B)/ded/sys_unix.o \
    $(B)/ded/con_tty.o
endif

ifeq ($(USE_PYTHON),1)
  Q3DOBJ += \
  $(B)/ded/py_init.o \
  $(B)/ded/py_main.o
endif

$(B)/tremded.$(ARCH)$(BINEXT): $(Q3DOBJ)
	$(echo_cmd) "LD $@"
	$(Q)$(CC) -o $@ $(Q3DOBJ) $(LDFLAGS)



#############################################################################
## TREMULOUS CGAME
#############################################################################

CGOBJ_ = \
  $(B)/base/cgame/cg_main.o \
  $(B)/base/cgame/bg_misc.o \
  $(B)/base/cgame/bg_pmove.o \
  $(B)/base/cgame/bg_slidemove.o \
  $(B)/base/cgame/bg_lib.o \
  $(B)/base/cgame/bg_alloc.o \
  $(B)/base/cgame/bg_voice.o \
  $(B)/base/cgame/cg_consolecmds.o \
  $(B)/base/cgame/cg_buildable.o \
  $(B)/base/cgame/cg_animation.o \
  $(B)/base/cgame/cg_animmapobj.o \
  $(B)/base/cgame/cg_draw.o \
  $(B)/base/cgame/cg_drawtools.o \
  $(B)/base/cgame/cg_ents.o \
  $(B)/base/cgame/cg_event.o \
  $(B)/base/cgame/cg_marks.o \
  $(B)/base/cgame/cg_players.o \
  $(B)/base/cgame/cg_playerstate.o \
  $(B)/base/cgame/cg_predict.o \
  $(B)/base/cgame/cg_servercmds.o \
  $(B)/base/cgame/cg_snapshot.o \
  $(B)/base/cgame/cg_view.o \
  $(B)/base/cgame/cg_weapons.o \
  $(B)/base/cgame/cg_scanner.o \
  $(B)/base/cgame/cg_attachment.o \
  $(B)/base/cgame/cg_trails.o \
  $(B)/base/cgame/cg_particles.o \
  $(B)/base/cgame/cg_ptr.o \
  $(B)/base/cgame/cg_tutorial.o \
  $(B)/base/ui/ui_shared.o \
  \
  $(B)/base/qcommon/q_math.o \
  $(B)/base/qcommon/q_shared.o \

CGOBJ = $(CGOBJ_) $(B)/base/cgame/cg_syscalls.o
CGVMOBJ = $(CGOBJ_:%.o=%.asm)

$(B)/base/cgame$(ARCH).$(SHLIBEXT): $(CGOBJ)
	$(echo_cmd) "LD $@"
	$(Q)$(CC) $(SHLIBLDFLAGS) -o $@ $(CGOBJ)
	$(Q)$(CC) $(SHLIBLDFLAGS) -o $@ $(CGOBJ) $(GAME_LDFLAGS)

$(B)/base/vm/cgame.qvm: $(CGVMOBJ) $(CGDIR)/cg_syscalls.asm $(Q3ASM)
	$(echo_cmd) "Q3ASM $@"
	$(Q)$(Q3ASM) -o $@ $(CGVMOBJ) $(CGDIR)/cg_syscalls.asm



#############################################################################
## TREMULOUS GAME
#############################################################################

GOBJ_ = \
  $(B)/base/game/g_main.o \
  $(B)/base/game/bg_misc.o \
  $(B)/base/game/bg_pmove.o \
  $(B)/base/game/bg_slidemove.o \
  $(B)/base/game/bg_lib.o \
  $(B)/base/game/bg_alloc.o \
  $(B)/base/game/bg_voice.o \
  $(B)/base/game/g_active.o \
  $(B)/base/game/g_client.o \
  $(B)/base/game/g_cmds.o \
  $(B)/base/game/g_combat.o \
  $(B)/base/game/g_physics.o \
  $(B)/base/game/g_buildable.o \
  $(B)/base/game/g_misc.o \
  $(B)/base/game/g_missile.o \
  $(B)/base/game/g_mover.o \
  $(B)/base/game/g_session.o \
  $(B)/base/game/g_spawn.o \
  $(B)/base/game/g_svcmds.o \
  $(B)/base/game/g_target.o \
  $(B)/base/game/g_team.o \
  $(B)/base/game/g_trigger.o \
  $(B)/base/game/g_utils.o \
  $(B)/base/game/g_maprotation.o \
  $(B)/base/game/g_ptr.o \
  $(B)/base/game/g_weapon.o \
  $(B)/base/game/g_admin.o \
  $(B)/base/game/sc_game.o \
  \
  $(B)/base/qcommon/q_math.o \
  $(B)/base/qcommon/q_shared.o \
  \
  $(B)/base/script_game/sc_datatype.o \
  $(B)/base/script_game/sc_main.o \
  $(B)/base/script_game/sc_c.o \
  $(B)/base/script_game/sc_common.o \
  $(B)/base/script_game/sc_event.o \
  $(B)/base/script_game/sc_utils.o
  

GOBJ = $(GOBJ_) $(B)/base/game/g_syscalls.o
GVMOBJ = $(GOBJ_:%.o=%.asm)

ifeq ($(USE_LUA),1)
  GOBJ +=  \
    $(B)/base/lua/lapi.o \
    $(B)/base/lua/lauxlib.o \
    $(B)/base/lua/lbaselib.o \
    $(B)/base/lua/lcode.o \
    $(B)/base/lua/ldblib.o \
    $(B)/base/lua/ldebug.o \
    $(B)/base/lua/ldo.o \
    $(B)/base/lua/ldump.o \
    $(B)/base/lua/lfunc.o \
    $(B)/base/lua/lgc.o \
    $(B)/base/lua/linit.o \
    $(B)/base/lua/liolib.o \
    $(B)/base/lua/llex.o \
    $(B)/base/lua/lmathlib.o \
    $(B)/base/lua/lmem.o \
    $(B)/base/lua/loadlib.o \
    $(B)/base/lua/lobject.o \
    $(B)/base/lua/lopcodes.o \
    $(B)/base/lua/loslib.o \
    $(B)/base/lua/lparser.o \
    $(B)/base/lua/lstate.o \
    $(B)/base/lua/lstring.o \
    $(B)/base/lua/lstrlib.o \
    $(B)/base/lua/ltable.o \
    $(B)/base/lua/ltablib.o \
    $(B)/base/lua/ltm.o \
    $(B)/base/lua/lua.o \
    $(B)/base/lua/lundump.o \
    $(B)/base/lua/lvm.o \
    $(B)/base/lua/lzio.o \
    $(B)/base/lua/print.o \
    $(B)/base/script_game/sc_lua.o
endif

ifeq ($(USE_PYTHON),1)
  GOBJ += \
    $(B)/base/python/py_entity.o \
    $(B)/base/python/py_function.o \
    $(B)/base/python/py_object.o \
    $(B)/base/script_game/sc_python.o
endif

$(B)/base/game$(ARCH).$(SHLIBEXT): $(GOBJ)
	$(echo_cmd) "LD $@"
	$(Q)$(CC) $(SHLIBLDFLAGS) -o $@ $(GOBJ)

$(B)/base/vm/game.qvm: $(GVMOBJ) $(GDIR)/g_syscalls.asm $(Q3ASM)
	$(echo_cmd) "Q3ASM $@"
	$(Q)$(Q3ASM) -o $@ $(GVMOBJ) $(GDIR)/g_syscalls.asm



#############################################################################
## TREMULOUS UI
#############################################################################

UIOBJ_ = \
  $(B)/base/ui/ui_main.o \
  $(B)/base/ui/ui_atoms.o \
  $(B)/base/ui/ui_shared.o \
  $(B)/base/ui/ui_gameinfo.o \
  \
  $(B)/base/ui/bg_misc.o \
  $(B)/base/ui/bg_lib.o \
  $(B)/base/qcommon/q_math.o \
  $(B)/base/qcommon/q_shared.o

UIOBJ = $(UIOBJ_) $(B)/base/ui/ui_syscalls.o
UIVMOBJ = $(UIOBJ_:%.o=%.asm)

ifeq ($(ENABLE_SCRIPT_UI),1)
  UIOBJ +=  \
    $(B)/base/script_ui/sc_datatype.o \
    $(B)/base/script_ui/sc_main.o \
    $(B)/base/script_ui/sc_c.o \
    $(B)/base/script_ui/sc_common.o \
    $(B)/base/script_ui/sc_event.o \
    $(B)/base/script_ui/sc_utils.o \
    $(B)/base/ui/ui_script.o \
    $(B)/base/game/bg_alloc.o 
  ifeq ($(USE_LUA),1)
  UIOBJ +=  \
    $(B)/base/lua/lapi.o \
    $(B)/base/lua/lauxlib.o \
    $(B)/base/lua/lbaselib.o \
    $(B)/base/lua/lcode.o \
    $(B)/base/lua/ldblib.o \
    $(B)/base/lua/ldebug.o \
    $(B)/base/lua/ldo.o \
    $(B)/base/lua/ldump.o \
    $(B)/base/lua/lfunc.o \
    $(B)/base/lua/lgc.o \
    $(B)/base/lua/linit.o \
    $(B)/base/lua/liolib.o \
    $(B)/base/lua/llex.o \
    $(B)/base/lua/lmathlib.o \
    $(B)/base/lua/lmem.o \
    $(B)/base/lua/loadlib.o \
    $(B)/base/lua/lobject.o \
    $(B)/base/lua/lopcodes.o \
    $(B)/base/lua/loslib.o \
    $(B)/base/lua/lparser.o \
    $(B)/base/lua/lstate.o \
    $(B)/base/lua/lstring.o \
    $(B)/base/lua/lstrlib.o \
    $(B)/base/lua/ltable.o \
    $(B)/base/lua/ltablib.o \
    $(B)/base/lua/ltm.o \
    $(B)/base/lua/lua.o \
    $(B)/base/lua/lundump.o \
    $(B)/base/lua/lvm.o \
    $(B)/base/lua/lzio.o \
    $(B)/base/lua/print.o \
    $(B)/base/script_ui/sc_lua.o
  endif
  ifeq ($(USE_PYTHON),1)
    UIOBJ += \
      $(B)/base/python/py_function.o \
      $(B)/base/python/py_object.o \
      $(B)/base/script_ui/sc_python.o
  endif
endif

$(B)/base/ui$(ARCH).$(SHLIBEXT): $(UIOBJ)
	$(echo_cmd) "LD $@"
	$(Q)$(CC) $(SHLIBLDFLAGS) -o $@ $(UIOBJ)

$(B)/base/vm/ui.qvm: $(UIVMOBJ) $(UIDIR)/ui_syscalls.asm $(Q3ASM)
	$(echo_cmd) "Q3ASM $@"
	$(Q)$(Q3ASM) -o $@ $(UIVMOBJ) $(UIDIR)/ui_syscalls.asm



#############################################################################
## CLIENT/SERVER RULES
#############################################################################

$(B)/client/%.o: $(ASMDIR)/%.s
	$(DO_AS)

$(B)/client/%.o: $(CDIR)/%.c
	$(DO_CC)

$(B)/client/%.o: $(SDIR)/%.c
	$(DO_CC)

$(B)/client/%.o: $(CMDIR)/%.c
	$(DO_CC)

$(B)/client/%.o: $(BLIBDIR)/%.c
	$(DO_BOT_CC)

$(B)/client/%.o: $(JPDIR)/%.c
	$(DO_CC)

$(B)/client/%.o: $(SPEEXDIR)/%.c
	$(DO_CC)

$(B)/client/%.o: $(RDIR)/%.c
	$(DO_CC)

$(B)/client/%.o: $(SDLDIR)/%.c
	$(DO_CC)

$(B)/clientsmp/%.o: $(SDLDIR)/%.c
	$(DO_SMP_CC)

$(B)/client/%.o: $(SYSDIR)/%.c
	$(DO_CC)

$(B)/client/%.o: $(SYSDIR)/%.rc
	$(DO_WINDRES)

$(B)/client/%.o: $(NDIR)/%.c
	$(DO_CC)


$(B)/ded/%.o: $(ASMDIR)/%.s
	$(DO_AS)

$(B)/ded/%.o: $(SDIR)/%.c
	$(DO_DED_CC)

$(B)/ded/%.o: $(CMDIR)/%.c
	$(DO_DED_CC)

$(B)/ded/%.o: $(BLIBDIR)/%.c
	$(DO_BOT_CC)

$(B)/ded/%.o: $(SYSDIR)/%.c
	$(DO_DED_CC)

$(B)/ded/%.o: $(SYSDIR)/%.rc
	$(DO_WINDRES)

$(B)/ded/%.o: $(NDIR)/%.c
	$(DO_DED_CC)
	
$(B)/ded/%.o: $(PYTHONDIR)/%.c
	$(DO_DED_CC)

# Extra dependencies to ensure the SVN version is incorporated
ifeq ($(USE_SVN),1)
  $(B)/client/cl_console.o : .svn/entries
  $(B)/client/common.o : .svn/entries
  $(B)/ded/common.o : .svn/entries
endif

ifeq ($(USE_GIT),1)
  $(B)/client/cl_console.o : .git/svn/.metadata
  $(B)/client/common.o : .git/svn/.metadata
  $(B)/ded/common.o : .git/svn/.metadata
endif

ifeq ($(USE_HG),1)
  $(B)/client/cl_console.o : .hg/dirstate
  $(B)/client/common.o : .hg/dirstate
  $(B)/ded/common.o : .hg/dirstate
endif

#############################################################################
## GAME MODULE RULES
#############################################################################

$(B)/base/cgame/bg_%.o: $(GDIR)/bg_%.c
	$(DO_CGAME_CC)

$(B)/base/cgame/%.o: $(CGDIR)/%.c
	$(DO_CGAME_CC)

$(B)/base/cgame/bg_%.asm: $(GDIR)/bg_%.c $(Q3LCC)
	$(DO_CGAME_Q3LCC)

$(B)/base/cgame/%.asm: $(CGDIR)/%.c $(Q3LCC)
	$(DO_CGAME_Q3LCC)


$(B)/base/game/%.o: $(GDIR)/%.c
	$(DO_GAME_CC)

$(B)/base/game/%.asm: $(GDIR)/%.c $(Q3LCC)
	$(DO_GAME_Q3LCC)


$(B)/base/ui/bg_%.o: $(GDIR)/bg_%.c
	$(DO_UI_CC)

$(B)/base/ui/%.o: $(UIDIR)/%.c
	$(DO_UI_CC)

$(B)/base/ui/bg_%.asm: $(GDIR)/bg_%.c $(Q3LCC)
	$(DO_UI_Q3LCC)

$(B)/base/ui/%.asm: $(UIDIR)/%.c $(Q3LCC)
	$(DO_UI_Q3LCC)

$(B)/base/script_game/%.o: $(SCRIPTDIR)/%.c
	$(DO_GAME_CC)

$(B)/base/script_game/%.asm: $(SCRIPTDIR)/%.c $(Q3LCC)
	$(DO_GAME_Q3LCC)
	
$(B)/base/script_cgame/%.o: $(SCRIPTDIR)/%.c
	$(DO_SHLIB_CC)

$(B)/base/script_cgame/%.asm: $(SCRIPTDIR)/%.c $(Q3LCC)
	$(DO_Q3LCC)
	
$(B)/base/script_ui/%.o: $(SCRIPTDIR)/%.c
	$(DO_UI_CC)

$(B)/base/script_ui/%.asm: $(SCRIPTDIR)/%.c $(Q3LCC)
	$(DO_UI_Q3LCC)

$(B)/base/python/%.o: $(PYTHONDIR)/%.c
	$(DO_SHLIB_CC)

$(B)/base/python/%.asm: $(PYTHONDIR)/%.c $(Q3LCC)
	$(DO_Q3LCC)

$(B)/base/lua/%.o: $(LUADIR)/%.c
	$(DO_SHLIB_CC)

$(B)/base/lua/%.asm: $(LUADIC)/%.c $(Q3LCC)
	$(DO_Q3LCC)

$(B)/base/qcommon/%.o: $(CMDIR)/%.c
	$(DO_SHLIB_CC)

$(B)/base/qcommon/%.asm: $(CMDIR)/%.c $(Q3LCC)
	$(DO_Q3LCC)


#############################################################################
# MISC
#############################################################################

OBJ = $(Q3OBJ) $(Q3POBJ) $(Q3POBJ_SMP) $(Q3DOBJ) \
  $(GOBJ) $(CGOBJ) $(UIOBJ) \
  $(GVMOBJ) $(CGVMOBJ) $(UIVMOBJ)
TOOLSOBJ = $(LBURGOBJ) $(Q3CPPOBJ) $(Q3RCCOBJ) $(Q3LCCOBJ) $(Q3ASMOBJ)


clean: clean-debug clean-release
	@$(MAKE) -C $(MASTERDIR) clean

clean-debug:
	@$(MAKE) clean2 B=$(BD)

clean-release:
	@$(MAKE) clean2 B=$(BR)

clean2:
	@echo "CLEAN $(B)"
	@rm -f $(OBJ)
	@rm -f $(OBJ_D_FILES)
	@rm -f $(TARGETS)

toolsclean: toolsclean-debug toolsclean-release

toolsclean-debug:
	@$(MAKE) toolsclean2 B=$(BD)

toolsclean-release:
	@$(MAKE) toolsclean2 B=$(BR)

toolsclean2:
	@echo "TOOLS_CLEAN $(B)"
	@rm -f $(TOOLSOBJ)
	@rm -f $(TOOLSOBJ_D_FILES)
	@rm -f $(LBURG) $(DAGCHECK_C) $(Q3RCC) $(Q3CPP) $(Q3LCC) $(Q3ASM)

distclean: clean toolsclean
	@rm -rf $(BUILD_DIR)

dist:
	rm -rf tremulous-$(SCM_VERSION)
	svn export . tremulous-$(SCM_VERSION)
	tar --owner=root --group=root --force-local -cjf tremulous-$(SCM_VERSION).tar.bz2 tremulous-$(SCM_VERSION)
	rm -rf tremulous-$(SCM_VERSION)

#############################################################################
# DEPENDENCIES
#############################################################################

OBJ_D_FILES=$(filter %.d,$(OBJ:%.o=%.d))
TOOLSOBJ_D_FILES=$(filter %.d,$(TOOLSOBJ:%.o=%.d))
-include $(OBJ_D_FILES) $(TOOLSOBJ_D_FILES)

.PHONY: all clean clean2 clean-debug clean-release copyfiles \
	debug default dist distclean makedirs \
	release targets \
	toolsclean toolsclean2 toolsclean-debug toolsclean-release
