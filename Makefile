DIR_SERVICE := $(CURDIR)/SomeServer.service
DIR_DAEMON := $(CURDIR)/server5_daemon

all:
	sudo gcc -o server5_daemon serverTCPandUDP.c -lsystemd
	sudo cp ${DIR_SERVICE} /etc/systemd/system
	sudo mv ${DIR_DAEMON} /usr/bin/
	sudo systemctl daemon-reload
