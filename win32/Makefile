NSIS_PATH=`cygpath -u "c:\\Program Files\\NSIS"`

all:
	PATH="$$PATH:$(NSIS_PATH)" makensis.exe sessionsave.nsi

clean:
	rm -f sessionsave.exe sessionsave.dll
