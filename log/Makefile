objects = log.o

objpath = ../object

CCFLAGS = -g 

log: log.c
	gcc -c $<
	cp $(objects) $(objpath)


clean:
	rm -rf $(objects)
	rm -rf $(objpath)/$(objects)
