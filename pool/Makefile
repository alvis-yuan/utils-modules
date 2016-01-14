objects = policy.o pool.o caching.o

objpath = ../object

CCFLAGS = -g
libs = libpool.a

pool: policy.c pool.c caching.c
	gcc -c $(CCFLAGS) $^
	ar cm $(libs) $(objects)
	cp $(libs) $(objpath)
	rm -rf $(objects)



clean:
	rm -rf $(objects) $(libs)
	rm -rf $(objpath)/$(libs)
