objects = thread.o

objpath = ../object

thread: thread.c
	gcc -g -c $<
	cp $(objects) $(objpath)


clean:
	rm -rf $(objects)
	rm -rf $(objpath)/$(objects)

