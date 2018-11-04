all:
	gcc -Wall -std=c11 -DNDEBUG -g -c caltool.c -o caltool.o
	gcc -Wall -std=c11 -DNDEBUG -g -c calutil.c -fpic -o calutil.o
	gcc caltool.o calutil.o -o caltool

	gcc `pkg-config --cflags python3` -c calmodule.c -Wall -std=c11 -DNDEBUG -g -fPIC -o calmodule.o
	gcc -shared calmodule.o calutil.o -o Cal.so

caltool:
	gcc -Wall -std=c11 -DNDEBUG -g -c caltool.c -o caltool.o
	gcc -Wall -std=c11 -DNDEBUG -g -c calutil.c -o calutil.o
	gcc caltool.o calutil.o -o caltool

clean:
	rm -f *.o *.so caltool
