# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

# def options(opt):
#     pass

# def configure(conf):
#     conf.check_nonfatal(header_name='stdint.h', define_name='HAVE_STDINT_H')

def build(bld):
    module = bld.create_ns3_module('mcih', ['core', 'wifi', 'internet', 'applications'])
    module.source = [
        'model/mcih.cc',
        'model/mcih-utility.cc',
        'model/mcih-packet.cc',
        'model/mcih-routing-table.cc',
        'model/mcih-neighbor.cc',
        'helper/mcih-helper.cc',
        ]

    module_test = bld.create_ns3_module_test_library('mcih')
    module_test.source = [
        'test/mcih-test-suite.cc',
        ]

    headers = bld(features='ns3header')
    headers.module = 'mcih'
    headers.source = [
        'model/mcih.h',
        'model/mcih-utility.h',
        'model/mcih-packet.h',
        'model/mcih-routing-table.h',
        'model/mcih-neighbor.h',
        'helper/mcih-helper.h',
        ]

    if bld.env.ENABLE_EXAMPLES:
        bld.recurse('examples')

    # bld.ns3_python_bindings()

