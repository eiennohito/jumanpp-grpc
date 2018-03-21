//
// Created by Arseny Tolmachev on 2018/03/09.
//

#ifndef JUMANPP_GRPC_INTERFACES_H
#define JUMANPP_GRPC_INTERFACES_H

namespace jumanpp {
namespace grpc {

struct CallImpl {
  virtual ~CallImpl() = default;
  virtual void Handle() = 0;
};

} // namespace grpc
} // namespace jumanpp

#endif //JUMANPP_GRPC_INTERFACES_H
