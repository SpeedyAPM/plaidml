pml_cc_library(
  NAME linalg_to_pxa
  HDRS
    passes.h
  SRCS
    linalg_to_pxa.cc
    tile_to_pxa_special_patterns.cc
    special_utils.cc
    #pooling.cc
    reshape.cc
    utils.cc
  DEPS
    ::passes-gen
    pmlc::dialect::pxa::analysis
    pmlc::dialect::pxa::ir
    pmlc::dialect::stdx::ir
    pmlc::dialect::tile::ir
    pmlc::util
)

pml_tblgen_library(
  NAME passes-gen
  TD_FILE passes.td
  OUTS
    -gen-pass-decls passes.h.inc
)
