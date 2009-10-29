CC = gcc
CFLAGS = -O2 -Wall -Wextra -Wno-unused-parameter -pedantic -pipe
LIBS = -lcurses
OBJDIR = .build
OBJECTS = main.o
OBJECTS :=  $(addprefix ${OBJDIR}/,${OBJECTS})

vindication: $(OBJECTS)
	$(CC) $(CFLAGS) $(LIBS) $(OBJECTS) -o vindication


${OBJDIR}/%.o : %.c
	@if [ ! -d $(OBJDIR) ]; then mkdir $(OBJDIR); fi #create directory if it doesn't exist
	$(CC) $(CFLAGS) -c -o $@ $<

install: vindication
	cp ./vindication /usr/bin/

clean:
	rm -f $(OBJECTS) vindication
