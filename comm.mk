%.o: %.c
	gcc -std=c99 -Wall -c $< -o $@

BIN_FILES += 

clear:
	-rm *.o
	-rm $(BIN_FILES)

.PHONY: clear

