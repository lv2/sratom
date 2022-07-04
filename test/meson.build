autoship = find_program('autoship', required: false)

unit_tests = [
  'sratom',
]

foreach unit : unit_tests
  test(
    unit,
    executable(
      'test_@0@'.format(unit),
      files('test_@0@.c'.format(unit)),
      c_args: c_suppressions,
      dependencies: [sratom_dep, serd_dep, sord_dep],
      include_directories: include_directories('../src'),
    ),
    suite: 'unit',
  )
endforeach

if autoship.found()
  test('autoship', autoship, args: ['test', sratom_src_root], suite: 'data')
endif
