# QuickStart
Pull open MPI docker image

````bash
docker pull mfisherman/openmpi:latest
````
Start and exec into docker image 
```bash 
docker run --rm -it -v ${pwd}:/project mfisherman/openmpi
```

Build with openMPI 
```bash
make
```

Run NQuen Program 
```bash
mpirun bin/brutalNQueen -np <core_numbers> 
```


