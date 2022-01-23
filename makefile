CC=g++
EXE=msh379
DEPS =
OBJ = msh379.o
CFLAGS = -Wall -std=c++11

%.o: %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(EXE): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIB)

.PHONY: clean
clean:
	rm -f $(OBJ) $(EXE)