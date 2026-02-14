CC = cc
CFLAGS = -O2 -Wall
SRC = src/main.c
BIN = trs
CONFIG_DIR = $(HOME)/.config/trs
DICT_FILE = example.dict
USR_LOCAL_BIN_PATH = /usr/local/bin

.PHONY: all clean install uninstall

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $(BIN) $(SRC)

install: $(BIN)
	@echo "Installing binary to $(USR_LOCAL_BIN_PATH)/ ..."
	sudo cp $(BIN) $(USR_LOCAL_BIN_PATH)/
	@echo "Creating $(CONFIG_DIR)/ ..."
	mkdir -p $(CONFIG_DIR)
	@echo "Copying example config ..."
	cp -n $(DICT_FILE) $(CONFIG_DIR)/
	@echo "Done"

clean:
	rm -f $(BIN)

uninstall:
	rm -f $(USR_LOCAL_BIN_PATH)/$(BIN)
	@echo "Uninstalled $(BIN), dictionaries and config directory can be removed manually at $(CONFIG_DIR)"
