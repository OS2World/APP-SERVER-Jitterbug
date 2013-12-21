# Jitterbug Makefile
# Done for OS/2 by harald.kipp@egnite.de.
#
# $Log: Makefile,v $
# Revision 1.1  2000/04/25 19:52:09  harald
# First OS/2 release
#
#
PROJ1 = jitterbug
PROJ2 = new_message

COMMON  = util.c smtp.c notify.c lock.c\
          list.c cgi.c loadparm.c vslprintf.c\
          snprintf.c
OS2SPEC = dirent.c pipe.c os2user.c
SOURCE1 = $(PROJ1).c search.c $(COMMON) $(OS2SPEC)
SOURCE2 = $(PROJ2).c $(COMMON) $(OS2SPEC)

OBJECT1 = $(SOURCE1:.c=.obj)
OBJECT2 = $(SOURCE2:.c=.obj)

TARGET1 = $(PROJ1).exe
TARGET2 = $(PROJ2).exe

WFLAGS = /Wall /Wppt- /Wuni- /Wext- /Wtrd- /Wppc-
CFLAGS = /C /DOS2 /Fi /Gd- /Gm /I$(LIBDIR) /Q $(WFLAGS)
LFLAGS = /NOLOGO /NOE /MAP

all: $(TARGET1) $(TARGET2)

.c.obj:
    $(CC) $(CFLAGS) $<

$(TARGET1): $(OBJECT1) Makefile
    $(CC) /Q /B"$(LFLAGS)" -Fe $(TARGET1) $(OBJECT1) tcp32dll.lib so32dll.lib $(PROJ1).def
    MSGBIND $(PROJ1).bnd
    ren $(TARGET1) $(TARGET1)

$(TARGET2): $(OBJECT2) Makefile
    $(CC) /Q /B"$(LFLAGS)" -Fe $(TARGET2) $(OBJECT2) tcp32dll.lib so32dll.lib $(PROJ2).def
    MSGBIND $(PROJ2).bnd
    ren $(TARGET2) $(TARGET2)

clean:
    -del *.obj *.bak *.map csetc.pch 2> nul
    -del $(TARGET1) $(TARGET2) 2> nul
