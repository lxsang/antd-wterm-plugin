include ../../var.mk
PL_NAME=wterm
PLUGINS=$(PL_NAME).$(EXT)
APP_DIR=$(BUILDIRD)/htdocs/

OBJS = 		$(PLUGINS_BASE)/plugin.o 

PLUGINSDEP = 	$(OBJS) \
				wterm.o
		
PLUGINLIBS = libantd.$(EXT) 
				
PCFLAGS=-W -Wall -g -D DEBUG  $(PPF_FLAG) 
 
main:  $(PLUGINSDEP)  $(PLUGINS)  #lib

%.o: %.c
		$(CC) $(PCFLAGS) -fPIC $(INCFLAG) -c  $< -o $@

%.$(EXT):
		-ln -s $(PBUILDIRD)/libantd.$(EXT) .
		$(CC) $(PCFLAGS) $(PLUGINSDEP) $(PLUGINLIBS) -shared -o $(PBUILDIRD)/$(basename $@).$(EXT) 


clean: #libclean
		-rm -f *.o  *.$(EXT) $(PBUILDIRD)/$(PLUGINS) 
		-rm $(PLUGINS_BASE)/plugin.o


.PRECIOUS: %.o
.PHONY: lib clean
full: clean main



