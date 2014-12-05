#/************************************************************************
# *                                                                      *
# *      TUsh - The Telnet User's Shell          Simon Marsh 1992        *
# *                                                                      *
# ************************************************************************/
#
#
# This is the directory you want TUsh installed in
# Don't leave this blank. If you want the program to go into the
# current directory just put .
#
# Note that this path will get searched for the help files, so it is
# wise to include the complete path, to prevent searching in a
# directory relative to where tush is called from.

DEST 	= .

# This is what you want it to be called
# You should really leave this as tush.

PROGRAM = tush

# If you dont have gcc, change this to cc.
#
# Some Suns seem to have problems doing the terminal ioctls when using gcc.
# If you compile and it gives a 'terminal ioctl error' , then try with
# cc instead.

#CC	 = cc
CC      = gcc

# if you want any fancy compiler flags put them here
# You might want to put -O for optimisation instead of -g.

CFLAGS  = -g
  
# You may wish to add DEFS to configure TUsh for your local
# system.
#
# SYSV compatability isn't really implemented. Basically all that
# happens when this flag is set, is that a few terminal ioctls and
# includes get changed. 
# 
#	    System V users (versions less than release 4) should
#	    include this line in the definitions.
#
SYSTEM = -DSYSV
#
#           BSD users (eg SunOS 4.xx) should insure that system is
#	    empty. BSD is the default flavor of UNIX.
#
#SYSTEM = -DBSD

#
# define this if you have problems compiling the
# 'check_malloc' function or your compiler cannot find 'mallopt'
#
MALL = -DNO_FANCY_MALLOC
#MALL =

# The default access setting is for everyone to be able to use TUsh
# change this to u for a private copy. 

CHMOD_FLAGS = a

#######################################################################

# You shouldn't need to alter anything below here.

DEFS = $(SYSTEM) $(MALL) -DHELPPATH='"$(DEST)/tush.doc"'
LIBS    = -lncurses 


HDRS          = clist.h \
		config.h

OBJS          = alias.o \
		command.o \
		main.o \
		socket.o \
		vscreen.o

SRCS          = alias.c \
		command.c \
		main.c \
		socket.c \
		vscreen.c

all:            $(PROGRAM)

$(PROGRAM):     $(OBJS)
	$(CC) $(CFLAGS) -o $(PROGRAM) $(DEFS) $(OBJS) $(LIBS)

install:        $(PROGRAM)
	strip $(PROGRAM)
	cp tush.doc $(DEST)
	cp $(PROGRAM) $(DEST)
	chmod $(CHMOD_FLAGS)+r $(DEST)/tush.doc
	chmod $(CHMOD_FLAGS)+rx $(DEST)/$(PROGRAM)


program:        $(PROGRAM)

depend:
	makedepend $(SRCS)

.c.o:
	$(CC) $(CFLAGS) $(DEFS) -c $<


###
alias.o: config.h Makefile
command.o: config.h clist.h Makefile
main.o: config.h Makefile
socket.o: config.h Makefile
vscreen.o: config.h Makefile
