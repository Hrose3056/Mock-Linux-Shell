CC=g++
EXE=msh379
OBJ = msh379.o
CFLAGS = -std=c++11
FILES_TO_TAR = makefile msh379.cpp projectReport.pdf

%.o: %.cpp
	$(CC) -c -o $@ $< $(CFLAGS)

$(EXE): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean tar
clean:
	rm -f $(OBJ) $(EXE)
tar:
	tar -cvf CMPUT379-Ass1-Hdesmara.tar $(FILES_TO_TAR)