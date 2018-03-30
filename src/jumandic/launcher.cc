#include <iostream>
#include "jumandic-svc.grpc.pb.h"
#include <grpc++/grpc++.h>
#include "jumandic/shared/jumandic_env.h"
#include "jumandic/shared/jumanpp_args.h"
#include "util/bounded_queue.h"
#include "core/proto/lattice_dump_output.h"
#include <thread>
#include <stack>
#include "interfaces.h"
#include "calls_impl.h"
#include "args.h"

using namespace jumanpp::grpc;

struct JumanppGrpcArgs {
  std::string configPath;
  int port = -1;
  int nthreads = 1;

  static bool ParseArgs(JumanppGrpcArgs* result, int argc, const char** argv) {
    args::ArgumentParser parser{"gRPC wrapper for Juman++"};
    args::ValueFlag<std::string> configPath{parser, "PATH", "Config path", {"config", "conf", 'c'}};
    args::ValueFlag<int> port{parser, "PORT", "Port to listen. -1 for automatic (will be printed to stdout).", {"port"}};
    args::ValueFlag<int> nthreads{parser, "NUM", "Number of computation threads", {"threads", 't'}};
    args::HelpFlag help{parser, "HELP", "Prints this message", {'h', "help"}};

    try {
      if (!parser.ParseCLI(argc, argv)) {
        return false;
      }
    } catch (args::Help& e) {
      std::cerr << parser;
      exit(1);
    } catch (std::exception& e) {
      std::cerr << e.what();
      exit(1);
    }

    if (configPath) {
      result->configPath = configPath.Get();
    }

    if (port) {
      result->port = port.Get();
    }

    if (nthreads) {
      result->nthreads = nthreads.Get();
    }

    return true;
  }
};

int main(int argc, char const *argv[]) {
  JumanppGrpcArgs args;
  if (!JumanppGrpcArgs::ParseArgs(&args, argc, argv)) {
    std::cerr << "Failed to parse args";
    exit(1);
  }

  JumanppGrpcEnv2 env;
  auto s = env.loadConfig(args.configPath);
  if (!s) {
    std::cerr << s;
    exit(1);
  }

  ::grpc::ServerBuilder bldr;
  std::string address = "[::]:";
  if (args.port > 0) {    
    address += std::to_string(args.port);
  } else {
    address += "0";
  }

  int boundPort = -1;
  bldr.AddListeningPort(address, ::grpc::InsecureServerCredentials(), &boundPort);

  env.registerService(&bldr);
  auto server = bldr.BuildAndStart();
  env.callImpl<DefaultConfigCall>();
  env.callImpl<JumanUnaryCall>();
  env.callImpl<JumanStreamCall>();
  env.callImpl<TopNUnaryCall>();
  env.callImpl<TopNStreamCall>();
  env.callImpl<LatticeDumpStreamImpl>();
  env.callImpl<LatticeDumpUnaryCall>();
  env.callImpl<FullLatticeDumpUnaryCall>();
  env.callImpl<LatticeDumpStreamFullImpl>();

  if (args.port <= 0) {
    std::cout << boundPort << "\n"
              << std::flush;
  }

  env.start(args.nthreads);

  return 0;
}