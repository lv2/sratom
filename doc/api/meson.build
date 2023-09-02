# Copyright 2020-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR ISC

c_sratom_rst = custom_target(
  'sratom.rst',
  command: [sphinxygen, '-f', '@INPUT0@', '@OUTDIR@'],
  input: [c_index_xml] + c_rst_files,
  output: 'sratom.rst',
)
