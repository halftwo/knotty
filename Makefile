

dirs = 3rd_party xslib dlog xic x4fcgi

all:
	for x in $(dirs); do (cd $$x; make); done

clean:
	for x in $(dirs); do (cd $$x; make clean); done

