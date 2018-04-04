set(JPP_GRPC_PROTOS ${CMAKE_CURRENT_BINARY_DIR}/proto/jumanpp_grpc)
FILE(MAKE_DIRECTORY ${JPP_GRPC_PROTOS})

set(JPP_PROTO_SRCS

  ${JPP_GRPC_BASE}/jumanpp/src/core/proto/lattice_dump.proto
  ${JPP_GRPC_BASE}/jumanpp/src/jumandic/proto/juman.proto
  ${JPP_GRPC_BASE}/jumanpp/src/jumandic/proto/jumanpp.proto
  ${JPP_GRPC_BASE}/src/jumandic/jumandic-svc.proto

  )

set(JPP_PYTHON_DIR ${CMAKE_CURRENT_BINARY_DIR}/jumanpp_grpc)
file(MAKE_DIRECTORY ${JPP_PYTHON_DIR})
file(WRITE ${JPP_PYTHON_DIR}/__init__.py "")

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/setup.py
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/setup.py
  COMMAND ${CMAKE_COMMAND} -E copy
  ${CMAKE_CURRENT_SOURCE_DIR}/setup.py
  ${CMAKE_CURRENT_BINARY_DIR}/setup.py
)

set(OUT_PYFILES ${CMAKE_CURRENT_BINARY_DIR}/setup.py)

foreach(FILE ${JPP_PROTO_SRCS})
  get_filename_component(FNAME ${FILE} NAME)
  set(DEST_FILE ${JPP_GRPC_PROTOS}/${FNAME})
  add_custom_command(
    OUTPUT ${DEST_FILE}
    DEPENDS ${FILE}
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/fix_proto_paths.py
      ${FILE} ${DEST_FILE} jumanpp_grpc
  )
  get_filename_component(NAME_ONLY ${FNAME} NAME_WE)
  set(PB_OUTNAME ${JPP_PYTHON_DIR}/${NAME_ONLY}_pb2.py)
  add_custom_command(
    OUTPUT ${PB_OUTNAME}
    COMMAND
    python3 -m grpc_tools.protoc --proto_path=${CMAKE_CURRENT_BINARY_DIR}/proto
    --python_out=${CMAKE_CURRENT_BINARY_DIR} --grpc_python_out=${CMAKE_CURRENT_BINARY_DIR}
    ${DEST_FILE}
    DEPENDS ${DEST_FILE}
  )
  list(APPEND OUT_PYFILES ${PB_OUTNAME})
endforeach(FILE)

add_custom_target(python DEPENDS ${OUT_PYFILES})