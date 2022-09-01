# Copyright 2021-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: CC0-1.0 OR ISC

c_sratom_rst = custom_target(
  'sratom.rst',
  command: [dox_to_sphinx, '-f', '@INPUT0@', '@OUTDIR@'],
  input: [c_index_xml] + c_rst_files,
  output: 'sratom.rst',
)
