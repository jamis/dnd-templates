all: libtemplates.a

clean:
	rm -f src/*.o src/*.a

libtemplates.a: src/templates.o src/extensions.o
	ar -rc src/libtemplates.a src/templates.o src/extensions.o
	ranlib src/libtemplates.a

src/templates.o: src/templates.c include/*.h
	gcc -c -Iinclude -o src/templates.o src/templates.c

src/extensions.o: src/extensions.c include/extensions.h
	gcc -c -Iinclude -o src/extensions.o src/extensions.c
