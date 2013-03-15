CFLAGS = `pkg-config --cflags opencv`
LIBS = `pkg-config --libs opencv`

vec_quant : vec_quant.c 
	gcc $< $(CFLAGS) $(LIBS) -o $@
