default:
	cd makeprocs; make
	cd reaction; make
	cd inject; make

clean:
	cd makeprocs; make clean
	cd reaction; make clean
	cd inject; make clean

run:
	cd ../../bin; dlxsim -x os.dlx.obj -a -u makeprocs.dlx.obj 1 1; ee469_fixterminal
