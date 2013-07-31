OBJECTS = $(patsubst src/%.c,obj/%.o,$(wildcard src/*.c))

lib: obj/libjq.a

obj/libjq.a: $(OBJECTS)
	ar rcs $@ $(OBJECTS)

$(OBJECTS): | obj

obj:
	mkdir -p $@

obj/%.o: src/%.c
	gcc -o $@ -c $<

test:
	cd tests/ && perl ../test-kit/runtests.pl *.c

install: test

clean:
	rm -Rf obj/ tmp/
