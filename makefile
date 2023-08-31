POSIX1993 = -D_POSIX_C_SOURCE=199309L
POSIX2008 = -D_POSIX_C_SOURCE=200809L
CC        = gcc
OPT       = -Wall -pedantic -ansi
REENT     = -D_REENTRANT
THREAD    = -lpthread
CURS      = -lncurses
TARGET    = ntc
CFLAGS    = -Iheaders
HDR       = headers
OBJ       = ntc.o ntc_ht.o ntc_queue.o ntc_tools.o ntc_dyn.o ntc_net.o ntc_terminal.o ntc_reports.o ntc_db.o

all : $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(CURS) $(THREAD)
	rm -f *.o


ntc_db.o: ntc_db.c $(addprefix $(HDR)/, ntc.h) $(addprefix $(HDR)/, ntc_db.h) $(addprefix $(HDR)/, ntc_tools.h)
	$(CC) $(OPT) $(CFLAGS) -c -std=c99 $(POSIX2008) ntc_db.c

ntc_reports.o: ntc_reports.c $(addprefix $(HDR)/, ntc.h) $(addprefix $(HDR)/, ntc_reports.h) $(addprefix $(HDR)/, ntc_tools.h) $(addprefix $(HDR)/, ntc_net.h)
	$(CC) $(OPT) $(CFLAGS) $(POSIX1993) -c -std=c99 ntc_reports.c 

ntc_terminal.o: ntc_terminal.c $(addprefix $(HDR)/, ntc.h) $(addprefix $(HDR)/, ntc_terminal.h) $(addprefix $(HDR)/, ntc_net.h) $(addprefix $(HDR)/, ntc_tools.h)
	$(CC) $(OPT) $(CFLAGS) $(POSIX1993) -c -std=c99 ntc_terminal.c 

ntc_net.o: ntc_net.c $(addprefix $(HDR)/, ntc.h) $(addprefix $(HDR)/, ntc_net.h) $(addprefix $(HDR)/, ntc_tools.h)
	$(CC) $(OPT) $(CFLAGS) -c -std=c99 ntc_net.c 

ntc_dyn.o: ntc_dyn.c $(addprefix $(HDR)/, ntc.h) $(addprefix $(HDR)/, ntc_dyn.h)
	$(CC) $(OPT) $(CFLAGS) -c -std=c99 ntc_dyn.c 

ntc_tools.o: ntc_tools.c $(addprefix $(HDR)/, ntc.h) $(addprefix $(HDR)/, ntc_tools.h)
	$(CC) $(OPT) $(CFLAGS) -c -std=c99 ntc_tools.c 

ntc_queue.o: ntc_queue.c $(addprefix $(HDR)/, ntc.h) $(addprefix $(HDR)/, ntc_queue.h) $(addprefix $(HDR)/, ntc_tools.h)
	$(CC) $(OPT) $(CFLAGS) -c -std=c99 ntc_queue.c 

ntc_ht.o: ntc_ht.c $(addprefix $(HDR)/, ntc.h) $(addprefix $(HDR)/, ntc_ht.h) $(addprefix $(HDR)/, ntc_tools.h)
	$(CC) $(OPT) $(CFLAGS) -c -std=c99 ntc_ht.c 

ntc.o: ntc.c $(addprefix $(HDR)/, ntc.h) $(addprefix $(HDR)/, ntc_ht.h) $(addprefix $(HDR)/, ntc_tools.h) $(addprefix $(HDR)/, ntc_queue.h) $(addprefix $(HDR)/, ntc_dyn.h) $(addprefix $(HDR)/, ntc_net.h) $(addprefix $(HDR)/, ntc_terminal.h) $(addprefix $(HDR)/, ntc_reports.h) $(addprefix $(HDR)/, ntc_db.h)
	$(CC) $(OPT) $(CFLAGS) $(POSIX1993) $(REENT) -c -std=c99 ntc.c

.PHONY:
	clean
clean:
	rm -f *.o
	rm -f $(TARGET)
	@echo ---------------------- Clean done ----------------------
	

