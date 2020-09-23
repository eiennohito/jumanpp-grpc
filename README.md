# gRPC server for Juman++

This is a project, making Juman++ easy to use from other languages,
especially if you already are using gRPC.

## Quickstart

You need to have
reasonably fresh C++ versions of Protobuf and gRPC installed.

First, build the server binary:

```bash
$ git clone --recursive git@github.com:eiennohito/jumanpp-grpc.git
$ cd jumanpp-grpc
$ mkdir build
$ cd build
$ cmake ..
$ make -j
```

Second, download a Juman++ model. You can find the
latest one in a package on Juman++ (V2)
[Releases](https://github.com/ku-nlp/jumanpp/releases) page.
We will assume that you have Juman++ installed
to the default directory. 


Now launch the server using

```shell
./src/jumandic/jumanpp-jumandic-grpc \
    --config=/usr/local/libexec/jumanpp/jumandic.config \
    --port=51231 \
    --threads=2
```

### Python (3) Client

You can use 
[our python example](python/examples/separator.py) to
check if the server is working.
We assume that you are still in the `build` folder.

First, generate gRPC python files with:
```shell
$ cmake .. -D JPP_GRPC_PYTHON=ON
$ pip3 install grpcio-tools
$ make python
$ cd python
$ pip3 install -e . 
``` 

You can now run the client with:
```shell
$ python3 ../python/examples/separator.py localhost:51231
Sentece: すもももももももものうち
すもも_名詞 も_助詞 もも_名詞 も_助詞 もも_名詞 の_助詞 うち_名詞
Sentece:  
```

