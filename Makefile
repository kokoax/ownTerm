OBJ = term.o
FLG = -I/usr/X11R6/include -L/usr/X11R6/lib

default: $(OBJ)
	$(CC) $(FLG) -o term $(OBJ) -lX11

term.o: term.c
	$(CC) $(FLG) -o $@ $< -lX11


clean:
	$(RM) $(OBJ)
	$(RM) term

