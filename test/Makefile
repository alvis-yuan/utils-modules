
object = test
objdir = ../object


test: test.c
	gcc -g -o $@  $< $(objdir)/log.o $(objdir)/libpool.a $(objdir)/thread.o -lpthread

clean:
	rm -rf $(object)
