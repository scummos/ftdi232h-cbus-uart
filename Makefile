TOPDIR  := $(shell cd ..; pwd)
include $(TOPDIR)/Rules.make

APP = uart2pts 

all: $(APP)

$(APP): main.c	
	$(CC) main.c -o $(APP) $(CFLAGS)	
	
clean:
	rm -f *.o ; rm $(APP)
