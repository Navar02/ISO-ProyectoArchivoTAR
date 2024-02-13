./targ10 seq.dat test.tar
tar -tvf test.tar
./targ10 pruebas test.tar
tar -tvf test.tar
./targ10 -e pruebas/f1.dat test.tar
./targ10 -e pruebas/carpeta test.tar
./targ10 -e pruebas/enlace test.tar
rm test.tar
