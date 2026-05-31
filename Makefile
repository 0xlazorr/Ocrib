# Ocrib Project Makefile

CC = gcc
CFLAGS = -Wall -O2
LDFLAGS = -lcurl

AGENT_SRC = agent.c
AGENT_OUT = remote_agent

BROKER_SRC = broker.py
SETUP_SRC = setup_db.py

.PHONY: all agent broker setup clean help

all: agent

agent: $(AGENT_SRC)
	$(CC) $(CFLAGS) $(AGENT_SRC) $(LDFLAGS) -o $(AGENT_OUT)

setup:
	python3 $(SETUP_SRC)

broker: setup
	python3 $(BROKER_SRC)

docker-up:
	docker-compose up --build -d

docker-down:
	docker-compose down

clean:
	rm -f $(AGENT_OUT) auth.key platform.db

help:
	@echo "Ocrib Build & Run Utility"
	@echo "-------------------------"
	@echo "make agent       - Build the hardware agent"
	@echo "make setup       - Initialize the database"
	@echo "make broker      - Run the cloud broker locally"
	@echo "make docker-up   - Start the broker in Docker"
	@echo "make docker-down - Stop the broker in Docker"
	@echo "make clean       - Remove built files and DB"
