objects = policy.o pool.o caching.o
target = test

logdir = ../log

CCFLAGS = -g
LDFLAGS = -lpthread
libs = libpool.a

test: pool log
	gcc $(CCFLAGS) -o $@ test.c $(libs) log.o $(LDFLAGS)

pool: policy.c pool.c caching.c
	gcc -c $(CCFLAGS) $^
	ar cm $(libs) $(objects)
	rm -rf $(objects)


log:
	make -C $(logdir) $@
	cp $(logdir)/log.o .


clean:
	make -C $(logdir) $@
	rm -rf $(objects) $(target) $(libs)
