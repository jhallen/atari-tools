
atr : atr.c
	gcc -W -Wall -pedantic -o atr atr.c

clean:
	@rm -f atr *.o
